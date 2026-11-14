#pragma once

#include <pep/utils/Configuration_fwd.hpp>

#include <QLabel>
#include <QPixmap>

#include <filesystem>
#include <string>

struct BrandingConfiguration {
  std::string projectName;
  std::filesystem::path projectLogo;
};

class Branding {
private:
  QPixmap logo_;
  QString projectName_;

private:
  Branding(const QString& logoPath, const QString& projectName);

public:
  const QString& getProjectName() const noexcept { return projectName_; }
  void showLogo(QLabel& host) const;
  
  static Branding Get(const pep::Configuration& configuration, const std::string& path);
};
