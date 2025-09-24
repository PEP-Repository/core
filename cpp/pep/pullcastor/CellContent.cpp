#include <pep/pullcastor/CellContent.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

namespace pep {
namespace castor {

namespace {

rxcpp::observable<std::string> LoadCellContent(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateResult& entry) {
  return client->retrieveData2(rxcpp::observable<>::from(entry), ticket, true)
    .flat_map([](std::shared_ptr<RetrieveResult> retrieveResult) {
      assert(retrieveResult->mContent);
      return retrieveResult->mContent->op(RxConcatenateStrings());
    });
}

}

rxcpp::observable<std::string> CellContent::getDataToStore(const std::string& existing) const {
  return this->getData()
    .flat_map([existing](const std::string& own) -> rxcpp::observable<std::string> {
    if (own == existing) {
      return rxcpp::observable<>::empty<std::string>();
    }
    return rxcpp::observable<>::just(own);
    });
}

std::shared_ptr<CellContent> CellContent::Create(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateAndRetrieveResult& ear) {
  auto preloaded = PreloadedCellContent::TryCreate(ear);
  if (preloaded != nullptr) {
    return preloaded;
  }
  return LazyCellContent::Create(client, ticket, ear);
}

PreloadedCellContent::PreloadedCellContent(const std::string& value)
  : mValue(value) {
}

std::shared_ptr<PreloadedCellContent> PreloadedCellContent::TryCreate(const EnumerateAndRetrieveResult& ear) {
  std::shared_ptr<PreloadedCellContent> result;
  if (ear.mDataSet) {
    result = PreloadedCellContent::Create(ear.mData);
  }
  return result;
}

rxcpp::observable<std::shared_ptr<PreloadedCellContent>> PreloadedCellContent::Load(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateAndRetrieveResult& ear) {
  auto result = TryCreate(ear);
  if (result != nullptr) {
    return rxcpp::observable<>::just(result);
  }
  return LoadCellContent(client, ticket, ear)
    .map([column = ear.mColumn](std::string content) {return PreloadedCellContent::Create(std::move(content)); });
}

rxcpp::observable<std::string> PreloadedCellContent::getData() const {
  return rxcpp::observable<>::just(mValue);
}

LazyCellContent::LazyCellContent(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateResult& entry) {
  mData = CreateRxCache([client, ticket, entry]() {
    return LoadCellContent(client, ticket, entry);
  });
}

rxcpp::observable<std::string> LazyCellContent::getData() const {
  return mData->observe();
}

}
}
