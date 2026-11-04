#pragma once

#include <inja/environment.hpp>
#include <inja/json.hpp>

#include <filesystem>
#include <string>


namespace pep {

class TemplateEnvironment {
public:
  using Data = inja::json;

  explicit TemplateEnvironment(const std::filesystem::path& rootDir);
  std::string renderTemplate(std::filesystem::path templatePath, const Data& data);

private:
  inja::Environment environment_;
};

}

