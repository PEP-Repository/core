#pragma once

#include <pep/server/NetworkedServer.hpp>

#include <filesystem>
#include <vector>

namespace pep {

/// All PEP servers, hosted together in a single process.
class Servers {
private:
  std::vector<NetworkedServer> instances_;

public:
  /// \brief Constructor: creates all servers.
  /// \param configDir Directory containing a subdirectory with configuration for each server.
  explicit Servers(const std::filesystem::path& configDir);

  /// \brief Runs all servers, each on its own thread.
  /// \remark Blocks until the "stop" method is called.
  void run();

  /// \brief Stops all servers, causing the "run" method to return.
  /// \remark May be called from any thread.
  void stop();
};

}
