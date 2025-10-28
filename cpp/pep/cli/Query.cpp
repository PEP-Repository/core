#include <pep/auth/UserGroup.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class CommandQuery : public ChildCommandOf<CliApplication> {
public:
  explicit CommandQuery(CliApplication& parent)
    : ChildCommandOf<CliApplication>("query", "Queries the system", parent) {
  }

private:
  struct ColumnAccessModes {
    bool readable = false;
    bool writable = false;
    bool meta_readable = false;
    bool meta_writable = false;
  };

  struct ParticipantGroupAccessModes {
    bool access = false;
    bool enumerate = false;
  };

  static void ReportColumnAccess(const std::string& caption, const std::map<std::string, ColumnAccessModes> entries) {
    std::cout << caption << " (" << entries.size() << "):\n";
    if (!entries.empty()) {
      for (const auto& entry : entries) {
        std::cout << "  "
          << (entry.second.meta_readable ? 'm' : ' ')
          << (entry.second.readable      ? 'r' : ' ')
          << (entry.second.writable      ? 'w' : ' ')
          << ' ' << entry.first << '\n';
      }
      std::cout << std::endl;
    }
  }

  class CommandQueryColumnAccess : public ChildCommandOf<CommandQuery> {
  public:
    explicit CommandQueryColumnAccess(CommandQuery& parent)
      : ChildCommandOf<CommandQuery>("column-access", "Reports enrolled user's access to columns and column groups", parent) {
    }

    inline std::optional<std::string> getAdditionalDescription() const override {
      return std::string("Column (group) access modes are reported using the following abbreviations:\n")
        + "  m - cell metadata (such as data presence and timestamp) are readable\n"
        + "  r - cell data are readable (implies read access to metadata as well)\n"
        + "  w - cell data are writable";
    }

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#query-column-access"; }

  int execute() override {
    return executeEventLoopFor([](std::shared_ptr<pep::CoreClient> client) {
      return client->getAccessManagerProxy()->getAccessibleColumns(false) // Implicit column access is noted in a separate output line
        .map(
          [userGroup = client->getEnrolledGroup()](const pep::ColumnAccess& access) {
            // Copy received information to std::map<> to sort entries (by name) for output
            std::map<std::string, ColumnAccessModes> cgs, columns;
            bool hasReadAccess = false;
            bool hasMetaWriteAccess = false;

            for (const auto& entry : access.columnGroups) {
              if (!entry.second.modes.empty()) {
                // Create column group entry and set accessibility flags
                auto& modes = cgs[entry.first];
                for (const auto& mode : entry.second.modes) {
                  if (mode == "read") {
                    modes.readable = true;
                    hasReadAccess = true;
                  }
                  else if (mode == "write") {
                    modes.writable = true;
                  }
                  else if (mode == "read-meta") {
                    modes.meta_readable = true;
                  }
                  else if (mode == "write-meta") {
                    modes.meta_writable = true;
                    hasMetaWriteAccess = true;
                  }
                  else {
                    throw std::runtime_error("Unsupported access mode '" + mode + "' encountered for column group '" + entry.first + "'");
                  }
                }

                // Create and/or update entries for columns associated with the group
                for (auto i : entry.second.columns.mIndices) {
                  auto& column = columns[access.columns[i]];
                  column.readable |= modes.readable;
                  column.writable |= modes.writable;
                  column.meta_readable |= modes.meta_readable;
                  column.meta_writable |= modes.meta_writable;
                }
              }
            }

            // Present output to user
            ReportColumnAccess("ColumnGroups", cgs);
            ReportColumnAccess("Columns", columns);
            if (userGroup == pep::UserGroup::DataAdministrator) {
              std::cerr
                << "As a member of the \"" << userGroup << "\" user group, you also have implicit" << '\n'
                << "\"read-meta\" access to all column groups and columns. Use the \"pepcli ama query\"" << '\n'
                << "command to list them." << std::endl;
            }
            else if (hasReadAccess) {
              std::cerr << "The \"read\" access privilege grants access to \"read-meta\"data as well." << std::endl;
            }

            if (hasMetaWriteAccess) {
              std::cerr << "The \"write-meta\" access privilege grants access to \"write\" data as well." << std::endl;
            }

            return pep::FakeVoid();
          });
      });
  }
  };

  class CommandQueryParticipantGroupAccess : public ChildCommandOf<CommandQuery> {
  public:
    explicit CommandQueryParticipantGroupAccess(CommandQuery& parent)
      : ChildCommandOf<CommandQuery>("participant-group-access", "Reports enrolled user's access to participant groups", parent) {
    }

    static void ReportParticipantGroupAccess(const pep::ParticipantGroupAccess& access, const std::string& userGroup) {
      std::map<std::string, ParticipantGroupAccessModes> pgs;

      for (const auto& entry : access.participantGroups) {
        if (!entry.second.empty()) {
          // Create column group entry and set accessibility flags
          auto& modes = pgs[entry.first];
          for (const auto& mode : entry.second) {
            if (mode == "access") {
              modes.access = true;
            }
            else if (mode == "enumerate") {
              modes.enumerate = true;
            }
            else {
              throw std::runtime_error("Unsupported access mode '" + mode + "' encountered for participant group '" + entry.first + "'");
            }
          }
        }
      }

      if (userGroup == pep::UserGroup::DataAdministrator) {
        std::cerr
          << "As a member of the \"" << userGroup << "\" user group, you have implicit" << '\n'
          << "full access to all participant groups." << std::endl;
      }

      if (!pgs.empty()) {
        std::cout << "Participant groups (" << pgs.size() << "):\n";
        for (const auto& pg : pgs) {
          std::cout << "  " << pg.first << " ("
            << (pg.second.access ? "access" : "")
            << (pg.second.access && pg.second.enumerate ? ", " : "")
            << (pg.second.enumerate ? "enumerate" : "")
            << ")\n";
        }
        std::cout << std::endl;
      }
    }

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#query-participant-group-access"; }

    int execute() override {
      return executeEventLoopFor([](std::shared_ptr<pep::CoreClient> client) {
        return client->getAccessManagerProxy()->getAccessibleParticipantGroups(true)
          .map([userGroup = client->getEnrolledGroup()](const pep::ParticipantGroupAccess& access) {
          ReportParticipantGroupAccess(access, userGroup);
          return pep::FakeVoid();
        });
        });
    }
  };

  class CommandQueryEnrollment : public ChildCommandOf<CommandQuery> {
  public:
    explicit CommandQueryEnrollment(CommandQuery& parent)
      : ChildCommandOf<CommandQuery>("enrollment", "Reports details on the enrollment", parent) {
    }

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#query-enrollment"; }

  int execute() override {
    return executeEventLoopFor([](std::shared_ptr<pep::CoreClient> client) {
      std::cout << "Enrolled as user \"" << client->getEnrolledUser() << "\""
        << " in group \"" << client->getEnrolledGroup() << "\"." << std::endl;
      return rxcpp::observable<>::just(pep::FakeVoid());
    });
  }
  };

  class CommandQueryToken : public ChildCommandOf<CommandQuery> {
  public:
    explicit CommandQueryToken(CommandQuery& parent)
      : ChildCommandOf<CommandQuery>("token", "Reports details on a token", parent) {
    }

  private:
    static void ReportTimestamp(std::ostream& destination, const std::string& announce, pep::Timestamp timestamp) {
      destination << '\n' << announce << ' ' << timestamp << ", i.e. " << pep::TimestampToXmlDateTime(timestamp);
    }

  protected:
    int execute() override {
      auto token = this->getParent().getParent().getTokenParameter();
      if (token == std::nullopt) {
        throw std::runtime_error("No token specified. Please specify a token to query on the command line.");
      }

      std::cout
        << "User \"" << token->getSubject() << "\"\n"
        << "Group \"" << token->getGroup() << "\"";
      ReportTimestamp(std::cout, "Issued at", token->getIssuedAt());
      ReportTimestamp(std::cout, "Expires at", token->getExpiresAt());
      std::cout << std::endl;

      return EXIT_SUCCESS;
    }
  };

protected:
  inline std::optional<std::string> getRelativeDocumentationUrl() const override {
    return "using-pepcli#query";
  }

  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
    std::vector<std::shared_ptr<pep::commandline::Command>> result;
    result.push_back(std::make_shared<CommandQueryColumnAccess>(*this));
    result.push_back(std::make_shared<CommandQueryParticipantGroupAccess>(*this));
    result.push_back(std::make_shared<CommandQueryEnrollment>(*this));
    result.push_back(std::make_shared<CommandQueryToken>(*this));
    return result;
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandQuery(CliApplication& parent) {
  return std::make_shared<CommandQuery>(parent);
}
