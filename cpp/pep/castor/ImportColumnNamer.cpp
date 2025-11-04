#include <pep/castor/ImportColumnNamer.hpp>
#include <pep/castor/Visit.hpp>

#include <boost/algorithm/string/join.hpp>

namespace pep {
namespace castor {

ImportColumnNamer::ImportColumnNamer(ColumnNameMappings mappings)
  : mMappings(std::move(mappings)) {
}

std::string ImportColumnNamer::joinColumnNameSections(const std::string& configuredPrefix, const std::vector<std::string>& sections) const {
  std::vector<std::string> parts = { configuredPrefix };
  parts.reserve(parts.size() + sections.size());
  std::transform(sections.cbegin(), sections.cend(), std::back_inserter(parts), [&mappings = mMappings](const std::string& section) {
    return mappings.getColumnNameSectionFor(section);
    });
  return boost::algorithm::join(parts, ".");
}

rxcpp::observable<std::string> ImportColumnNamer::getImportableColumnNames(std::shared_ptr<CastorConnection> connection, const ShortPseudonymDefinition& sp, const std::optional<unsigned>& answerSetCount) {
  auto spName = sp.getColumn().getFullName();

  const auto& castorSp = sp.getCastor();
  if (!castorSp) {
    throw std::runtime_error("Short pseudonym " + spName + " does not refer to a Castor study");
  }
  const auto& storageDefinitions = castorSp->getStorageDefinitions();
  if (storageDefinitions.empty()) {
    throw std::runtime_error("No storage configured for short pseudonym " + spName);
  }

  auto studySlug = castorSp->getStudySlug();
  if (studySlug.empty()) {
    throw std::runtime_error("No study slug configured for short pseudonym " + spName);
  }
  return rxcpp::rxs::iterate(storageDefinitions).flat_map([studySlug, spName, connection, nameMappings = mMappings, answerSetCount](std::shared_ptr<CastorStorageDefinition> storage) {
    const auto& dataColumn = storage->getDataColumn();
    if (dataColumn.empty()) {
      throw std::runtime_error("No data column configured for storage of Castor short pseudonym " + spName);
    }

    std::string slug;
    if (!storage->getImportStudySlug().empty()) {
      slug = storage->getImportStudySlug();
    }
    else {
      slug = studySlug;
    }

    return connection->getStudyBySlug(slug)
      .flat_map([dataColumn, storage, spName, answerSetCount, namer = std::make_shared<ImportColumnNamer>(nameMappings)](std::shared_ptr<Study> study) -> rxcpp::observable<std::string> {
      auto type = storage->getStudyType();
      switch (type) {
      case pep::CastorStudyType::STUDY:
        return study->getForms()
          .map([dataColumn, namer](std::shared_ptr<pep::castor::Form> form) {return namer->getColumnName(dataColumn, form); });

      case pep::CastorStudyType::SURVEY:
        // If all survey package instances should be imported, both "week offset device column" and "answer set count" must be specified.
        // If only a single ("the latest") survey package instance should be imported, neither "week offset device column" nor "answer set count" must be specified.
        if (storage->getWeekOffsetDeviceColumn().empty()) {
          if (answerSetCount.has_value()) {
            throw std::runtime_error("Only specify an answer set count when a week offset device column has also been specified");
          }
        }
        else {
          if (!answerSetCount.has_value()) {
            throw std::runtime_error("An answer set count must be provided for studies from which all survey (instances) must be imported");
          }
        }

        return study->getSurveyPackages()
          .flat_map([dataColumn, answerSetCount, namer](std::shared_ptr<SurveyPackage> sp) {
          return sp->getSurveys()
            .flat_map([dataColumn, answerSetCount, spName = sp->getName(), namer](std::shared_ptr<Survey> survey) {
            return survey->getSteps()
              .flat_map([dataColumn, answerSetCount, spName, namer](std::shared_ptr<SurveyStep> step) -> rxcpp::observable<std::string> {
              if (!answerSetCount.has_value()) {
                return rxcpp::observable<>::just(namer->getColumnName(dataColumn, spName, step));
              }
              assert(*answerSetCount > 0U);
              return rxcpp::observable<>::range(0, static_cast<int>(*answerSetCount) - 1)
                .flat_map([dataColumn, spName, namer, step](int index) {
                auto payload = namer->getColumnName(dataColumn, spName, step, static_cast<unsigned int>(index));
                auto weekno = namer->getWeekNumberColumnName(dataColumn, spName, step, static_cast<unsigned int>(index));
                return rxcpp::observable<>::just(payload)
                  .concat(rxcpp::observable<>::just(weekno));
                });
              });
              });
            });

      case pep::CastorStudyType::REPEATING_DATA:
        return rxcpp::observable<>::just(storage->getDataColumn());
      }

      throw std::runtime_error("Column " + spName + " has unsupported study type " + std::to_string(type));
    });
  });

}

std::string ImportColumnNamer::getColumnName(const std::string& prefix, std::shared_ptr<Form> form) const {
  return this->joinColumnNameSections(prefix, { form->getVisit()->getName(), form->getName() });
}

std::string ImportColumnNamer::getColumnName(const std::string& prefix, std::shared_ptr<Form> form, std::shared_ptr<Participant> participant) const {
  assert(participant != nullptr);
  return getColumnName(prefix, form);
}

std::string ImportColumnNamer::getColumnName(const std::string& prefix, const std::string& spName, std::shared_ptr<SurveyStep> step, const std::optional<unsigned>& index) const {
  std::vector<std::string> sections = { spName, step->getSurvey()->getName(), step->getName() };
  if (index.has_value()) {
    sections.push_back("AnswerSet" + std::to_string(*index));
  }
  return this->joinColumnNameSections(prefix, sections);
}

std::string ImportColumnNamer::getWeekNumberColumnName(const std::string& prefix, const std::string& spName, std::shared_ptr<SurveyStep> step, unsigned index) const {
  return this->getColumnName(prefix, spName, step, index) + ".WeekNumber";
}

}
}
