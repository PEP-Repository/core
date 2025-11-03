#include <pep/async/RxIterate.hpp>
#include <pep/castor/CastorClient.hpp>
#include <pep/castor/Ptree.hpp>
#include <pep/castor/Study.hpp>
#include <pep/utils/Configuration.hpp>

#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-switch_if_empty.hpp>
#include <rxcpp/operators/rx-take.hpp>

#include <boost/property_tree/ptree.hpp>

namespace pep::castor {

namespace {

/*!
  * \brief Creates (heap-allocated) child ptrees from nodes in a parent ptree.
  * \param parent The parent ptree containing the (list of) child ptrees.
  * \param embeddedItemsNodeName The name of the node containing the child ptrees to return. Function will locate the children under "_embedded.theSpecifiedEmbeddedItemsNodeName".
  * \return A heap-allocated child ptree for every node the parent has at the specified location.
  * \remark Helper function to convert a multi-item JSON response page ("here's a ptree for a full page of items") to individual items ("here's a single ptree per item").
  *         The returned ptrees are heap-allocated (as opposed to stack-allocated) so they can be efficiently passed through RX pipelines.
  */
std::vector<std::shared_ptr<boost::property_tree::ptree>> CreateSharedChildTrees(JsonPtr parent, const std::string& embeddedItemsNodeName) {
  const auto& children = GetFromPtree<boost::property_tree::ptree>(*parent, "_embedded." + embeddedItemsNodeName);
  std::vector<std::shared_ptr<boost::property_tree::ptree>> result;
  result.reserve(children.size());
  std::transform(children.begin(), children.end(), std::back_inserter(result), [](const auto& item) {return std::make_shared<boost::property_tree::ptree>(item.second); });
  return result;
}

}

/*!
  * \brief Network connectivity implementation for the "CastorConnection" class.
  * \remark A level of indirection (i.e. a separate struct) is needed because CastorConnection.hpp can't forward declare the nested CastorClient::Connection class: see https://stackoverflow.com/a/1021809
  */
struct CastorConnection::Implementor {
  std::shared_ptr<CastorClient> client;
};

CastorConnection::CastorConnection(const std::filesystem::path& apiKeyFile, std::shared_ptr<boost::asio::io_context> io_context)
  : CastorConnection(EndPoint("data.castoredc.com", 443), ApiKey::FromFile(apiKeyFile), io_context) {
}

CastorConnection::CastorConnection(const EndPoint& endPoint, const ApiKey& apiKey, std::shared_ptr<boost::asio::io_context> io_context, const std::optional<std::filesystem::path>& caCert)
  : mImplementor(std::make_unique<Implementor>()) {
  assert(io_context != nullptr);

  mImplementor->client = castor::CastorClient::Create(*io_context, endPoint, apiKey.id, apiKey.secret, caCert);
  mOnRequestForwarding = mImplementor->client->onRequest.subscribe([this](std::shared_ptr<const HTTPRequest> request) { onRequest.notify(request); });
  mImplementor->client->start();
}

CastorConnection::~CastorConnection() noexcept {
  mOnRequestForwarding.cancel();
  mImplementor.reset();
}

std::shared_ptr<HTTPRequest> CastorConnection::makeGet(const std::string& path) {
  return mImplementor->client->makeGet(path);
}

std::shared_ptr<HTTPRequest> CastorConnection::makePost(const std::string& path, const std::string& body) {
  return mImplementor->client->makePost(path, body);
}

rxcpp::observable<JsonPtr> CastorConnection::sendCastorRequest(std::shared_ptr<HTTPRequest> request) {
  return mImplementor->client->sendCastorRequest(request);
}

rxcpp::observable<JsonPtr> CastorConnection::getJsonEntries(const std::string& apiPath, const std::string& embeddedItemsNodeName) {
  std::shared_ptr<HTTPRequest> request = this->makeGet(apiPath);
  return this->sendCastorRequest(request).flat_map([embeddedItemsNodeName](JsonPtr response) {
    auto list = CreateSharedChildTrees(response, embeddedItemsNodeName); // Clone these here to prevent ptrees from being copied by value in call to observable<>::iterate
    return RxIterate(std::move(list))
      .map([](std::shared_ptr<boost::property_tree::ptree> ptree) {return std::static_pointer_cast<const boost::property_tree::ptree>(std::move(ptree)); });
  });
}

rxcpp::observable<AuthenticationStatus> CastorConnection::authenticationStatus() {
  return mImplementor->client->authenticationStatus();
}

void CastorConnection::reauthenticate() {
  mImplementor->client->reauthenticate();
}

rxcpp::observable<std::shared_ptr<Study>> CastorConnection::getStudies() {
  return Study::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<Study>> CastorConnection::getStudyBySlug(const std::string& slug) {
  return getStudies().filter([slug](std::shared_ptr<Study> study) { return study->getSlug() == slug; })
    .switch_if_empty(rxcpp::rxs::error<std::shared_ptr<Study>>(rxcpp::empty_error("No Castor study found with slug " + slug)))
    .first();
}

CastorException CastorException::FromErrorResponse(const HTTPResponse& response, const std::string& additionalInfo) {
  std::string title = response.getStatusMessage();
  std::string body = response.getBody();
  std::string detail = body;
  try {
    boost::property_tree::ptree responseJson;
    ReadJsonIntoPtree(responseJson, body);
    title = responseJson.get<std::string>("error", title);
    title = responseJson.get<std::string>("title", title);
    detail = responseJson.get<std::string>("error_description", detail);
    detail = responseJson.get<std::string>("detail", detail);
  } catch (const boost::property_tree::ptree_error&) {
    // Ignore: Already logged by ReadJsonIntoPtree
  }
  return CastorException(response.getStatusCode(), title, detail + "\n" + additionalInfo);
}

}
