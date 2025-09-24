#include <pep/assessor/Branding.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>
#include <pep/assessor/Branding.PropertySerializer.hpp>

namespace {

const std::string LOG_TAG = "PEP Assessor branding";

}

Branding::Branding(const QString& logoPath, const QString& projectName)
  : mLogo(logoPath), mProjectName(projectName) {
}

void Branding::showLogo(QLabel& host) const {
  host.setScaledContents(false);
  host.setAlignment(Qt::AlignCenter);
  host.setPixmap(mLogo.scaled(host.size(), Qt::KeepAspectRatio));
}

Branding Branding::Get(const pep::Configuration& configuration, const std::string& path) {
  QString projectName = "PEP Assessor";
  QString logoPath = ":/images/PEP.svg";

  auto configured = configuration.get<std::optional<BrandingConfiguration>>(path);
  if (configured) {
    if (configured->projectName.empty()) {
      LOG(LOG_TAG, pep::warning) << "Empty project name configured; using generic value";
    }
    else {
      projectName = QString::fromStdString(configured->projectName);
    }

    if (!configured->projectLogo.empty()) {
      if (std::filesystem::exists(configured->projectLogo)) {
        logoPath = QString::fromStdString(configured->projectLogo.string());
      }
      else {
        LOG(LOG_TAG, pep::warning) << "Project logo could not be found at " << configured->projectLogo << "; using PEP logo";
      }
    }
  }

  return Branding(logoPath, projectName);
}
