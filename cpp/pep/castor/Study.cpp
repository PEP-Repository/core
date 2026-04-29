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

void Study::setDefaultSiteAbbreviation(const std::string& abbreviation) {
  if (mDefaultSiteAbbrev.has_value()) {
    if (*mDefaultSiteAbbrev != abbreviation) {
      throw std::runtime_error("Can't set default site abbreviation to '" + abbreviation + "' because it does not match previous value of '" + *mDefaultSiteAbbrev + "'");
    }
  }
  else {
    mDefaultSiteAbbrev = abbreviation;
  }
}

rxcpp::observable<std::string> Study::getDefaultSiteId() {
  if (mDefaultSiteId.has_value()) {
    return rxcpp::observable<>::just(*mDefaultSiteId);
  }
  if (!mDefaultSiteAbbrev.has_value()) {
    return rxcpp::observable<>::error<std::string>(std::runtime_error("No site abbreviation has been set on study " + this->getName()));
  }

  return this->getSites()
    .op(RxGroupToVectors([](std::shared_ptr<Site> site) {return site->getAbbreviation(); })) // Not using RxToUnorderedMap or similar to prevent exceptions in case of duplicate site->getAbbreviation() values
    .flat_map([self = SharedFrom(*this)](auto byAbbrev) -> rxcpp::observable<std::string> {
        const auto& abbreviation = *self->mDefaultSiteAbbrev;
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
          return rxcpp::observable<>::empty<std::string>();
        }

        const std::vector<std::shared_ptr<Site>>& sites = *found->second;
        assert(!sites.empty());
        self->mDefaultSiteId = sites.front()->getId();

        for (auto i = 1U; i < sites.size(); ++i) {
          LOG("Study", warning) << "Multiple sites found for abbreviation " << abbreviation
            << " during default site retrieval for study " << self->getName()
            << " (slug " << self->getSlug() << "). Skipping site with ID " << sites[i]->getId()
            << " in favor of previously found " << (*self->mDefaultSiteId);
        }

        return rxcpp::observable<>::just(*self->mDefaultSiteId);
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
