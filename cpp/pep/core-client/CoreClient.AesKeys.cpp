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

constexpr unsigned KeyRequestBatchSize = 2500;

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
  baseCtx->reqSizes.reserve(entries.size() / KeyRequestBatchSize + 1);
  baseCtx->reqs.reserve(entries.size() / KeyRequestBatchSize + 1);
  for (unsigned offset = 0; offset < entries.size(); offset += KeyRequestBatchSize) {
    EncryptionKeyRequest request;
    request.ticket2 = ticket;
    for (unsigned i = offset;
         i < std::min(static_cast<unsigned>(entries.size()), offset + KeyRequestBatchSize);
         i++) {
      const auto& entry = *entries[i];
      request.entries.emplace_back(
         entry.metadata,
         entry.polymorphicKey,
         KeyBlindMode::Unblind,
         entry.localPseudonymsIndex
      );
    }
    baseCtx->reqSizes.push_back(request.entries.size());
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
      unsigned offset = i * KeyRequestBatchSize;
      auto action = ctx->wg->add(
          "unblindKeys offset " + std::to_string(offset));
      auto req_index = i;
      getAccessManagerProxy(true)->requestEncryptionKey(std::move(ctx->reqs[i]))
      .last().subscribe([action, offset, ctx, req_index](
          EncryptionKeyResponse resp) {
        if (!ctx->ok)
          return;
        if (resp.keys.size() != ctx->reqSizes[req_index]) {
          std::ostringstream ss;
          ss << "EncryptionKeyResponse contains " << resp.keys.size()
             << " entries instead of " << ctx->reqSizes[req_index];
          ctx->on_error(std::make_exception_ptr(
                std::runtime_error(ss.str())));
          return;
        }

        for (unsigned i = offset;
            i < std::min(static_cast<unsigned>(ctx->encKeys.size()), offset + KeyRequestBatchSize);
            i++)
          ctx->encKeys[i] = resp.keys.at(i - offset);
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

  assert(request->entries.size() == keys.size());

  // Use multiple KeyRequest instances as needed to keep message size down.
  std::unordered_map<size_t, EncryptionKeyRequest> keyRequests; // Associate each KeyRequest with the corresponding offset in DataEntriesRequest2::entries_
  keyRequests.reserve(request->entries.size() / KeyRequestBatchSize + 1);
  for (size_t i = 0U; i < request->entries.size(); i++) {
    const auto& entry = request->entries[i];

    auto indexInKeyRequest = i % KeyRequestBatchSize;
    auto offset = i - indexInKeyRequest; // a multiple of KeyRequestBatchSize
    assert(offset % KeyRequestBatchSize == 0U);
    assert(keyRequests[offset].entries.size() == indexInKeyRequest);
    keyRequests[offset].entries.emplace_back(
      entry.metadata,
      EncryptedKey(systemPublicKeys_.globalDataEncryptionKey, keys[i].point),
      KeyBlindMode::Blind,
      entry.pseudonymIndex
    );
  }
  // Give each KeyRequest a (reference to the) ticket
  std::for_each(keyRequests.begin(), keyRequests.end(), [ticket = MakeSharedCopy(request->ticket)](std::pair<const size_t, EncryptionKeyRequest>& pair) {pair.second.ticket2 = ticket; });

  return RxIterate(std::move(keyRequests))
    .flat_map([this, request](std::pair<const size_t, EncryptionKeyRequest> pair) {
    auto [offset, keyRequest] = std::move(pair);
    const size_t count = keyRequest.entries.size();
    return getAccessManagerProxy(true)->requestEncryptionKey(std::move(keyRequest))
      .op(RxGetOne())
      .map([request, offset, count](EncryptionKeyResponse keyResponse) {
        if (keyResponse.keys.size() != count) {
          std::ostringstream ss;
          ss << "EncryptionKeyResponse contains " << keyResponse.keys.size()
            << " entries instead of " << count;
          throw std::runtime_error(ss.str());
        }

        for (size_t i = 0; i < count; i++) {
          request->entries[i + offset].polymorphicKey = keyResponse.keys[i];
        }

        return FakeVoid();
      });
    }).op(RxInstead(FakeVoid())); // Emit only a single FakeVoid "value"
}


}
