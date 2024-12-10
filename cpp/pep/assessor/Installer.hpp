#pragma once

#include <pep/gui/PlaintextCredentials.hpp>

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
  virtual unsigned getPipelineId() const = 0;
  virtual unsigned getJobId() const = 0;

  virtual bool supersedesRunningVersion() const = 0;
  void start(const Context& context) const;

  static std::shared_ptr<Installer> GetAvailable();
};

#endif
