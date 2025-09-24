#pragma once

#include <pep/utils/Configuration_fwd.hpp>
#include <pep/messaging/MessageSequence.hpp>

#include <pep/async/IoContext_fwd.hpp>

namespace prometheus {
  class Registry;
}


namespace pep
{
  // interface to store and retrieve pages
  class PageStore
  {
  public:
    virtual ~PageStore() = default;

    // returns either one string (when the page was found) or
    // an empty observable (when the page does not exist).
    virtual messaging::MessageSequence get(
        const std::string& path) = 0;

    // returns the MD5 ('ETag') of the page computed by the backend,
    // usually the S3 server.
    virtual rxcpp::observable<std::string> put(
        const std::string& path,
        std::vector<std::shared_ptr<std::string>> page_parts) = 0;

    inline rxcpp::observable<std::string> put(
        const std::string& path,
        const std::string& page) {

      return this->put(path, std::vector<std::shared_ptr<std::string>>{
          std::make_shared<std::string>(page)});
    }

    // Creates a new page store from the given configuration, io_context,
    // and prometheus registry.  The registry may be empty.
    //
    // For now the config->get<std::string>("type") must be one of:
    //
    //     "s3":     use an S3 server to store the pages -
    //               used for production;
    //
    //     "local":  use a legacy 'datadir' on the local disk - 
    //               used for local development; 
    //
    //     "dual":   use both an S3 server and legacy local storage -
    //               used by integration, to keep the two methods in sync.
    //
    // The exact format for the "config" can be found in the 
    // <Type>PageStore::Create static methods in PageStore.cpp.
    static std::shared_ptr<PageStore> Create(
        std::shared_ptr<boost::asio::io_context> io_context,
        std::shared_ptr<prometheus::Registry> metrics_registry,
        std::shared_ptr<Configuration> config);
    
  protected:
    PageStore() = default;
  };
}
