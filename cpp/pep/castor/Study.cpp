#include <pep/castor/Study.hpp>

#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/async/RxToVector.hpp>
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
    // - Not using RxToUnorderedList because we want to report duplicates (instead of raising an exception).
    // - Not using RxGroupToVectors because it produced a link failure: see e.g. https://gitlab.pep.cs.ru.nl/pep/core/-/jobs/840447#L2582 ,
    //    which may have been caused by this clang bug (or one very similar to it): https://github.com/llvm/llvm-project/issues/57561 .
    .op(RxToVector())
    .flat_map([self = SharedFrom(*this)](std::shared_ptr<std::vector<std::shared_ptr<Site>>> sites) -> rxcpp::observable<std::string> {
    // Iterate over all sites:
    // 1. collecting each (unique) abbreviation (so we can use them in a diagnostic that we may emit later), and
    // 2. assigning self->mDefaultSiteId if we find a Site matching self->mDefaultSiteAbbrev, and
    // 3. logging warnings (for followup hits) if multiple Sites match self->mDefaultSiteAbbrev.
    std::set<std::string> abbrevs;
    std::transform(sites->begin(), sites->end(), std::inserter(abbrevs, abbrevs.begin()), [self](std::shared_ptr<Site> site) {
      auto found = site->getAbbreviation();
      if (found == self->mDefaultSiteAbbrev) {
        if (!self->mDefaultSiteId.has_value()) {
          self->mDefaultSiteId = found;
        } else {
          LOG("Study", warning) << "Multiple sites found for abbreviation " << found
            << " during default site retrieval for study " << self->getName()
            << " (slug " << self->getSlug() << "). Skipping site with ID " << site->getId()
            << " in favor of previously found " << (*self->mDefaultSiteId);
        }
      }
      return found;
      });

    // If no Site matches self->mDefaultSiteAbbrev, don't return a value (but do output diagnostic information)
    if (!self->mDefaultSiteId.has_value()) {
      std::string available = "<none>";
      if (!abbrevs.empty()) {
        available = boost::algorithm::join(abbrevs, ", ");
      }
      std::string description = self->mDefaultSiteAbbrev->empty()
        ? "an empty abbreviation"
        : ("abbreviation \"" + *self->mDefaultSiteAbbrev + '"');
      LOG("Study", error) << "Not assigning a default site to study " << self->getName()
        << " (slug " << self->getSlug() << ")"
        << " because no site could be found with " << description << '.'
        << " Available abbreviations are " << available << '.'; // Makes it (much) easier to find the configuration error

      return rxcpp::observable<>::empty<std::string>();
    }

    // Return the ID of the (first) Site that matches self->mDefaultSiteAbbrev
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
