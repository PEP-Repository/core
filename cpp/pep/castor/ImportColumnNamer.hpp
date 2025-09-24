#pragma once

#include <pep/structure/ColumnName.hpp>
#include <pep/structure/ShortPseudonyms.hpp>
#include <pep/castor/Participant.hpp>
#include <pep/castor/Form.hpp>
#include <pep/castor/SurveyPackage.hpp>
#include <pep/castor/SurveyPackageInstance.hpp>
#include <pep/castor/SurveyStep.hpp>

namespace pep {
namespace castor {
  
class ImportColumnNamer {
private:
  ColumnNameMappings mMappings;

private:
  std::string joinColumnNameSections(const std::string& prefix, const std::vector<std::string>& sections) const;

public:
  explicit ImportColumnNamer(const ColumnNameMappings& mappings);

  rxcpp::observable<std::string> getImportableColumnNames(std::shared_ptr<CastorConnection> connection, const ShortPseudonymDefinition& sp, const std::optional<unsigned>& answerSetCount);

  std::string getColumnName(const std::string& prefix, std::shared_ptr<Form> form) const;
  std::string getColumnName(const std::string& prefix, std::shared_ptr<Form> form, std::shared_ptr<Participant> participant) const;

  std::string getColumnName(const std::string& prefix, const std::string& spName, std::shared_ptr<SurveyStep> step, const std::optional<unsigned>& index = std::nullopt) const;
  std::string getWeekNumberColumnName(const std::string& prefix, const std::string& spName, std::shared_ptr<SurveyStep> step, unsigned index) const;
};

}
}
