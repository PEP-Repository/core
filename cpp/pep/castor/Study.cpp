#include <pep/castor/Study.hpp>

#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/RxGroupToVectors.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/castor/ImportColumnNamer.hpp>
#include <pep/castor/Site.hpp>
#include <pep/castor/Participant.hpp>
#include <pep/castor/Form.hpp>
#include <pep/castor/Survey.hpp>
#include <pep/castor/SurveyPackage.hpp>
#include <pep/castor/SurveyPackageInstance.hpp>
#include <pep/castor/Visit.hpp>
#include <pep/castor/OptionGroup.hpp>
#include <pep/castor/Field.hpp>
#include <pep/castor/RepeatingData.hpp>
#include <pep/castor/Ptree.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-connect_forever.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-replay.hpp>
#include <rxcpp/operators/rx-take.hpp>

namespace pep {
namespace castor {

namespace {

const std::string API_ENDPOINT = "study";

}

Study::Study(std::shared_ptr<CastorConnection> connection, JsonPtr json)
  : CastorObject(json, "study_id"),
  mName(GetFromPtree<std::string>(*json, "name")),
  mSlug(GetFromPtree<std::string>(*json, "slug")),
  mConnection(connection) {
  assert(connection != nullptr);
}

void Study::setDefaultSiteByAbbreviation(const std::string& abbreviation) {
  auto action = mdefaultSiteWg->add("Default site assignment");
  auto self = SharedFrom(*this);
  getSites()
    .op(RxGroupToVectors([](std::shared_ptr<Site> site) {return site->getAbbreviation(); }))
    .concat_map([self, abbreviation](auto byAbbrev) -> rxcpp::observable<std::shared_ptr<Site>> {
    auto found = byAbbrev->find(abbreviation);
    if (found == byAbbrev->cend()) {
      std::string available = "<none>";
      if (!byAbbrev->empty()) {
        std::vector<std::string> abbrevs;
        abbrevs.reserve(byAbbrev->size());
        std::transform(byAbbrev->cbegin(), byAbbrev->cend(), std::back_inserter(abbrevs), [](const auto& pair) {return '"' + pair.first + '"'; });
        available = boost::algorithm::join(abbrevs, ", ");
      }
      std::string description = abbreviation.empty()
        ? "an empty abbreviation"
        : ("abbreviation \"" + abbreviation + '"');
      LOG("Study", error) << "Not assigning a default site to study " << self->getName()
        << " (slug " << self->getSlug() << ")"
        << " because no site could be found with " << description << '.'
        << " Available abbreviations are " << available << '.';
      return rxcpp::observable<>::empty<std::shared_ptr<Site>>();
    }
    return RxIterate(*(found->second)); // Return sites with the sought-after abbreviation
      })
    .subscribe(
      [self, abbreviation](std::shared_ptr<Site> site) {
        if (self->mdefaultSiteId != nullptr) {
          LOG("Study", warning) << "Multiple sites found for abbreviation " << abbreviation
            << " during default site retrieval for study " << self->getName()
            << " (slug " << self->getSlug() << "). Skipping site with ID " << site->getId()
            << " in favor of previously found " << (*self->mdefaultSiteId);
        }
        else {
          self->mdefaultSiteId = std::make_shared<std::string>(site->getId());
        }
      },
      [self, action, abbreviation](std::exception_ptr ep) {
        LOG("Study", error) << "Error occurred during default site retrieval for study " << self->getName()
          << " (slug " << self->getSlug() << ")"
          << " and abbreviation " << abbreviation
          << ": " << GetExceptionMessage(ep);
        action.done();
      },
      [action]() {
        action.done();
      }
    );
}

rxcpp::observable<std::string> Study::getDefaultSiteId() const {
  //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) Function leak seems like a false positive as long as WaitGroup finishes (TODO we should probably support cancellation)
  return mdefaultSiteWg->delayObservable<std::string>([self = SharedFrom(*this)]() {
    const auto& optional = self->mdefaultSiteId;
    if (!optional) {
      throw std::runtime_error("No default site ID has been set on study " + self->getName());
    }
    return rxcpp::observable<>::just(*optional);
  });
}


rxcpp::observable<std::shared_ptr<Participant>> Study::createParticipant(const std::string& participantId) {
  return getDefaultSiteId().flat_map([self = SharedFrom(*this), participantId](const std::string& siteId){
    return Participant::CreateNew(self, participantId, siteId);
  });
}
std::string Study::getName() const {
  return mName;
}

rxcpp::observable<std::shared_ptr<Site>> Study::getSites() {
  return Site::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<Participant>> Study::getParticipants() {
  return Participant::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<Survey>> Study::getSurveys() {
  return Survey::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<SurveyPackageInstance>> Study::getSurveyPackageInstances() {
  return SurveyPackageInstance::BulkRetrieve(SharedFrom(*this), this->getParticipants());
}

rxcpp::observable<std::shared_ptr<SurveyPackage>> Study::getSurveyPackages() {
  return SurveyPackage::RetrieveForParent(SharedFrom(*this));
}
rxcpp::observable<std::shared_ptr<RepeatingData>> Study::getRepeatingData() {
  return RepeatingData::RetrieveForParent(SharedFrom(*this));
}
std::string Study::getSlug() const {
  return mSlug;
}

rxcpp::observable<std::shared_ptr<Form>> Study::getForms() {
  return Form::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<Visit>> Study::getVisits() {
  return Visit::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<OptionGroup>> Study::getOptionGroups() {
  return OptionGroup::RetrieveForParent(SharedFrom(*this));
}

rxcpp::observable<std::shared_ptr<Field>> Study::getFields() {
  return Field::RetrieveForParent(SharedFrom(*this));
}

//! \return A url that can be used to retrieve this study from the Castor API
std::string Study::makeUrl() const {
  return API_ENDPOINT + "/" + this->getId();
}

rxcpp::observable<std::shared_ptr<Study>> Study::RetrieveForParent(std::shared_ptr<CastorConnection> connection) {
  return connection->getJsonEntries(API_ENDPOINT, "study")
    .map([connection](JsonPtr studyProperties) {
    return Study::Create(connection, studyProperties);
      });
}

}
}
