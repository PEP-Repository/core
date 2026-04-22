#include <gtest/gtest.h>

#include "CommandLineParameterTestFixture.hpp"

// Truth Tables for possible command/parameter combinations and forwarding patterns
//
// Legend:
//  [N] = Test case N
// X[N] = EXPECT_DEBUG_DEATH (assert failure)
//    0 = not tested, already sufficiently covered
//
// Table 1: Command-Parameter combinations
// Shows how command types interact with parameter types.
//
//           Par │Normal│Depr   │Alias │Forwarding│NoLongSupp│
//               │Par   │ Par   │Par   │Depr      │Par       │
// Cmd           │      │       │      │Par       │          │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// Normal Cmd    │ [1]  │ [2]   │ [3]  │   [4]    │   [5]    │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// Depr Cmd      │ [6]  │ [7]   │ [8]  │   [9]    │   [10]   │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// Alias Cmd     │ [11] │ [12]  │ [13] │   [14]   │   [15]   │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// Forwarding    │ [16] │ [17]  │ [18] │   [19]   │   [20]   │
// Depr Cmd      │      │       │      │          │          │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// NoLongSupp    │ [21] │ [22]  │ [23] │   [24]   │   [25]   │
// Cmd           │      │       │      │          │          │
//
// For all where applicable: extra parameters are forwarded as well
// Cant combine no longer supported with deprecation, or with forwarding/aliasing
// Parameter transformations should occur before forwarding to new command
// Targets should support the transformed parameter

// Table 2: Parameter-Parameter combinations
// Shows how multiple parameter types combine in a single invocation.
//
//         Par 2 │Normal│Depr   │Alias |Forwarding│NoLongSupp│
// Par 1         │Par   │Par    │Par   |Depr Par  │Par       │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// Normal Par    │ [1]  │   -   │   -  |    -     │    -     │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// Depr Par      │ [2]  │  [3]  │   -  |    -     │    -     │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// Alias Par     │ [4]  │  [5]  │[6a]  |    -     │    -     │
//               |      │       │[X6b] │          |          │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// Forwarding    │ [7]  │  [8]  │ [9]  |   [10]   │    -     │
// Depr Par      │      │       │      |          │          │
// ──────────────┼──────┼───────┼──────┼──────────┼──────────┤
// NoLongSupp    │ [11] │ [12]  │ [13] |   [14]   │   [15]   │
// Par           │      │       │      |          │          │
//
// Table 3: All command/parameter forwarding combinations
// Shows which command/parameter types can forward to which command/parameter types.
// Both commands and parameters can forward to commands with optional parameter transformations.
// Parameters can also forward to just parameters (rename/transform without command change).
//
//            TO │Normal │Depr   │Forwarding│Alias│NoLongSupp│Normal│Depr   │Alias |Forwarding │NoLongSupp│
// FROM          │Cmd    │Cmd    │Depr Cmd  │Cmd  │Cmd       │Par   │Par    │Par   |Depr Par   │Par       │
// ──────────────┼───────┼───────┼──────────┼─────┼──────────┼──────┼───────┼──────┼───────────┼──────────┤
// Alias Cmd     │ [1]   │ [2]   │   X[3]   │X[4] │   X[5]   │ [6]  │ [7]   │ [8]  │  X[9]     │   X[10]  │
// ──────────────┼───────┼───────┼──────────┼─────┼──────────┼──────┼───────┼──────┼───────────┼──────────┤
// Forwarding    │ [11]  │ [12]  │   X[13]  │X[14]│   X[15]  │  0   │  0    │  X0  │   X0      │   X0     │
// Depr Cmd      │       │       │          │     │          │      │       │      │           │          │
// ──────────────┼───────┼───────┼──────────┼─────┼──────────┼──────┼───────┼──────┼───────────┼──────────┤
// Alias Par     │ [16]  │ [17]  │   X[18]  │X[19]│   X[20]  │ [21] │ [22]  │ X[23]│  X[24]    │   X[25]  │
// ──────────────┼───────┼───────┼──────────┼─────┼──────────┼──────┼───────┼──────┼───────────┼──────────┤
// Forwarding    │ [26]  │ [27]  │   X0     │X0   │   X0     │   0  │  0    │ X0   │  X0       │   X0     │
// Depr Par      │       │       │          │     │          │      │       │      │           │          │
//
// Extra test cases (E):
// 1. Complex forwarding: Transform parameter into two new parameters
// 2. Complex forwarding: 

// ---- Shared command tree used by all tests ----
//
// Legend:
//   [NORMAL]              - No special behavior
//   [ALIAS]               - Silent forward/rename, no warning
//   [DEPRECATED_ALIAS]    - Warning + forwards/renames to target
//   [DEPRECATED]          - Warning + keeps original (no forward/rename)
//   [NO_LONGER_SUPPORTED] - Error message + exit with failure
//   [Table-Test]          - format shows which tests use each entity
//
// app                            [NORMAL]           Application management tool
//  │ --quick-start               [ALIAS]               -> to "app server start --port"    [2-6a][3-16][X2-6b][X3-8]
//  │ --verbose-mode              [ALIAS]               -> to "app ... --verbose"          [2-6a]
//  │ --quick-deploy              [ALIAS]               -> to "app deploy --name"          [3-17][X2-6b]
//  │ --bad-forward-depr-cmd      [DEPRECATED_ALIAS]    -> to "app init-user"              [X3-18]
//  │ --bad-alias-cmd             [ALIAS]               -> to "app create-user"            [X3-19]
//  │ --bad-removed-cmd           [ALIAS]               -> to "app old-user"               [X3-20]
//  │ --legacy-port               [DEPRECATED_ALIAS]    -> to "app server start --port"    [3-26]
//  │ --deploy-name               [DEPRECATED_ALIAS]    -> to "app deploy --name"          [3-27]
//  │
//  ├── user                      [NORMAL]              User management                    
//  │     --name                  [NORMAL]              User name                          [1-1][1-6][1-11][1-16][1-21][2-1][2-2][2-4][2-7][2-11][3-1][3-2][3-3][3-4][3-5][3-11][3-12][3-13][3-14][3-15]
//  │     --old-email             [DEPRECATED]                                             [1-2][1-12][1-17][1-22][2-2][2-3][2-5][2-8][2-12]
//  │     --mail                  [ALIAS]               -> to "app user --email"           [1-3][1-13][1-18][1-23][2-4][2-5][2-6a][2-9][2-13][3-8][3-21]
//  │     --username              [DEPRECATED_ALIAS]    -> to "app user --email"           [1-4][1-14][1-19][1-24][2-7][2-8][2-9][2-10][2-14]
//  │     --role                  [NO_LONGER_SUPPORTED]                                    [1-5][1-10][1-15][1-20][1-25][2-11][2-12][2-13][2-14][2-15]
//  │     --email                 [NORMAL]              User email address (current)       [2-1]
//  │     --old-name              [DEPRECATED]                                             [1-7][2-3]
//  │     --permission            [NO_LONGER_SUPPORTED]                                    [2-15]
//  │     --legacy-name           [DEPRECATED_ALIAS]    -> to "app user --name"            [2-10]
//  │     --chain-mail            [ALIAS]               -> to "app user --mail"            [X3-23]
//  │     --forward-username      [DEPRECATED_ALIAS]    -> to "app user --username"        [X3-9][X3-24]
//  │     --removed-role          [ALIAS]               -> to "app user --role"            [X3-10][X3-25]
//  |     --old-mail              [ALIAS]               -> to "app user --old-email"       [3-22]
//  |
//  ├── deploy                    [DEPRECATED]                                             [1-6][1-7][1-8][1-9][1-10][3-2][X2-6b]
//  │     --name                  [NORMAL]              Deploy name                        [1-6][3-2][3-12][3-27][X2-6b]
//  │     --old-name              [DEPRECATED]                                             [1-7]
//  │     --deploy-alias          [ALIAS]               -> to "app deploy --name"          [1-8]
//  │     --deployment-name       [DEPRECATED_ALIAS]    -> to "app deploy --name"          [1-9]
//  │     --role                  [NO_LONGER_SUPPORTED]                                    [1-10]
//  |
//  ├── create-user               [ALIAS]               -> to "app user"                   [1-11][1-12][1-13][1-14][1-15][3-1]
//  ├── init-user                 [DEPRECATED_ALIAS]    -> to "app user"                   [1-16][1-17][1-18][1-19][1-20][3-11]
//  ├── old-user                  [NO_LONGER_SUPPORTED]                                    [1-21][1-22][1-23][1-24][1-25]
//  |
//  ├── server                    [NORMAL]              Server management                  [3-16][X2-6b][X3-8]
//  │   └── start                 [NORMAL]              Start the server                   [3-16][X2-6b][X3-8]
//  │         --port              [NORMAL]              Server port number                 [3-16][X2-6b][X3-8]
//  │         --verbose           [NORMAL]              Enable verbose logging             [2-6a]
//  │
//  ├── db                        [ALIAS]               -> to "app database"               [X3-14]
//  ├── database                  [NORMAL]              Database operations                [X3-14]
//  │     --source                [NORMAL]              Database source path               [X3-14]
//  │
//  ├── create-deploy             [ALIAS]               -> to "app deploy"                 [3-2]
//  ├── new-user                  [ALIAS]               -> to "app init-user"              [X3-3]
//  ├── user-alias                [ALIAS]               -> to "app create-user"            [X3-4]
//  ├── removed-alias             [ALIAS]               -> to "app old-user"               [X3-5]
//  ├── create-user-email         [ALIAS]               -> to "app user --email"           [3-6]
//  ├── create-user-old-email     [ALIAS]               -> to "app user --old-email        [3-7]
//  ├── create-user-mail          [ALIAS]               -> to "app user --mail"            [X3-8]
//  ├── create-user-fwd-username  [ALIAS]               -> to "app user --forward-username"[X3-9]
//  ├── create-user-removed-role  [ALIAS]               -> to "app user --removed-role"    [X3-10]
//  ├── init-deploy               [DEPRECATED_ALIAS]    -> to "app deploy"                 [3-12]
//  ├── old-init                  [DEPRECATED_ALIAS]    -> to "app init-user"              [X3-13]
//  ├── old-db                    [DEPRECATED_ALIAS]    -> to "app database"               [X3-14]
//  └── removed-init              [DEPRECATED_ALIAS]    -> to "app old-user"               [X3-15]

namespace pep::application::test {

#define DEBUG_OUTPUT 1

// Convenience constants
const pep::commandline::CommandPath appCommandPath{"app"};
const pep::commandline::CommandPath userCommandPath{"app", "user"};
const pep::commandline::CommandPath serverStartCommandPath{"app", "server", "start"};
const pep::commandline::CommandPath deployCommandPath{"app", "deploy"};

// Helper functions
std::queue<std::string> ToQueue(std::initializer_list<std::string> values) {
  std::queue<std::string> result;
  for (const auto& value : values) {
    result.push(value);
  }
  return result;
}

std::pair<int, std::string> ProcessWithCapturedStderr(AppCmd& cmd, std::initializer_list<std::string> args) {
  auto queuedArgs = ToQueue(args);
  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(queuedArgs);
  std::string err = testing::internal::GetCapturedStderr();
  #if DEBUG_OUTPUT
  std::cout << "=== Captured stderr ===\n" << err << "======================\n";
  #endif
  return {exitCode, std::move(err)};
}

// RecordingCommandMixin implementation
void RecordingCommandMixin::captureParameters() {
  if (auto* parent = getParentMixin()) {
    parent->captureParameters();
  }
  auto* cmd = getCommand();

  std::vector<std::string> segments;
  const pep::commandline::Command* current = cmd;
  while (current != nullptr) {
    segments.insert(segments.begin(), current->getName());
    current = current->getParentCommand();
  }
  pep::commandline::CommandPath commandPath{segments};
  
  getStorage().recordCommandValues(commandPath, cmd->getParameterValues());
}

// AppCmd implementation
std::string AppCmd::getName() const { return "app"; }

std::string AppCmd::getDescription() const { return "Application management tool."; }

pep::commandline::Parameters AppCmd::getSupportedParameters() const {
    return pep::commandline::Command::getSupportedParameters()
      + pep::commandline::Parameter("global-param", "Global application parameter").value(pep::commandline::Value<std::string>())
      + pep::commandline::Parameter("legacy-port", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::NamedValues toAdd;
            auto portStr = vals.getOptional<std::string>("legacy-port");
            if (portStr.has_value()) {
              pep::commandline::Values portValues;
              portValues.add(std::stoi(*portStr));
              toAdd["port"] = portValues;
            }
            return pep::commandline::ParameterTransformationResult{std::move(toAdd), nullptr, {"server", "start"}};
          })
          .deprecated("Use 'server start --port' instead.")
      + pep::commandline::Parameter("deploy-name", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::NamedValues toAdd;
            toAdd["name"] = vals["deploy-name"];
            return pep::commandline::ParameterTransformationResult{std::move(toAdd), nullptr, {"deploy"}};
          })
          .deprecated("Use 'deploy --name' instead.")
      + pep::commandline::Parameter("quick-start", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::NamedValues toAdd;
            auto portStr = vals.getOptional<std::string>("quick-start");
            if (portStr.has_value()) {
              pep::commandline::Values portValues;
              portValues.add(std::stoi(*portStr));
              toAdd["port"] = portValues;
            }
            return pep::commandline::ParameterTransformationResult{std::move(toAdd), nullptr, {"server", "start"}};
          })
      + pep::commandline::Parameter("verbose-mode", std::nullopt)
          .alias("verbose")
      + pep::commandline::Parameter("quick-deploy", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::NamedValues toAdd;
            toAdd["name"] = vals["quick-deploy"];
            return pep::commandline::ParameterTransformationResult{std::move(toAdd), nullptr, {"deploy"}};
          })
      + pep::commandline::Parameter("bad-forward-depr-cmd", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::NamedValues toAdd;
            toAdd["name"] = vals["bad-forward-depr-cmd"];
            return pep::commandline::ParameterTransformationResult{std::move(toAdd), nullptr, {"init-user"}};
          })
      + pep::commandline::Parameter("bad-alias-cmd", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::NamedValues toAdd;
            toAdd["name"] = vals["bad-alias-cmd"];
            return pep::commandline::ParameterTransformationResult{std::move(toAdd), nullptr, {"create-user"}};
          })
      + pep::commandline::Parameter("bad-removed-cmd", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            return pep::commandline::ParameterTransformationResult{{}, nullptr, {"old-user"}};
          })
      + pep::commandline::Parameter("bad-transform-path", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::NamedValues toAdd;
            toAdd["name"] = vals["bad-transform-path"];
            return pep::commandline::ParameterTransformationResult{std::move(toAdd), nullptr, {"missing-subcommand"}};
          })
      + pep::commandline::Parameter("mode", std::nullopt).value(pep::commandline::Value<std::string>())
          .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::CommandPath childPath;
            auto modeVal = vals.getOptional<std::string>("mode");
            if (modeVal.has_value()) {
              if (*modeVal == "start") {
                childPath = {"server", "start"};
              } else if (*modeVal == "deploy") {
                childPath = {"deploy"};
              }
            }
            return pep::commandline::ParameterTransformationResult{{}, nullptr, childPath};
          })
          .deprecated("Use specific command instead.");
}

std::vector<std::shared_ptr<pep::commandline::Command>> AppCmd::createChildCommands() {
  auto forwardToUserParam = [](const std::string& paramName) {
    return [paramName](std::queue<std::string> args) {
      pep::commandline::NamedValues transformed;
      if (!args.empty()) {
        transformed.add(paramName, args.front());
        args.pop();
      }
      return transformed;
    };
  };
  
  return {
    std::make_shared<UserCmd>(*this),
    std::make_shared<DatabaseCmd>(*this),
    std::make_shared<ServerCmd>(*this),
    std::make_shared<DeployCmd>(*this),
    pep::commandline::CreateAliasCommand(*this, "create-user", *this, {"user"}),
    pep::commandline::CreateAliasCommand(*this, "create-user-email", *this, {"user"}, nullptr, forwardToUserParam("email")),
    pep::commandline::CreateAliasCommand(*this, "create-user-old-email", *this, {"user"}, nullptr, forwardToUserParam("old-email")),
    pep::commandline::CreateAliasCommand(*this, "create-user-mail", *this, {"user"}, nullptr, forwardToUserParam("mail")),
    pep::commandline::CreateAliasCommand(*this, "create-user-fwd-username", *this, {"user"}, nullptr, forwardToUserParam("forward-username")),
    pep::commandline::CreateAliasCommand(*this, "create-user-removed-role", *this, {"user"}, nullptr, forwardToUserParam("removed-role")),
    pep::commandline::CreateAliasCommand(*this, "db", *this, {"database"}),
    pep::commandline::CreateDeprecatedCommand(*this, "init-user", *this, {"user"}, "Use 'user' instead."),
    pep::commandline::CreateDeprecatedCommand(*this, "old-init", *this, {"init-user"}, "Use 'user' instead of the deprecated init-user."),
    pep::commandline::CreateAliasCommand(*this, "new-user", *this, {"init-user"}),
    pep::commandline::CreateDeprecatedCommand(*this, "init-deploy", *this, {"deploy"}, "Use 'deploy' instead."),
    pep::commandline::CreateAliasCommand(*this, "create-deploy", *this, {"deploy"}),
    pep::commandline::CreateDeprecatedCommand(*this, "old-db", *this, {"db"}, "Use 'database' instead."),
    pep::commandline::CreateAliasCommand(*this, "user-alias", *this, {"create-user"}),
    pep::commandline::CreateDeprecatedCommand(*this, "removed-init", *this, {"old-user"}, "This forwards to removed command."),
    pep::commandline::CreateAliasCommand(*this, "removed-alias", *this, {"old-user"}),
    pep::commandline::CreateNoLongerSupportedCommand(*this, "old-user", "Use 'user' instead."),
  };
}

void AppCmd::recordCommandValues(const pep::commandline::CommandPath& commandPath, const pep::commandline::NamedValues& values) {
  mCapturedParams[commandPath.toString()] = values;
}

// UserCmd implementation
UserCmd::UserCmd(AppCmd& parent)
  : pep::commandline::ChildCommandOf<AppCmd>("user", "User management", parent) {}

AppCmd& UserCmd::getStorage() { return getParent(); }

RecordingCommandMixin* UserCmd::getParentMixin() { return &getParent(); }

pep::commandline::Parameters UserCmd::getSupportedParameters() const {
  return pep::commandline::ChildCommandOf<AppCmd>::getSupportedParameters()
    + pep::commandline::Parameter("name", "User name").value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("email", "User email address").value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("age", "User age").value(pep::commandline::Value<int>())
    + pep::commandline::Parameter("groups", "User groups").value(pep::commandline::Value<std::string>().multiple())
    + pep::commandline::Parameter("username", std::nullopt).value(pep::commandline::Value<std::string>())
        .rename("email")
    + pep::commandline::Parameter("old-email", std::nullopt).value(pep::commandline::Value<std::string>())
        .deprecated("Use --email instead.")
    + pep::commandline::Parameter("old-name", std::nullopt).value(pep::commandline::Value<std::string>())
        .deprecated("Use --name instead.")
    + pep::commandline::Parameter("role", std::nullopt).value(pep::commandline::Value<std::string>())
        .noLongerSupported("Use --name instead.")
    + pep::commandline::Parameter("permission", std::nullopt).value(pep::commandline::Value<std::string>())
        .noLongerSupported("Permissions are no longer supported.")
    + pep::commandline::Parameter("mail", std::nullopt).value(pep::commandline::Value<std::string>())
        .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
          pep::commandline::NamedValues toAdd;
          toAdd["email"] = vals["mail"];
          return pep::commandline::ParameterTransformationResult{std::move(toAdd)};
        })
    + pep::commandline::Parameter("legacy-name", std::nullopt).value(pep::commandline::Value<std::string>())
        .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
          pep::commandline::NamedValues toAdd;
          toAdd["name"] = vals["legacy-name"];
          return pep::commandline::ParameterTransformationResult{std::move(toAdd)};
        }).deprecated("Use --name instead.")
    + pep::commandline::Parameter("forward-username", std::nullopt).value(pep::commandline::Value<std::string>())
        .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
          pep::commandline::NamedValues toAdd;
          toAdd["username"] = vals["forward-username"];
          return pep::commandline::ParameterTransformationResult{std::move(toAdd)};
        })
    + pep::commandline::Parameter("removed-role", std::nullopt).value(pep::commandline::Value<std::string>())
        .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
          pep::commandline::NamedValues toAdd;
          toAdd["role"] = vals["removed-role"];
          return pep::commandline::ParameterTransformationResult{std::move(toAdd)};
        })
    + pep::commandline::Parameter("old-mail", std::nullopt).value(pep::commandline::Value<std::string>())
        .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
          pep::commandline::NamedValues toAdd;
          toAdd["old-email"] = vals["old-mail"];
          return pep::commandline::ParameterTransformationResult{std::move(toAdd)};
        })
    + pep::commandline::Parameter("chain-mail", std::nullopt).value(pep::commandline::Value<std::string>())
        .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
          pep::commandline::NamedValues toAdd;
          toAdd["mail"] = vals["chain-mail"];
          return pep::commandline::ParameterTransformationResult{std::move(toAdd)};
        });
}

int UserCmd::execute() {
  captureParameters();
  return EXIT_SUCCESS;
}

// DatabaseCmd implementation
DatabaseCmd::DatabaseCmd(AppCmd& parent)
  : pep::commandline::ChildCommandOf<AppCmd>("database", "Database operations", parent) {}

AppCmd& DatabaseCmd::getStorage() { return getParent(); }

RecordingCommandMixin* DatabaseCmd::getParentMixin() { return &getParent(); }

pep::commandline::Parameters DatabaseCmd::getSupportedParameters() const {
  return pep::commandline::ChildCommandOf<AppCmd>::getSupportedParameters()
    + pep::commandline::Parameter("source", "Database source path").value(pep::commandline::Value<std::string>().required())
    + pep::commandline::Parameter("timeout", "Database timeout in seconds").value(pep::commandline::Value<int>())
    + pep::commandline::Parameter("tables", "Database tables to process").value(pep::commandline::Value<std::string>().multiple());
}

int DatabaseCmd::execute() {
  captureParameters();
  return EXIT_SUCCESS;
}

// DeployCmd implementation
DeployCmd::DeployCmd(AppCmd& parent)
  : pep::commandline::DeprecatedChildCommandOf<AppCmd>("deploy", "Deploy the application.", parent, "Use 'user' instead.") {}

AppCmd& DeployCmd::getStorage() { return getParent(); }

RecordingCommandMixin* DeployCmd::getParentMixin() { return &getParent(); }

pep::commandline::Parameters DeployCmd::getSupportedParameters() const {
  return pep::commandline::DeprecatedChildCommandOf<AppCmd>::getSupportedParameters()
    + pep::commandline::Parameter("name", "Deployment name").value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("deployment-name", std::nullopt).value(pep::commandline::Value<std::string>())
        .rename("name")
    + pep::commandline::Parameter("old-name", std::nullopt).value(pep::commandline::Value<std::string>())
        .deprecated("Use --name instead.")
    + pep::commandline::Parameter("deploy-alias", std::nullopt).value(pep::commandline::Value<std::string>())
        .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
          pep::commandline::NamedValues toAdd;
          toAdd["name"] = vals["deploy-alias"];
          return pep::commandline::ParameterTransformationResult{std::move(toAdd)};
        })
    + pep::commandline::Parameter("role", std::nullopt).value(pep::commandline::Value<std::string>())
        .noLongerSupported("Use --name instead.");
}

int DeployCmd::execute() {
  captureParameters();
  return EXIT_SUCCESS;
}

// ServerCmd implementation
ServerCmd::ServerCmd(AppCmd& parent)
  : pep::commandline::ChildCommandOf<AppCmd>("server", "Server management", parent) {}

AppCmd& ServerCmd::getStorage() { return getParent(); }

RecordingCommandMixin* ServerCmd::getParentMixin() { return &getParent(); }

std::vector<std::shared_ptr<pep::commandline::Command>> ServerCmd::createChildCommands() {
  return { std::make_shared<StartCmd>(*this) };
}

// StartCmd implementation
StartCmd::StartCmd(ServerCmd& parent)
  : pep::commandline::ChildCommandOf<ServerCmd>("start", "Start the server", parent) {}

AppCmd& StartCmd::getStorage() { return getParent().getStorage(); }

RecordingCommandMixin* StartCmd::getParentMixin() { return &getParent(); }

pep::commandline::Parameters StartCmd::getSupportedParameters() const {
  return pep::commandline::ChildCommandOf<ServerCmd>::getSupportedParameters()
    + pep::commandline::Parameter("host", "Server host address").value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("port", "Server port number").value(pep::commandline::Value<int>())
    + pep::commandline::Parameter("verbose", "Enable verbose logging")
    + pep::commandline::Parameter("workers", "Number of worker threads").value(pep::commandline::Value<int>())
    + pep::commandline::Parameter("modules", "Modules to load").value(pep::commandline::Value<std::string>().multiple())
    + pep::commandline::Parameter("host-port", std::nullopt).value(pep::commandline::Value<std::string>())
        .forwardingAlias([](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
          pep::commandline::NamedValues toAdd;
          auto hostPortVal = vals.getOptional<std::string>("host-port");
          if (hostPortVal.has_value()) {
            std::string combined = *hostPortVal;
            auto colonPos = combined.find(':');
            if (colonPos != std::string::npos) {
              pep::commandline::Values hostValues;
              hostValues.add(combined.substr(0, colonPos));
              toAdd["host"] = hostValues;
              pep::commandline::Values portValues;
              portValues.add(std::stoi(combined.substr(colonPos + 1)));
              toAdd["port"] = portValues;
            }
          }
          return pep::commandline::ParameterTransformationResult{std::move(toAdd)};
        })
        .deprecated("Split into --host and --port.");
}

int StartCmd::execute() {
  captureParameters();
  return EXIT_SUCCESS;
}

} // namespace pep::application::test

// Make test infrastructure accessible to TEST() macros
using namespace pep::application::test;

// Multi-value parameter test
TEST(ParameterTypes, MultipleValuesParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Alice", "--groups", "admin", "--groups", "dev", "--groups", "qa"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  auto groups = cmd.getCapturedValues<std::string>(userCommandPath, "groups");
  ASSERT_EQ(groups.size(), 3);
  ASSERT_EQ(groups[0], "admin");
  ASSERT_EQ(groups[1], "dev");
  ASSERT_EQ(groups[2], "qa");
  ASSERT_EQ(cmd.getCapturedCount(userCommandPath, "groups"), 3);
  ASSERT_TRUE(cmd.hasCapturedParam(userCommandPath, "groups"));
  ASSERT_TRUE(err.empty());
}

// Test empty multi-value parameter
TEST(ParameterTypes, EmptyMultipleValuesParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Bob"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  auto groups = cmd.getCapturedValues<std::string>(userCommandPath, "groups");
  ASSERT_TRUE(groups.empty());
  ASSERT_EQ(cmd.getCapturedCount(userCommandPath, "groups"), 0);
  ASSERT_FALSE(cmd.hasCapturedParam(userCommandPath, "groups"));
  ASSERT_TRUE(err.empty());
}

// Integer parameter
TEST(ParameterTypes, IntegerParameterTypes) {
  // Test with UserCmd
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Bob", "--age", "30"});
  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<int>(userCommandPath, "age").value_or(0), 30);
  ASSERT_TRUE(cmd.hasCapturedParam(userCommandPath, "age"));
  ASSERT_TRUE(err.empty());
  
  // Test with ServerCmd
  AppCmd cmd2;
  const auto [exitCode2, err2] = ProcessWithCapturedStderr(cmd2, {"server", "start", "--workers", "4", "--port", "8080"});
  ASSERT_EQ(exitCode2, EXIT_SUCCESS);
  ASSERT_EQ(cmd2.getCapturedValue<int>(serverStartCommandPath, "workers").value_or(0), 4);
  ASSERT_EQ(cmd2.getCapturedValue<int>(serverStartCommandPath, "port").value_or(0), 8080);
  ASSERT_TRUE(cmd2.hasCapturedParam(serverStartCommandPath, "workers"));
  ASSERT_TRUE(cmd2.hasCapturedParam(serverStartCommandPath, "port"));
  ASSERT_TRUE(err2.empty());
}

// Mixed parameter types
TEST(ParameterTypes, MixedParameterTypes) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Charlie", "--age", "25", "--groups", "admin", "--groups", "users"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Charlie");
  ASSERT_EQ(cmd.getCapturedValue<int>(userCommandPath, "age").value_or(0), 25);
  auto groups = cmd.getCapturedValues<std::string>(userCommandPath, "groups");
  ASSERT_EQ(groups.size(), 2);
  ASSERT_EQ(groups[0], "admin");
  ASSERT_EQ(groups[1], "users");
  ASSERT_EQ(cmd.getCapturedCount(userCommandPath, "name"), 1);
  ASSERT_EQ(cmd.getCapturedCount(userCommandPath, "age"), 1);
  ASSERT_EQ(cmd.getCapturedCount(userCommandPath, "groups"), 2);
  ASSERT_TRUE(cmd.hasCapturedParam(userCommandPath, "name"));
  ASSERT_TRUE(cmd.hasCapturedParam(userCommandPath, "age"));
  ASSERT_TRUE(cmd.hasCapturedParam(userCommandPath, "groups"));
  ASSERT_TRUE(err.empty());
}

// Tests from table 1: Command-Parameter combinations

// [1-1]: Normal Cmd + Normal Par
// Trivial case
TEST(CommandParameterCombinations, NormalCommandWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Alice"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Alice");
  ASSERT_TRUE(err.empty());
}

// [1-2]: Normal Cmd + Depr Par
// Show deprecation warning
TEST(CommandParameterCombinations, NormalCommandWithDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--old-email", "user@domain.com"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  // Verify the old-email parameter was captured
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-email").value_or(""), "user@domain.com");
  ASSERT_NE(err.find("Warning: '--old-email' is deprecated. Use --email instead."), std::string::npos);
}

// [1-3]: Normal Cmd + Alias Par
// Forward to new command/parameter
TEST(CommandParameterCombinations, NormalCommandWithAliasParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--mail", "test@example.com"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "test@example.com");
  EXPECT_TRUE(err.empty());
}

// [1-4]: Normal Cmd + Forwarding Depr Par
// Show deprecation warning, forward to new command/parameter
TEST(CommandParameterCombinations, RenamesParameterAndWarns) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--username", "john@example.com"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "john@example.com");
  ASSERT_NE(err.find("Warning: '--username' is deprecated. Use --email instead."), std::string::npos);
}

// [1-5]: Normal Cmd + NoLongSupp Par
// Fail with error about no longer supported parameter
TEST(CommandParameterCombinations, NoLongerSupportedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--role", "admin"});

  ASSERT_EQ(exitCode, EXIT_FAILURE);
  ASSERT_NE(err.find("Error: The parameter '--role' is no longer supported. Use --name instead."), std::string::npos);
}

// [1-6]: Depr Cmd + Normal Par
// Show deprecation warning
TEST(CommandParameterCombinations, DeprecatedCommandWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"deploy", "--name", "production"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(deployCommandPath, "name").value_or(""), "production");
  ASSERT_NE(err.find("Warning: The command 'deploy' is deprecated. Use 'user' instead."), std::string::npos);
}

// [1-7]: Depr Cmd + Depr Par
// Show two deprecation warnings
TEST(CommandParameterCombinations, SelfDeprecatedCommandWithParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"deploy", "--old-name", "staging"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(deployCommandPath, "old-name").value_or(""), "staging");
  ASSERT_NE(err.find("Warning: The command 'deploy' is deprecated."), std::string::npos);
  ASSERT_NE(err.find("Warning: '--old-name' is deprecated."), std::string::npos);
}

// [1-8]: Depr Cmd + Alias Par
// Show command deprecation warning, forward to new command
TEST(CommandParameterCombinations, DeprecatedCommandWithAliasParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"deploy", "--deploy-alias", "test-deployment"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(deployCommandPath, "name").value_or(""), "test-deployment");
  ASSERT_NE(err.find("Warning: The command 'deploy' is deprecated."), std::string::npos);
  ASSERT_EQ(err.find("'--deploy-alias'"), std::string::npos);
}

// [1-9]: Depr Cmd + Forwarding Depr Par
// Show two deprecation warnings, forward to new command/parameter
TEST(CommandParameterCombinations, DeprecatedCommandWithForwardingParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"deploy", "--deployment-name", "staging"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(deployCommandPath, "name").value_or(""), "staging");
  ASSERT_NE(err.find("Warning: The command 'deploy' is deprecated."), std::string::npos);
  ASSERT_NE(err.find("Warning: '--deployment-name' is deprecated. Use --name instead."), std::string::npos);
}

// [1-10]: Depr Cmd + NoLongSupp Par
// Fail with error about no longer supported parameter
TEST(CommandParameterCombinations, DeprecatedCommandWithNoLongerSupportedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"deploy", "--role", "admin"});

  EXPECT_EQ(exitCode, EXIT_FAILURE);
  EXPECT_NE(err.find("Error: The parameter '--role' is no longer supported."), std::string::npos);
}

// [1-11]: Alias Cmd + Normal Par
// Forward to new command
TEST(CommandParameterCombinations, AliasCommandWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-user", "--name", "Alice"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Alice");
  ASSERT_TRUE(err.empty());
}

// [1-12]: Alias Cmd + Depr Par
// Show deprecation warning, forward to new command
TEST(CommandParameterCombinations, AliasCommandWithDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-user", "--old-email", "test@example.com"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-email").value_or(""), "test@example.com");
  ASSERT_NE(err.find("Warning: '--old-email' is deprecated."), std::string::npos);
  ASSERT_EQ(err.find("create-user"), std::string::npos);
}

// [1-13]: Alias Cmd + Alias Par
// Only allow parameter transformations that dont forward to a different command
TEST(CommandParameterCombinations, AliasCommandWithAliasParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-user", "--mail", "test@example.com"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "test@example.com");
  EXPECT_TRUE(err.empty());
}

// [1-14]: Alias Cmd + Forwarding Depr Par
// Show deprecation warning, only allow parameter transformations that dont forward to a different command
TEST(CommandParameterCombinations, AliasCommandWithForwardingDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-user", "--username", "user@example.com"});
    
  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "user@example.com");
  EXPECT_NE(err.find("Warning: '--username' is deprecated. Use --email instead."), std::string::npos);
  EXPECT_EQ(err.find("create-user"), std::string::npos);
}

// [1-15]: Alias Cmd + NoLongSupp Par
// Fail with error about no longer supported parameter
TEST(CommandParameterCombinations, AliasCommandWithNoLongerSupportedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-user", "--role", "admin"});

  EXPECT_EQ(exitCode, EXIT_FAILURE);
  EXPECT_EQ(err.find("create-user"), std::string::npos);
  EXPECT_NE(err.find("Error: The parameter '--role' is no longer supported."), std::string::npos);
}

// [1-16]: Forwarding Depr Cmd + Normal Par
// Show deprecation warning, forward to new command/parameter
TEST(CommandParameterCombinations, ForwardingDeprecatedCommandWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"init-user", "--name", "Bob"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Bob");
  ASSERT_NE(err.find("Warning: The command 'init-user' is deprecated. Use 'user' instead."), std::string::npos);
}

// [1-17]: Forwarding Depr Cmd + Depr Par
// Show two deprecation warnings, forward to new command/parameter
TEST(CommandParameterCombinations, ForwardingDeprecatedCommandWithDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"init-user", "--old-email", "user@example.com"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-email").value_or(""), "user@example.com");
  ASSERT_NE(err.find("Warning: The command 'init-user' is deprecated."), std::string::npos);
  ASSERT_NE(err.find("Warning: '--old-email' is deprecated."), std::string::npos);
}


// [1-18]: Forwarding Depr Cmd + Alias Par
// Show deprecation warning, only allow parameter transformations that dont forward to a different command
TEST(CommandParameterCombinations, ForwardingDeprecatedCommandWithAliasParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"init-user", "--mail", "test@example.com"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "test@example.com");
  EXPECT_NE(err.find("Warning: The command 'init-user' is deprecated."), std::string::npos);
  EXPECT_EQ(err.find("mail"), std::string::npos);
}

// [1-19]: Forwarding Depr Cmd + Forwarding Depr Par
// Show two deprecation warnings, only allow parameter transformations that dont forward to a different command
TEST(CommandParameterCombinations, ForwardingDeprecatedCommandWithForwardingParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"init-user", "--username", "test@example.com"});
  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_NE(err.find("Warning: '--username' is deprecated. Use --email instead."), std::string::npos);
  ASSERT_NE(err.find("Warning: The command 'init-user' is deprecated. Use 'user' instead."), std::string::npos);
}

// [1-20]: Forwarding Depr Cmd + NoLongSupp Par
// Show command deprecation warning, fail with error about no longer supported parameter
TEST(CommandParameterCombinations, ForwardingDeprecatedCommandWithNoLongerSupportedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"init-user", "--role", "admin"});

  ASSERT_EQ(exitCode, EXIT_FAILURE) << "Should fail when using removed parameter, even through deprecated command forwarding";
  ASSERT_NE(err.find("Warning: The command 'init-user' is deprecated."), std::string::npos);
  ASSERT_NE(err.find("Error: The parameter '--role' is no longer supported."), std::string::npos);
}

// [1-21]: NoLongSupp Cmd + Normal Par
// Fail with error about no longer supported command
TEST(CommandParameterCombinations, NoLongerSupportedCommandWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"old-user", "ignored-argument"});

  ASSERT_EQ(exitCode, EXIT_FAILURE);
  ASSERT_NE(err.find("Error: The command 'old-user' is no longer supported. Use 'user' instead."), std::string::npos);
}

// [1-22]: NoLongSupp Cmd + Depr Par
// Fail with error about no longer supported command
TEST(CommandParameterCombinations, NoLongerSupportedCommandWithDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"old-user", "--old-email", "test@example.com"});

  EXPECT_EQ(exitCode, EXIT_FAILURE);
  EXPECT_NE(err.find("Error: The command 'old-user' is no longer supported."), std::string::npos);
  EXPECT_EQ(err.find("'--old-email'"), std::string::npos);
}

// [1-23]: NoLongSupp Cmd + Alias Par
// Fail with error about no longer supported command
TEST(CommandParameterCombinations, NoLongerSupportedCommandWithAliasParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"old-user", "--mail", "test@example.com"});

  EXPECT_EQ(exitCode, EXIT_FAILURE);
  EXPECT_NE(err.find("Error: The command 'old-user' is no longer supported."), std::string::npos);
  EXPECT_EQ(err.find("mail"), std::string::npos);
}

// [1-24]: NoLongSupp Cmd + Forwarding Depr Par
// Fail with error about no longer supported command
TEST(CommandParameterCombinations, NoLongerSupportedCommandWithForwardingDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"old-user", "--username", "test@example.com"});

  EXPECT_EQ(exitCode, EXIT_FAILURE);
  EXPECT_NE(err.find("Error: The command 'old-user' is no longer supported."), std::string::npos);
  EXPECT_EQ(err.find("'--username'"), std::string::npos);
}

// [1-25]: NoLongSupp Cmd + NoLongSupp Par
// Fail with error about no longer supported command
TEST(CommandParameterCombinations, NoLongerSupportedCommandWithNoLongerSupportedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"old-user", "--role", "admin"});

  EXPECT_EQ(exitCode, EXIT_FAILURE);
  EXPECT_NE(err.find("Error: The command 'old-user' is no longer supported."), std::string::npos);
  EXPECT_EQ(err.find("'--role'"), std::string::npos);
}

// Tests from table 2: Parameter-Parameter combinations

// [2-1]: Normal Par + Normal Par
// Trivial case
TEST(ParameterParameterCombinations, NormalParameterWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Alice", "--email", "alice@example.com"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "alice@example.com");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Alice");
  EXPECT_TRUE(cmd.hasCapturedParam(userCommandPath, "email"));
  EXPECT_TRUE(cmd.hasCapturedParam(userCommandPath, "name"));
  EXPECT_EQ(cmd.getCapturedCount(userCommandPath, "email"), 1);
  EXPECT_EQ(cmd.getCapturedCount(userCommandPath, "name"), 1);
  EXPECT_TRUE(err.empty());
}

// [2-2]: Depr Par + Normal Par
// Show deprecation warning for Par 1
TEST(ParameterParameterCombinations, DeprecatedParameterWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Alice", "--old-email", "alice@example.com"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_TRUE(cmd.hasCapturedParam(userCommandPath, "old-email"));
  EXPECT_TRUE(cmd.hasCapturedParam(userCommandPath, "name"));
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-email").value_or(""), "alice@example.com");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Alice");
  EXPECT_NE(err.find("Warning: '--old-email' is deprecated."), std::string::npos);
}

// [2-3]: Depr Par + Depr Par
// Show deprecation warning for both Par 1 and Par 2
TEST(ParameterParameterCombinations, DeprecatedParameterWithDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--old-email", "old@example.com", "--old-name", "staging"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedCount(userCommandPath, "old-email"), 1);
  EXPECT_EQ(cmd.getCapturedCount(userCommandPath, "old-name"), 1);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-email").value_or(""), "old@example.com");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-name").value_or(""), "staging");
  EXPECT_NE(err.find("Warning: '--old-email' is deprecated."), std::string::npos);
  EXPECT_NE(err.find("Warning: '--old-name' is deprecated."), std::string::npos);
}

// [2-4]: Alias Par + Normal Par
// Forward/transform according to Par 1, forwarding should include Par 2
TEST(ParameterParameterCombinations, AliasParameterWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--mail", "test@example.com", "--name", "Test User"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "test@example.com");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Test User");
  EXPECT_TRUE(err.empty());
}

// [2-5]: Alias Par + Depr Par
// Show deprecation warning for Par 2, forward/transform according to Par 1, forwarding should include Par 2
TEST(ParameterParameterCombinations, AliasParameterWithDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--mail", "bob@example.com", "--old-name", "Bob"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "bob@example.com");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-name").value_or(""), "Bob");
  EXPECT_NE(err.find("Warning: '--old-name' is deprecated. Use --name instead."), std::string::npos);
}

// [2-6a]: Alias Par + Alias Par
// Multiple alias parameters: just one forwarding to a different command is fine
TEST(ParameterParameterCombinations, AliasParameterWithAliasParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"--quick-start", "8080", "--verbose-mode"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<int>(serverStartCommandPath, "port").value_or(0), 8080);
  EXPECT_TRUE(err.empty());
}

// X[2-6b]: Alias Par + Alias Par (multiple command forwards)
// Programmer error: Only ONE alias parameter can forward to a different command
TEST(ParameterParameterCombinations, AliasParameterWithAliasParameterToDifferentCommands) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"--quick-start", "8080", "--quick-deploy", "production"});
    cmd.process(args);
  }, ".*") << "Framework should assert when multiple alias parameters forward to commands";
}

// [2-7]: Forwarding Depr Par + Normal Par
// Show deprecation warning for Par 1, forward/transform according to Par 1, forwarding should include Par 2
TEST(ParameterParameterCombinations, ForwardingDeprecatedParameterWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Bob", "--username", "alice@example.com"});
  
  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "alice@example.com");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Bob");
  EXPECT_NE(err.find("Warning: '--username' is deprecated."), std::string::npos);
}

// [2-8]: Forwarding Depr Par + Depr Par
// Show deprecation warning for both Par 1 and Par 2, forward/transform according to Par 1, forwarding should include Par 2
TEST(ParameterParameterCombinations, ForwardingDeprecatedParameterWithDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--old-email", "old@example.com", "--username", "new@example.com"});
  
  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "new@example.com");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-email").value_or(""), "old@example.com");
  EXPECT_NE(err.find("Warning: '--username' is deprecated."), std::string::npos);
  EXPECT_NE(err.find("Warning: '--old-email' is deprecated."), std::string::npos);
}

// [2-9]: Forwarding Depr Par + Alias Par
// Show deprecation warning for Par 1, only one Alias Par allowed to forward to different command
TEST(ParameterParameterCombinations, ForwardingDeprecatedParameterWithAliasParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--legacy-name", "Bob", "--mail", "bob@example.com"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Bob");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "bob@example.com");
  EXPECT_NE(err.find("Warning: '--legacy-name' is deprecated. Use --name instead."), std::string::npos);
}

// [2-10]: Forwarding Depr Par + Forwarding Depr Par
// Show deprecation warning for both Par 1 and Par 2, only allow 1 parameter transformations that forwards to a different command
TEST(ParameterParameterCombinations, ForwardingDeprecatedParameterWithForwardingDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--username", "bob@example.com", "--legacy-name", "Bob"});
  
  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "bob@example.com");
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "name").value_or(""), "Bob");
  EXPECT_NE(err.find("Warning: '--username' is deprecated."), std::string::npos);
  EXPECT_NE(err.find("Warning: '--legacy-name' is deprecated."), std::string::npos);
}

// [2-11]: NoLongSupp Par + Normal Par
// Fail with error about no longer supported parameter
TEST(ParameterParameterCombinations, NoLongerSupportedParameterWithNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--name", "Bob", "--role", "admin"});

  EXPECT_EQ(exitCode, EXIT_FAILURE) << "Should error on no-longer-supported parameter even when combined with normal parameter";
  EXPECT_NE(err.find("Error: The parameter '--role' is no longer supported."), std::string::npos);
}

// [2-12]: NoLongSupp Par + Depr Par
// Fail with error about no longer supported parameter
TEST(ParameterParameterCombinations, NoLongerSupportedParameterWithDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--old-email", "old@example.com", "--role", "admin"});

  EXPECT_EQ(exitCode, EXIT_FAILURE) << "Should error on no-longer-supported parameter even when combined with in-place deprecated parameter";
  EXPECT_NE(err.find("Error: The parameter '--role' is no longer supported."), std::string::npos);
}

// [2-13]: NoLongSupp Par + Alias Par
// Fail with error about no longer supported parameter
TEST(ParameterParameterCombinations, NoLongerSupportedParameterWithAliasParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--role", "admin", "--mail", "test@example.com"});

  EXPECT_EQ(exitCode, EXIT_FAILURE);
  EXPECT_NE(err.find("Error: The parameter '--role' is no longer supported."), std::string::npos);
}

// [2-14]: NoLongSupp Par + Forwarding Depr Par
// Fail with error about no longer supported parameter
TEST(ParameterParameterCombinations, NoLongerSupportedParameterWithForwardingDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--username", "alice@example.com", "--role", "admin"});

  EXPECT_EQ(exitCode, EXIT_FAILURE) << "Should error on no-longer-supported parameter even when combined with forwarding deprecated parameter";
  EXPECT_NE(err.find("Error: The parameter '--role' is no longer supported."), std::string::npos);
}

// [2-15]: NoLongSupp Par + NoLongSupp Par
// Should see error for at least one of the no-longer-supported parameters
TEST(ParameterParameterCombinations, NoLongerSupportedParameterWithNoLongerSupportedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--role", "admin", "--permission", "write"});

  EXPECT_EQ(exitCode, EXIT_FAILURE) << "Should fail when using no-longer-supported parameters together";
  EXPECT_TRUE(
    err.find("Error: The parameter '--role' is no longer supported.") != std::string::npos ||
    err.find("Error: The parameter '--permission' is no longer supported.") != std::string::npos
  );
}

// Tests from table 3: All command/parameter forwarding combinations

// [3-1]: Alias Cmd -> Normal Cmd
// Trivial Command Alias test
TEST(ForwardingCombinations, AliasCommandToNormalCommand) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-user"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_TRUE(err.empty());
}

// [3-2]: Alias Cmd -> Depr Cmd
// Alias, and show deprecation warning for the target command
TEST(ForwardingCombinations, AliasCommandToDeprecatedCommand) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-deploy"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_NE(err.find("Warning: The command 'deploy' is deprecated."), std::string::npos);
}

// X[3-3]: Alias Cmd -> Forwarding Depr Cmd
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, AliasCommandToForwardingDeprecatedCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"new-user"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias forwards to a deprecated command";
}

// X[3-4]: Alias Cmd -> Alias Cmd
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, AliasCommandToAliasCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"user-alias"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias forwards to another alias command";
}

// X[3-5]: Alias Cmd -> NoLongSupp Cmd
// Programmer error: Any forwarding to a no longer supported cmd/par should assert
TEST(ForwardingCombinations, AliasCommandToNoLongerSupportedCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"removed-alias"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias forwards to a no-longer-supported command";
}

// [3-6]: Alias Cmd -> Normal Par
// Trivial forwarding alias test
TEST(ForwardingCombinations, AliasCommandToNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-user-email", "test@example.com"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "test@example.com");
  EXPECT_TRUE(err.empty());
}

// [3-7]: Alias Cmd -> Depr Par
// Forward according to alias, show deprecation warning for the target parameter
TEST(ForwardingCombinations, AliasCommandToDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"create-user-old-email", "bob@example.com"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-email").value_or(""), "bob@example.com");
  EXPECT_EQ(err.find("create-user-old-email"), std::string::npos);
  EXPECT_NE(err.find("Warning: '--old-email' is deprecated."), std::string::npos);
}

// X[3-8]: Alias Cmd -> Alias Par
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, AliasCommandToAliasParameter) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"create-user-mail", "8080"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias command is combined with alias parameter that forwards to different command";
}

// X[3-9]: Alias Cmd -> Forwarding Depr Par
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, AliasCommandToForwardingDeprecatedParameter) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"create-user-fwd-username", "test@example.com"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias command is used with alias parameter that forwards to forwarding deprecated parameter";
}

// X[3-10]: Alias Cmd -> NoLongSupp Par
// Programmer error: Any forwarding to a no longer supported cmd/par should assert
TEST(ForwardingCombinations, AliasCommandToNoLongerSupportedParameter) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"create-user-removed-role", "admin"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias command is used with alias parameter that forwards to no-longer-supported parameter";
}

// [3-11]: Forwarding Depr Cmd -> Normal Cmd
// Show deprecation warning for the forwarding command, forward to command
TEST(ForwardingCombinations, ForwardingDeprecatedCommandToNormalCommand) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"init-user"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_NE(err.find("Warning: The command 'init-user' is deprecated. Use 'user' instead."), std::string::npos);
}

// [3-12]: Forwarding Depr Cmd -> Depr Cmd
// Show deprecation warning for the forwarding command, forward to command, show deprecation warning for target command
TEST(ForwardingCombinations, ForwardingDeprecatedCommandToDeprecatedCommand) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"init-deploy", "--name", "staging"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(deployCommandPath, "name").value_or(""), "staging");
  ASSERT_NE(err.find("Warning: The command 'init-deploy' is deprecated."), std::string::npos);
  ASSERT_NE(err.find("Warning: The command 'deploy' is deprecated."), std::string::npos);
}

// X[3-13]: Forwarding Depr Cmd -> Forwarding Depr Cmd
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, ForwardingDeprecatedCommandToForwardingDeprecatedCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"old-init", "--name", "ChainTest"});
    cmd.process(args);
  }, ".*") << "Framework should assert when deprecated command forwards to another deprecated command";
}

// X[3-14]: Forwarding Depr Cmd -> Alias Cmd
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, ForwardingDeprecatedCommandToAliasCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"old-db", "--source", "/data/test.db"});
    cmd.process(args);
  }, ".*") << "Framework should assert when deprecated command forwards to an alias command";
}

// X[3-15]: Forwarding Depr Cmd -> NoLongSupp Cmd
// Programmer error: Any forwarding to a no longer supported cmd/par should assert
TEST(ForwardingCombinations, ForwardingDeprecatedCommandToNoLongerSupportedCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"removed-init"});
    cmd.process(args);
  }, ".*") << "Framework should assert when deprecated command forwards to a no-longer-supported command";
}

// [3-16]: Alias Par -> Normal Cmd
// Trivial Parameter Alias test
TEST(ForwardingCombinations, AliasParameterToNormalCommand) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"--quick-start", "8080"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<int>(serverStartCommandPath, "port").value_or(0), 8080);
  EXPECT_TRUE(cmd.hasCapturedParam(serverStartCommandPath, "port"));
  EXPECT_EQ(cmd.getCapturedCount(serverStartCommandPath, "port"), 1);
  EXPECT_TRUE(err.empty());
}

// [3-17]: Alias Par -> Depr Cmd
// Alias, and show deprecation warning for the target command
TEST(ForwardingCombinations, AliasParameterToDeprecatedCommand) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"--quick-deploy", "production"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(deployCommandPath, "name").value_or(""), "production");
  EXPECT_EQ(err.find("quick-deploy"), std::string::npos); // Parameter doesn't warn (alias is silent)
  EXPECT_NE(err.find("Warning: The command 'deploy' is deprecated."), std::string::npos); // Command warns
}

// X[3-18]: Alias Par -> Forwarding Depr Cmd
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, AliasParameterToForwardingDeprecatedCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"--bad-forward-depr-cmd", "test"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias parameter forwards to a forwarding deprecated command";
}

// X[3-19]: Alias Par -> Alias Cmd
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, AliasParameterToAliasCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"--bad-alias-cmd", "test"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias parameter forwards to an alias command";
}

// X[3-20]: Alias Par -> NoLongSupp Cmd
// Programmer error: Any forwarding to a no longer supported cmd/par should assert
TEST(ForwardingCombinations, AliasParameterToNoLongerSupportedCommand) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"--bad-removed-cmd", "value"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias parameter forwards to a no-longer-supported command";
}

// [3-21]: Alias Par -> Normal Par
// Trivial Parameter Alias test
TEST(ForwardingCombinations, AliasParameterToNormalParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--mail", "test@example.com"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "email").value_or(""), "test@example.com");
  EXPECT_TRUE(err.empty());
}

// [3-22]: Alias Par -> Depr Par
// Alias, and show deprecation warning for the target parameter
TEST(ForwardingCombinations, AliasParameterToDeprecatedParameter) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"user", "--old-mail", "old@example.com"});

  EXPECT_EQ(exitCode, EXIT_SUCCESS);
  EXPECT_EQ(cmd.getCapturedValue<std::string>(userCommandPath, "old-email").value_or(""), "old@example.com");
  EXPECT_EQ(err.find("old-mail"), std::string::npos);
  EXPECT_NE(err.find("Warning: '--old-email' is deprecated."), std::string::npos);
}

// X[3-23]: Alias Par -> Alias Par
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, AliasParameterToAliasParameter) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"user", "--chain-mail", "test@example.com"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias parameter forwards to another alias parameter (creating chain)";
}

// X[3-24]: Alias Par -> Forwarding Depr Par
// Programmer error: Any forwarding to a forwarding cmd/par should assert
TEST(ForwardingCombinations, AliasParameterToForwardingDeprecatedParameter) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"user", "--forward-username", "test@example.com"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias parameter forwards to a forwarding deprecated parameter";
}

// X[3-25]: Alias Par -> NoLongSupp Par
// Programmer error: Any forwarding to a no longer supported cmd/par should assert
TEST(ForwardingCombinations, AliasParameterToNoLongerSupportedParameter) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"user", "--removed-role", "admin"});
    cmd.process(args);
  }, ".*") << "Framework should assert when alias parameter forwards to a no-longer-supported parameter";
}

// [3-26]: Forwarding Depr Par -> Normal Cmd
// Show deprecation warning for source parameter, forward to target command
TEST(ForwardingCombinations, ForwardingDeprecatedParameterToNormalCommand) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"--legacy-port", "8080"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<int>(serverStartCommandPath, "port").value_or(0), 8080);
  ASSERT_NE(err.find("Warning: '--legacy-port' is deprecated. Use 'server start --port' instead."), std::string::npos);
}

// [3-27]: Forwarding Depr Par -> Depr Cmd
// Show deprecation warning for source parameter, forward to target command and show its deprecation warning
TEST(ForwardingCombinations, ForwardingDeprecatedParameterToDeprecatedCommand) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"--deploy-name", "production"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(deployCommandPath, "name").value_or(""), "production");
  ASSERT_NE(err.find("Warning: '--deploy-name' is deprecated. Use 'deploy --name' instead."), std::string::npos);
  ASSERT_NE(err.find("Warning: The command 'deploy' is deprecated."), std::string::npos);
}

// Extra tests

// [4-1]: Value splitting
// We should see a deprecation warning for the deprecated parameter, and value host:port should be split to host and port parameters on the target command
TEST(ComplexForwarding, ValueSplittingViaDispatchTo) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"server", "start", "--host-port", "localhost:8080"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_NE(err.find("Warning: '--host-port' is deprecated."), std::string::npos);
  ASSERT_EQ(cmd.getCapturedValue<int>(serverStartCommandPath, "port").value_or(0), 8080);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(serverStartCommandPath, "host").value_or(""), "localhost");
  ASSERT_EQ(cmd.getCapturedCount(serverStartCommandPath, "port"), 1);
  ASSERT_EQ(cmd.getCapturedCount(serverStartCommandPath, "host"), 1);
}

// [4-3a]: Conditional parameter forwarding based on parameter value
// deprecation warning + parameter value should be forwarded to the correct target command
TEST(ComplexForwarding, ConditionalRedirectToStartWithParameters) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"--mode", "start", "--port", "9000"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_NE(err.find("Warning: '--mode' is deprecated."), std::string::npos);
  ASSERT_EQ(cmd.getCapturedValue<int>(serverStartCommandPath, "port").value_or(0), 9000);
}

// [4-3b]: Conditional parameter forwarding based on parameter value
// deprecation warning + parameter value should be forwarded to the correct target command
TEST(ComplexForwarding, ConditionalRedirectToDeployWithParameters) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"--mode", "deploy", "--name", "production"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_NE(err.find("Warning: '--mode' is deprecated."), std::string::npos);
  ASSERT_EQ(cmd.getCapturedValue<std::string>(deployCommandPath, "name").value_or(""), "production");
}

// [4-3]: Parameter availability
// Check root parameters stay available after forwarding to a subcommand
TEST(ComplexForwarding, RootParametersAvailableAfterSubcommandForward) {
  AppCmd cmd;
  const auto [exitCode, err] = ProcessWithCapturedStderr(cmd, {"--global-param", "global-value", "server", "start", "--port", "7000"});

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_TRUE(err.empty());
  ASSERT_EQ(cmd.getCapturedValue<std::string>(appCommandPath, "global-param").value_or(""), "global-value");
  ASSERT_EQ(cmd.getCapturedCount(appCommandPath, "global-param"), 1);
  ASSERT_EQ(cmd.getCapturedValue<int>(serverStartCommandPath, "port").value_or(0), 7000);
  ASSERT_EQ(cmd.getCapturedCount(serverStartCommandPath, "port"), 1);
}

// [4-4]: Bad forwarding
// Programmer error: transformed dispatch path must point to an existing subcommand chain
TEST(ComplexForwarding, InvalidTransformationChildPathAsserts) {
  EXPECT_DEBUG_DEATH({
    AppCmd cmd;
    auto args = ToQueue({"--bad-transform-path", "value"});
    cmd.process(args);
  }, ".*") << "Framework should assert when a parameter transformation provides an invalid child path.";
}
