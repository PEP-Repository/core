#pragma once

#include <pep/gui/PlaintextCredentials.hpp>
#include <pep/versioning/SemanticVersion.hpp>
#include <filesystem>

#include <memory>

#ifdef _WIN32

class Installer {
public:
  struct Context {
    std::filesystem::path logDirectory;
    std::filesystem::path elevateExe;
    std::function<pep::win32api::PlaintextCredentials()> getAdministrativeCredentials;
  };

protected:
  virtual std::filesystem::path getLocalMsiPath() const = 0;

public:
  virtual unsigned getMajorVersion() const = 0;
  virtual unsigned getMinorVersion() const = 0;
  virtual unsigned getBuild() const = 0;
  virtual unsigned getRevision() const = 0;

  virtual bool supersedesRunningVersion() const = 0;
  void start(const Context& context) const;

  static std::shared_ptr<Installer> GetAvailable();
  pep::SemanticVersion getSemver() const;
};

#endif
