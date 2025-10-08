#include <pep/storagefacility/PageStore.hpp>
#include <pep/storagefacility/S3Client.hpp>
#include <pep/storagefacility/S3Credentials.PropertySerializer.hpp>

#include <cassert>
#include <filesystem>

#include <pep/utils/Log.hpp>

#include <rxcpp/operators/rx-switch_if_empty.hpp>
#include <rxcpp/operators/rx-merge.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>

#include <pep/utils/Sha.hpp>
#include <pep/async/RxLazy.hpp>
#include <pep/async/RxButFirst.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/File.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>

#include <prometheus/gauge.h>
#include <prometheus/registry.h>

static const std::string LOG_TAG ("PageStore");

namespace pep
{


  class S3PageStore
    : public PageStore,
      public std::enable_shared_from_this<S3PageStore>
  {
  public:

    messaging::MessageSequence
      get(const std::string& path) override;

    rxcpp::observable<std::string> put(
        const std::string& path,
        std::vector<std::shared_ptr<std::string>> page_parts) override;

    static std::shared_ptr<S3PageStore> Create(
        std::shared_ptr<boost::asio::io_context> io_context,
        std::shared_ptr<prometheus::Registry> metrics_registry,
        std::shared_ptr<Configuration> config);

    // pubic constructor for the sake of std::make_shared
    S3PageStore(
      const s3::Client::Parameters& s3params,
        unsigned int conn_count,
        const std::string& write_bucket,
        const std::vector<std::string>& buckets,
        std::shared_ptr<prometheus::Registry> metrics_registry);

    ~S3PageStore() override;

  private:

    std::vector<std::shared_ptr<s3::Client>> clients;

    // keeps track of the number of open requests per connection
    std::shared_ptr<std::vector<unsigned int>> open_requests_counts;

    std::string write_bucket;
    std::vector<std::string> buckets;

    // gets the index of (one of) the quietest connections
    size_t getQuietestConn();

    // gets page from specified bucket
    messaging::MessageSequence get(const std::string& path,
        const std::string bucket);


    struct Metrics {
      prometheus::Gauge& active_requests;
      prometheus::Gauge& pending_requests;
      prometheus::Gauge& pending_pages_size;

      Metrics(std::shared_ptr<prometheus::Registry> registry)
        : active_requests(prometheus::BuildGauge()
            .Name("pep_sf_s3_active_requests")
            .Help("number of active requests to S3")
            .Register(*registry)
            .Add({})),
          pending_requests(prometheus::BuildGauge()
            .Name("pep_sf_s3_pending_requests")
            .Help("number of requests to S3 that will be sent soon")
            .Register(*registry)
            .Add({})),
          pending_pages_size(prometheus::BuildGauge()
            .Name("pep_sf_s3_pending_pages_size")
            .Help("total size of the pages pending to be sent to S3")
            .Register(*registry)
            .Add({})) { }
    };

    std::optional<Metrics> metrics;
  };


  std::shared_ptr<S3PageStore> S3PageStore::Create(
      std::shared_ptr<boost::asio::io_context> io_context,
      std::shared_ptr<prometheus::Registry> metrics_registry,
      std::shared_ptr<Configuration> config)
  {
    s3::Client::Parameters s3params = {
      config->get<EndPoint>("EndPoint"),
      config->get<s3::Credentials>("Credentials"),
      io_context,
      config->get<std::optional<std::filesystem::path>>("Ca-Cert-Path")
    };

    unsigned int conn_count = config->get<unsigned int>("Connections", 5);
    std::string write_bucket = config->get<std::string>("Write-To-Bucket");

    std::vector<std::string> buckets;

    for (const std::string& bucket
        : config->get<std::vector<std::string>>("Read-From-Buckets")) {
      buckets.push_back(bucket);
    }

    if (buckets.begin() == buckets.end())
      throw std::runtime_error("S3PageStore configuration error: "
          "no buckets to read from!");

    if (std::find(buckets.begin(), buckets.end(), write_bucket)
          == buckets.end()) {
      throw std::runtime_error("S3PageStore configuration error: "
          "writing to a bucket we're not reading from!");
    }

    return std::make_shared<S3PageStore>(
        s3params, conn_count, write_bucket, buckets, metrics_registry);
  }



  S3PageStore::S3PageStore(
      const s3::Client::Parameters& s3params,
      unsigned int conn_count,
      const std::string& write_bucket,
      const std::vector<std::string>& buckets,
      std::shared_ptr<prometheus::Registry> metrics_registry)

    : PageStore(),
      clients(),
      open_requests_counts(std::make_shared<std::vector<unsigned int>>()),
      write_bucket(write_bucket),
      buckets(buckets),
      metrics(metrics_registry ? std::make_optional<Metrics>(metrics_registry)
                               : std::nullopt)
  {
    for (auto i = 0U; i < conn_count; i++) {
      auto client = s3::Client::Create(s3params);
      client->start();
      this->clients.push_back(client);
      this->open_requests_counts->push_back(0);
    }
  }

  S3PageStore::~S3PageStore() {
    for (auto client : clients) {
      client->shutdown();
    }
#if BUILD_HAS_DEBUG_FLAVOR()
    for (unsigned int count : *(this->open_requests_counts))
      assert(count == 0);
    // The "count" variable is only used in an assertion, making it
    // unused in non-debug builds.
    //
    // Why not a LOG(LOG_TAG, error) here instead of an assert?
    //
    // Either there's a bug in the open requests counting code---which we don't
    // want to be buried in the logs---or some request is actually still active,
    // which will cause an inexplicable segfault when it'll try to decrement
    // the deleted open_requests_counts[idx] upon completion.
#endif
  }


  size_t S3PageStore::getQuietestConn() {
    size_t candidate = 0;

    unsigned int candidate_count = this->open_requests_counts->at(candidate);

    for (size_t contender=1;
        contender < this->open_requests_counts->size(); contender++) {

      unsigned int contender_count = this->open_requests_counts->at(contender);

      if (contender_count < candidate_count) {
        candidate = contender;
        candidate_count = contender_count;
      }
    }

    return candidate;
  }


  messaging::MessageSequence
    S3PageStore::get(const std::string& path) {

    // If the object is not in the first bucket, it might be in one of the
    // next buckets, so the idea is to first call
    //
    //   this->get(path, this->buckets[0]),
    //
    // and if this yields no results, call
    //
    //   this->get(path, this->buckets[1]),
    //
    // and so on.  Since we can't decide here whether these observables
    // will be empty, we employ  obs1.switch_if_empty(obs2),  which returns
    // obs2 when obs1 is empty.
    //
    // We can't use
    //
    //   this->get(path, buckets[0]).switch_if_empty(
    //     this->get(path, buckets[1]).switch_if_empty(
    //       this->get(path, buckets[2]).switch_if_empty(
    //         ...
    //
    // since calling this->get(...) prepares a request to S3, which
    // we'd like to prevent when the request turns out to be unnecesary,
    // for the sake of efficiency, and because an unconsumed request
    // might cause errors and memory leaks.
    //
    // That's why we use RxLazy([](){ this->get(path, bucket[1]) }),
    // which calls the lambda function only when needed.
    messaging::MessageSequence result
      = rxcpp::observable<>::empty<std::shared_ptr<std::string>>();

    for (const std::string& bucket : this->buckets) {

      result = result.switch_if_empty(RxLazy<std::shared_ptr<std::string>>(

        [self=this->shared_from_this(),bucket,path]()
            -> messaging::MessageSequence
        {
          return self->get(path, bucket);
        }

      ));
    }

    return result;
  }


  messaging::MessageSequence S3PageStore::get(
      const std::string& _path, std::string bucket)
  {
    std::string path = _path; // don't leave the reference dangling
    auto self = this->shared_from_this();

    if (this->metrics)
      this->metrics->pending_requests.Increment();

    // We should decrement the pending_requests counter not only when the
    // observable we will in a moment create is subscribed to,
    // but also when it becomes clear it will never be subscribed to
    // on account of it being destroyed.
    // We achieve this using a 'defer guard';  when post_pending is
    // destroyed (or manually triggered) pending_requests is decremented.
    // We use 'defer_shared' instead of 'defer_unique' because rxcpp
    // cannot deal with non-copyable callbacks.
    auto post_pending = defer_shared([self]{
      if (self->metrics)
        self->metrics->pending_requests.Decrement();
    });

    // The "subscribe" on the returned observable may be called much later,
    // so we do not immediately pick a connection.
    return RxLazy<std::shared_ptr<std::string>>(
    [self,path,bucket,post_pending=std::move(post_pending)]()
      -> messaging::MessageSequence {

      size_t conn_idx = self->getQuietestConn();

      post_pending->trigger();
      // NB. We can't use post_pending.reset() since post_pending is const.

      (self->open_requests_counts->at(conn_idx))++;

      if (self->metrics) {
        self->metrics->active_requests.Increment();
      }

      auto post_active = defer_shared([self, conn_idx]{
        (self->open_requests_counts->at(conn_idx))--;
        if (self->metrics)
          self->metrics->active_requests.Decrement();
      });

      return self->clients.at(conn_idx)->getObject(path, bucket)
        .op(RxButFirst(

          // RxButFirst makes sure the function below is called after
          // getObject's work should be done.
          [post_active=std::move(post_active)]() {
            post_active->trigger();
          }
      ));

    });
  }


  rxcpp::observable<std::string> S3PageStore::put(
      const std::string& _path,
      std::vector<std::shared_ptr<std::string>> page_parts)
  {
    std::string path = _path; // don't leave the reference dangling
    size_t pages_size = 0;

    for (const auto& page_part : page_parts) {
      pages_size += page_part->size();
    }

    if (this->metrics) {
      this->metrics->pending_requests.Increment();
      this->metrics->pending_pages_size.Increment(
          static_cast<double>(pages_size));
    }

    auto self = this->shared_from_this();

    auto post_pending = defer_shared([self,pages_size]{
      if (self->metrics) {
        self->metrics->pending_requests.Decrement();
        self->metrics->pending_pages_size.Decrement(
            static_cast<double>(pages_size));
      }
    });

    // The "subscribe" on the returned observable may be called much later,
    // so we do not immediately pick a connection.
    return RxLazy<std::string>(
    [self,path,page_parts=std::move(page_parts),post_pending=std::move(post_pending)]()
      -> rxcpp::observable<std::string> {

      size_t conn_idx = self->getQuietestConn();

      post_pending->trigger();

      (self->open_requests_counts->at(conn_idx))++;
      if (self->metrics) {
        self->metrics->active_requests.Increment();
      }

      auto post_active = defer_shared([self,conn_idx](){
        (self->open_requests_counts->at(conn_idx))--;
        if (self->metrics)
          self->metrics->active_requests.Decrement();
      });

      return self->clients.at(conn_idx)->putObject(path,
        self->write_bucket, page_parts)
        .op(RxButFirst(

          // RxButFirst makes sure the function below is called after
          // putObject's work should be done.
          [post_active=std::move(post_active)](){
            post_active->trigger();
          }
      ));

    });
  }

  // stores data directly on disk
  class LocalPageStore
    : public PageStore,
      public std::enable_shared_from_this<LocalPageStore>
  {
  public:

    messaging::MessageSequence
      get(const std::string& path) override;

    rxcpp::observable<std::string> put(
        const std::string& path,
        std::vector<std::shared_ptr<std::string>> page_parts) override;

    static std::shared_ptr<LocalPageStore> Create(
        std::shared_ptr<boost::asio::io_context> io_context,
        std::shared_ptr<Configuration> config);

    // pubic constructor for the sake of std::make_shared
    LocalPageStore(
        std::filesystem::path datadir,
        std::string bucket);

  private:

    std::filesystem::path bucketdir;
  };

  LocalPageStore::LocalPageStore(
      std::filesystem::path datadir,
      std::string bucket)
    : bucketdir(datadir/bucket)
  {
    if (!std::filesystem::is_directory(this->bucketdir)) {
      throw std::runtime_error("Configuration error: "
          + this->bucketdir.string() + " is not a directory.");
    }
  }

  std::shared_ptr<LocalPageStore> LocalPageStore::Create(
      std::shared_ptr<boost::asio::io_context> io_context,
      std::shared_ptr<Configuration> config)
  {
    std::filesystem::path datadir =
        config->get<std::filesystem::path>("DataDir");
    std::string bucket = config->get<std::string>("Bucket");

    return std::make_shared<LocalPageStore>(datadir, bucket);
  }


  messaging::MessageSequence
    LocalPageStore::get(const std::string& path) {

    std::filesystem::path fullpath = this->bucketdir / path;

    return CreateObservable<std::shared_ptr<std::string>> (
      [fullpath](rxcpp::subscriber<std::shared_ptr<std::string>> s){
        // we don't want to throw an error when the file doesn't exist
        if (std::filesystem::exists(fullpath)) {
          auto result = std::make_shared<std::string>();

          try {
            *result = ReadFile(fullpath);
          } catch(...) {
            LOG(LOG_TAG, error) << "could not read from "
              << std::quoted(fullpath.string());
            throw;
          }
          s.on_next(result);
        }

        s.on_completed();
      });
  }

  rxcpp::observable<std::string> LocalPageStore::put(
      const std::string& path,
      std::vector<std::shared_ptr<std::string>> page_parts) {

    std::filesystem::path fullpath = this->bucketdir / path;

    // since this is fallback code, speed is not of the essence
    auto page = std::make_shared<std::string>();
    for (auto& part : page_parts)
      *page += *part;

    return CreateObservable<std::string> (
      [fullpath, page](rxcpp::subscriber<std::string> s){
        try {
          std::filesystem::create_directories(fullpath.parent_path());
          WriteFile(fullpath, *page);
        } catch (...) {
          LOG(LOG_TAG, error) << "could not write to "
            << std::quoted(fullpath.string());
          throw;
        }
        s.on_next(s3::ETag(*page));

        s.on_completed();
      });
  }


  // Run both a LocalPageStore and an S3PageStore - to see if they agree.
  class DualPageStore
    : public PageStore,
      public std::enable_shared_from_this<DualPageStore>
  {
  public:

    messaging::MessageSequence
      get(const std::string& path) override;

    rxcpp::observable<std::string> put(
        const std::string& path,
        std::vector<std::shared_ptr<std::string>> page_parts) override;

    static std::shared_ptr<DualPageStore> Create(
        std::shared_ptr<boost::asio::io_context> io_context,
        std::shared_ptr<prometheus::Registry> metrics_registry,
        std::shared_ptr<Configuration> config);

    // pubic constructor for the sake of std::make_shared
    DualPageStore(
        std::shared_ptr<S3PageStore> s3store,
        std::shared_ptr<LocalPageStore> localstore);

  private:

    std::shared_ptr<S3PageStore> s3store;
    std::shared_ptr<LocalPageStore> localstore;
  };

  DualPageStore::DualPageStore(
      std::shared_ptr<S3PageStore> s3store,
      std::shared_ptr<LocalPageStore> localstore)
    : s3store(s3store), localstore(localstore)
  {
  }

  std::shared_ptr<DualPageStore> DualPageStore::Create(
      std::shared_ptr<boost::asio::io_context> io_context,
      std::shared_ptr<prometheus::Registry> metrics_registry,
      std::shared_ptr<Configuration> config)
  {
    return std::make_shared<DualPageStore>(
        S3PageStore::Create(io_context, metrics_registry, config),
        LocalPageStore::Create(io_context, config));
  }

  constexpr char SYNC_ERROR_MSG[]
    = "DualPageStore: disagreement between local and S3 storage!";

  messaging::MessageSequence
      DualPageStore::get(const std::string& path) {
    // forward the request to the S3 and local store, and merge the results
    // into one vector ...
    return this->s3store->get(path)
      .merge(this->localstore->get(path))
      .op(RxToVector())
      // ... and extract the contents of the vector, if any
      .flat_map(

      [](std::shared_ptr<std::vector<std::shared_ptr<std::string>>> values)
          -> messaging::MessageSequence {
        switch(values->size()) {
        case 0:
          return rxcpp::observable<>::empty<std::shared_ptr<std::string>>();
        case 2:
          if (*(values->at(0)) == *(values->at(1)))
            return rxcpp::observable<>::just(values->at(0));
          throw std::runtime_error(std::string(SYNC_ERROR_MSG) +
              " Get: Contents differ.");
        case 1:
          throw std::runtime_error(std::string(SYNC_ERROR_MSG) +
              " Get: Page found in only one of the two stores.");
        default:
          throw std::runtime_error("DualPageStore: Get: assertion error: "
              "got more than one result from a store.");
        }
      }).as_dynamic();
  }

  rxcpp::observable<std::string> DualPageStore::put(
      const std::string& path,
      std::vector<std::shared_ptr<std::string>> page_parts) {

    // forward the request to the S3 and local store, and merge the results
    // into one vector ...
    return this->s3store->put(path, page_parts)
      .merge(this->localstore->put(path, page_parts))
      .op(RxToVector())
      // ... and extract the contents of the vector, if any
      .flat_map([](std::shared_ptr<std::vector<std::string>> values)
          -> rxcpp::observable<std::string> {
        switch(values->size()) {
        case 2:
          if (values->at(0) == values->at(1))
            return rxcpp::observable<>::just(values->at(0));
          throw std::runtime_error(std::string(SYNC_ERROR_MSG) +
              " Put: ETags differ.");
        case 1:
          throw std::runtime_error(std::string(SYNC_ERROR_MSG) +
              " Put: only one store failed to put the given put.");
        case 0:
          throw std::runtime_error("DualPageStore: Put: both "
              "stores failed silently.");
        default:
          LOG(LOG_TAG, error) << "DualPageStore::put: got unexpectedly many "
              << "ETags from page stores: ";
          for (auto& etag : *values) {
            LOG(LOG_TAG, error) << "\t - " << std::quoted(etag);
          }
          throw std::runtime_error("DualPageStore: Put: assertion error: "
              "got more than one result from a store.");
        }
      }).as_dynamic();
  }


  std::shared_ptr<PageStore> PageStore::Create(
      std::shared_ptr<boost::asio::io_context> io_context,
      std::shared_ptr<prometheus::Registry> metrics_registry,
      std::shared_ptr<Configuration> config)
  {
    std::string type = config->get<std::string>("Type"); // throws

    if (type == "s3") {
      return S3PageStore::Create(io_context, metrics_registry, config);
    } else if (type == "local") {
      return LocalPageStore::Create(io_context, config);
    } else if (type == "dual") {
      return DualPageStore::Create(io_context, metrics_registry, config);
    }

    throw std::runtime_error("Configuration error: "
        "unknown page storage type, " + type
          + "; use 's3', 'local' or 'dual'.");
  }
}

