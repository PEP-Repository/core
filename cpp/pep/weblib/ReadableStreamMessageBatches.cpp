#include <pep/weblib/ReadableStreamMessageBatches.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/async/RxSubsequently.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Log.hpp>
#include <pep/weblib/EmscriptenValPtr.hpp>
#include <pep/weblib/OnEmscriptenThread.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <rxcpp/operators/rx-subscribe_on.hpp>

#include <boost/noncopyable.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <span>
#include <stdexcept>

using namespace emscripten;
using namespace pep;

namespace {

const std::string LogTag("ReadableStreamMessageBatches");

using MessageSequence = rxcpp::observable<std::shared_ptr<std::string>>;
using MessageBatches = rxcpp::observable<MessageSequence>;

struct StreamReadState {
  weblib::EmscriptenValPtr reader;
  std::size_t pageSize;
  rxcpp::observe_on_one_worker ioWorker;
  std::string pending;
  bool streamDone = false;
  std::atomic<bool> finished = false;
  rxcpp::observe_on_one_worker mainWorker = weblib::observe_on_emscripten_main_thread();
};

class StreamPageReader : public boost::noncopyable {
  std::shared_ptr<StreamReadState> state_;
  rxcpp::subscriber<std::shared_ptr<std::string>> inner_;
  val self_;

  StreamPageReader(std::shared_ptr<StreamReadState> state, rxcpp::subscriber<std::shared_ptr<std::string>> inner)
    : state_(std::move(state)), inner_(std::move(inner)) {}

  void deleteSelf() {
    self_.call<void>("delete");
    PEP_LOG(LogTag, Severity::Verbose) << this << " deleted self";
  }

  void next() {
    if (state_->pending.size() >= state_->pageSize || state_->streamDone) {
      emitPage();
      return;
    }
    PEP_LOG(LogTag, Severity::Verbose) << this << " reading chunk, " << state_->pending.size() << " bytes pending";
    val promise = state_->reader->call<val>("read");
    promise.call<val>("then", self_["onChunk"].call<val>("bind", self_), self_["onError"].call<val>("bind", self_));
  }

  void emitPage() {
    auto& pending = state_->pending;
    const auto length = std::min(pending.size(), state_->pageSize);
    if (state_->streamDone && length == pending.size()) {
      state_->finished = true;
    }
    if (length != 0) {
      auto page = std::make_shared<std::string>(pending, 0, length);
      pending.erase(0, length);
      inner_.on_next(std::move(page));
    }
    inner_.on_completed();
    deleteSelf();
  }

public:
  static MessageSequence ReadPage(std::shared_ptr<StreamReadState> state) {
    return CreateObservable<std::shared_ptr<std::string>>(
        [state](rxcpp::subscriber<std::shared_ptr<std::string>> inner) {
          val self(new StreamPageReader(state, std::move(inner)), allow_raw_pointers{});
          auto* reader = self.as<StreamPageReader*>(allow_raw_pointers{});
          reader->self_ = self;
          reader->next();
        })
        .subscribe_on(state->mainWorker);
  }

  void onChunk(val result) {
    try {
      if (result["done"].as<bool>()) {
        state_->streamDone = true;
      } else {
        const val chunk = result["value"];
        if (!chunk.instanceof(val::global("Uint8Array"))) {
          throw std::invalid_argument("ReadableStream must produce Uint8Array chunks");
        }
        auto& pending = state_->pending;
        const auto offset = pending.size();
        const auto length = chunk["byteLength"].as<std::size_t>();
        pending.resize(offset + length);
        const std::span bytes = ConvertBytes<std::uint8_t>(std::span{pending.data() + offset, length});
        val(typed_memory_view(bytes.size(), bytes.data())).call<void>("set", chunk);
      }
      next();
    } catch (...) {
      inner_.on_error(std::current_exception());
      deleteSelf();
    }
  }

  void onError(val error) {
    auto message = val::global("String")(std::move(error)).as<std::string>();
    PEP_LOG(LogTag, Severity::Debug) << this << " ReadableStream read failed: " << message;
    inner_.on_error(std::make_exception_ptr(std::runtime_error("Failed to read from ReadableStream: " + message)));
    deleteSelf();
  }
};

MessageSequence MakeStreamBatch(std::shared_ptr<StreamReadState> state) {
  return StreamPageReader::ReadPage(state)
      .observe_on(state->ioWorker);
}

void ProvideStreamBatch(std::shared_ptr<StreamReadState> state, rxcpp::subscriber<MessageSequence> outer) {
  if (!state->finished) {
    outer.on_next(MakeStreamBatch(state)
        .op(RxSubsequently([state, outer]() {
          ProvideStreamBatch(state, outer);
        })));
  } else {
    outer.on_completed();
  }
}

}

EMSCRIPTEN_BINDINGS(ReadableStreamMessageBatches) {
  class_<StreamPageReader>("StreamPageReader")
      .function("onChunk", &StreamPageReader::onChunk)
      .function("onError", &StreamPageReader::onError)
      ;
}

namespace pep::weblib {

MessageBatches ReadableStreamToMessageBatches(val reader, std::size_t pageSize, rxcpp::observe_on_one_worker ioWorker) {
  if (pageSize == 0) {
    throw std::invalid_argument("pageSize must be nonzero");
  }
  auto state = std::make_shared<StreamReadState>(std::move(reader), pageSize, std::move(ioWorker));
  return CreateObservable<MessageSequence>(
      [state](rxcpp::subscriber<MessageSequence> outer) {
        ProvideStreamBatch(state, outer);
      });
}

}
