#include <pep/core-client/CoreClient.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/async/WaitGroup.hpp>
#include <pep/utils/OpenSSLHasher.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {

namespace {

const std::string LogTag("CoreClient.AesKeys");

constexpr unsigned KEY_REQUEST_BATCH_SIZE = 2500;

}

CoreClient::AESKey::AESKey(const CurvePoint& point)
  : point(point), bytes(Sha256().digest(point.pack())) {
}

rxcpp::observable<std::vector<CoreClient::AESKey>>
CoreClient::unblindAndDecryptKeys(
      std::span<const std::shared_ptr<EnumerateResult>> entries,
      std::shared_ptr<SignedTicket2> ticket) {
  PEP_LOG(LogTag, Severity::Debug) << "unblindAndDecryptKeys";

  struct Context {
    std::shared_ptr<WaitGroup> wg;
    std::vector<EncryptionKeyRequest> reqs;
    std::vector<size_t> reqSizes;
    std::vector<EncryptedKey> encKeys;
    bool ok = true;
    std::optional<rxcpp::subscriber<std::vector<EncryptedKey>>> subscriber;

    void on_error(const std::exception_ptr ep) {
      if (!this->ok)
        return;
      this->ok = false;
      this->subscriber->on_error(ep);
    }
  };
  auto baseCtx = std::make_shared<Context>();
  baseCtx->encKeys.resize(entries.size());
  baseCtx->reqSizes.reserve(entries.size() / KEY_REQUEST_BATCH_SIZE + 1);
  baseCtx->reqs.reserve(entries.size() / KEY_REQUEST_BATCH_SIZE + 1);
  for (unsigned offset = 0; offset < entries.size(); offset += KEY_REQUEST_BATCH_SIZE) {
    EncryptionKeyRequest request;
    request.ticket2_ = ticket;
    for (unsigned i = offset;
         i < std::min(static_cast<unsigned>(entries.size()), offset + KEY_REQUEST_BATCH_SIZE);
         i++) {
      const auto& entry = *entries[i];
      request.entries_.emplace_back(
         entry.metadata,
         entry.polymorphicKey,
         KeyBlindMode::Unblind,
         entry.localPseudonymsIndex
      );
    }
    baseCtx->reqSizes.push_back(request.entries_.size());
    baseCtx->reqs.push_back(std::move(request));
  }
  // We proceed in two steps.  Step one: we send the request to unblind the keys.
  return CreateObservable<std::vector<EncryptedKey>>(
    [this, baseCtx](
      rxcpp::subscriber<std::vector<EncryptedKey>> subscriber) {
    auto ctx = std::make_shared<Context>(*baseCtx);
    ctx->subscriber = subscriber;
    ctx->wg = WaitGroup::Create();
    for (unsigned i = 0; i < ctx->reqs.size(); i++) {
      unsigned offset = i * KEY_REQUEST_BATCH_SIZE;
      auto action = ctx->wg->add(
          "unblindKeys offset " + std::to_string(offset));
      auto req_index = i;
      getAccessManagerProxy(true)->requestEncryptionKey(std::move(ctx->reqs[i]))
      .last().subscribe([action, offset, ctx, req_index](
          EncryptionKeyResponse resp) {
        if (!ctx->ok)
          return;
        if (resp.keys_.size() != ctx->reqSizes[req_index]) {
          std::ostringstream ss;
          ss << "EncryptionKeyResponse contains " << resp.keys_.size()
             << " entries instead of " << ctx->reqSizes[req_index];
          ctx->on_error(std::make_exception_ptr(
                std::runtime_error(ss.str())));
          return;
        }

        for (unsigned i = offset;
            i < std::min(static_cast<unsigned>(ctx->encKeys.size()), offset + KEY_REQUEST_BATCH_SIZE);
            i++)
          ctx->encKeys[i] = resp.keys_.at(i - offset);
        action.done();
      }, [ctx](std::exception_ptr ep){
        ctx->on_error(ep);
      });
    }
    ctx->wg->wait([ctx](){
      ctx->subscriber->on_next(ctx->encKeys);
      ctx->subscriber->on_completed();
    });
  }).flat_map([this](std::vector<EncryptedKey> encKeys){
    // Step two: we decrypt the retrieved keys.
    return getWorkerPool()->batched_map<8>(std::move(encKeys),
           ObserveOnAsio(*ioContext_),
        [this](EncryptedKey encKey) {
      auto point = encKey.decrypt(privateKeyData_);
      return AESKey(point);
    });
  });
}

rxcpp::observable<FakeVoid> CoreClient::encryptAndBlindKeys(
  std::shared_ptr<DataEntriesRequest2<DataStoreEntry2>> request,
  const std::vector<AESKey>& keys) {
  PEP_LOG(LogTag, Severity::Debug) << "encryptAndBlindKeys";

  assert(request->entries_.size() == keys.size());

  // Use multiple KeyRequest instances as needed to keep message size down.
  std::unordered_map<size_t, EncryptionKeyRequest> keyRequests; // Associate each KeyRequest with the corresponding offset in DataEntriesRequest2::entries_
  keyRequests.reserve(request->entries_.size() / KEY_REQUEST_BATCH_SIZE + 1);
  for (size_t i = 0U; i < request->entries_.size(); i++) {
    const auto& entry = request->entries_[i];

    auto indexInKeyRequest = i % KEY_REQUEST_BATCH_SIZE;
    auto offset = i - indexInKeyRequest; // a multiple of KEY_REQUEST_BATCH_SIZE
    assert(offset % KEY_REQUEST_BATCH_SIZE == 0U);
    assert(keyRequests[offset].entries_.size() == indexInKeyRequest);
    keyRequests[offset].entries_.emplace_back(
      entry.metadata_,
      EncryptedKey(systemPublicKeys_.globalDataEncryptionKey, keys[i].point),
      KeyBlindMode::Blind,
      entry.pseudonymIndex_
    );
  }
  // Give each KeyRequest a (reference to the) ticket
  std::for_each(keyRequests.begin(), keyRequests.end(), [ticket = MakeSharedCopy(request->ticket_)](std::pair<const size_t, EncryptionKeyRequest>& pair) {pair.second.ticket2_ = ticket; });

  return RxIterate(std::move(keyRequests))
    .flat_map([this, request](std::pair<const size_t, EncryptionKeyRequest> pair) {
    auto [offset, keyRequest] = std::move(pair);
    const size_t count = keyRequest.entries_.size();
    return getAccessManagerProxy(true)->requestEncryptionKey(std::move(keyRequest))
      .op(RxGetOne())
      .map([request, offset, count](EncryptionKeyResponse keyResponse) {
        if (keyResponse.keys_.size() != count) {
          std::ostringstream ss;
          ss << "EncryptionKeyResponse contains " << keyResponse.keys_.size()
            << " entries instead of " << count;
          throw std::runtime_error(ss.str());
        }

        for (size_t i = 0; i < count; i++) {
          request->entries_[i + offset].polymorphicKey_ = keyResponse.keys_[i];
        }

        return FakeVoid();
      });
    }).op(RxInstead(FakeVoid())); // Emit only a single FakeVoid "value"
}


}
