#pragma once

#include <pep/utils/Configuration_fwd.hpp>

#include <filesystem>

#include <QLabel>
#include <QPixmap>

struct BrandingConfiguration {
  std::string projectName;
  std::filesystem::path projectLogo;
};

class Branding {
private:
  QPixmap mLogo;
  QString mProjectName;

private:
  Branding(const QString& logoPath, const QString& projectName);

public:
  const QString& getProjectName() const noexcept { return mProjectName; }
  void showLogo(QLabel& host) const;
  
  static Branding Get(const pep::Configuration& configuration, const std::string& path);
};
