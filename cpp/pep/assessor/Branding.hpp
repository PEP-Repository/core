#pragma once

#include <pep/utils/Configuration.hpp>

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

namespace pep {

  template <>
  class PropertySerializer<BrandingConfiguration> : public PropertySerializerByReference<BrandingConfiguration> {
  public:
    void write(boost::property_tree::ptree& destination, const BrandingConfiguration& value) const override;
    void read(BrandingConfiguration& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
  };

}
