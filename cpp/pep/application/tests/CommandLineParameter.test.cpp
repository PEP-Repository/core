#include <gtest/gtest.h>

#include <pep/application/CommandLineCommand.hpp>
#include <pep/application/CommandLineCommandWrappers.hpp>

#include <queue>
#include <string>
#include <vector>

namespace {

std::queue<std::string> ToQueue(std::initializer_list<std::string> values) {
  std::queue<std::string> result;
  for (const auto& value : values) {
    result.push(value);
  }
  return result;
}

// ---- Shared command tree used by all tests ----
//
//  RootCmd  (--old-val → "subcmd-c subsubcmd-c1 --value")
//  ├── subcmd-a  (--value, --new-param, --old-param → --new-param, --warned-param in-place deprecated, --gone no-longer-supported)
//  ├── subcmd-b  (--value  required)
//  ├── subcmd-c
//  │   └── subsubcmd-c1  (--value)
//  ├── alias-a    → subcmd-a
//  ├── alias-b    → subcmd-b
//  ├── alias-c1   → subcmd-c subsubcmd-c1
//  ├── dep-a      → subcmd-a              (warns)
//  ├── dep-c1     → subcmd-c subsubcmd-c1 (warns)
//  ├── dep-self                            (warns, executes normally)
//  └── removed                            (no longer supported)

class RootCmd;
class SubcmdC;
class DeprecatedCmdSelf;

class SubcmdA : public pep::commandline::ChildCommandOf<RootCmd> {
public:
  explicit SubcmdA(RootCmd& parent);
  pep::commandline::Parameters getSupportedParameters() const override;
  int execute() override;
};

class SubcmdB : public pep::commandline::ChildCommandOf<RootCmd> {
public:
  explicit SubcmdB(RootCmd& parent);
  pep::commandline::Parameters getSupportedParameters() const override;
  int execute() override;
};

class DeprecatedCmdSelf : public pep::commandline::DeprecatedChildCommandOf<RootCmd> {
public:
  explicit DeprecatedCmdSelf(RootCmd& parent)
    : pep::commandline::DeprecatedChildCommandOf<RootCmd>("dep-self", "Deprecated self command.", parent, "Use 'subcmd-a' instead.") {}
  pep::commandline::Parameters getSupportedParameters() const override;
  int execute() override;
};

class SubSubCmdC1 : public pep::commandline::ChildCommandOf<SubcmdC> {
public:
  explicit SubSubCmdC1(SubcmdC& parent, RootCmd& root);
  pep::commandline::Parameters getSupportedParameters() const override;
  int execute() override;
private:
  RootCmd& mRoot;
};

class SubcmdC : public pep::commandline::ChildCommandOf<RootCmd> {
public:
  explicit SubcmdC(RootCmd& parent)
    : pep::commandline::ChildCommandOf<RootCmd>("subcmd-c", "Subcmd C", parent) {}

  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
    return { std::make_shared<SubSubCmdC1>(*this, getParent()) };
  }
};

class RootCmd : public pep::commandline::Command {
public:
  std::string getName() const override { return "root-cmd"; }
  std::string getDescription() const override { return "Shared test fixture root."; }

  pep::commandline::Parameters getSupportedParameters() const override {
    return pep::commandline::Command::getSupportedParameters()
      + pep::commandline::Parameter("old-val", std::nullopt).value(pep::commandline::Value<std::string>())
          .deprecated("Use 'subcmd-c subsubcmd-c1 --value' instead.", [](pep::commandline::Command&, const pep::commandline::NamedValues& vals) {
            pep::commandline::NamedValues toAdd;
            toAdd["value"] = vals["old-val"];
            return pep::commandline::ParameterDeprecationResult{std::move(toAdd), nullptr, "subcmd-c subsubcmd-c1"};
          });
  }

  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
    return {
      std::make_shared<SubcmdA>(*this),
      std::make_shared<SubcmdB>(*this),
      std::make_shared<SubcmdC>(*this),
      pep::commandline::CreateAliasCommand(*this, "alias-a", *this, "subcmd-a"),
      pep::commandline::CreateAliasCommand(*this, "alias-b", *this, "subcmd-b"),
      pep::commandline::CreateAliasCommand(*this, "alias-c1", *this, "subcmd-c subsubcmd-c1"),
      pep::commandline::CreateDeprecatedCommand(*this, "dep-a", *this, "subcmd-a", "Use 'subcmd-a' instead."),
      pep::commandline::CreateDeprecatedCommand(*this, "dep-c1", *this, "subcmd-c subsubcmd-c1", "Use 'subcmd-c subsubcmd-c1' instead."),
      pep::commandline::CreateNoLongerSupportedCommand(*this, "removed", "Use 'subcmd-a' instead."),
      std::make_shared<DeprecatedCmdSelf>(*this),
    };
  }

  int execute() override { return EXIT_SUCCESS; }

  void recordSubcmdInvocation(std::optional<std::string> value) { ++mSubcmdInvocations; mCaptured = std::move(value); }
  void recordC1Invocation(std::optional<std::string> value)     { ++mC1Invocations;     mCaptured = std::move(value); }

  int subcmdInvocations() const noexcept { return mSubcmdInvocations; }
  int c1Invocations()     const noexcept { return mC1Invocations; }
  const std::optional<std::string>& captured() const noexcept { return mCaptured; }

private:
  int mSubcmdInvocations = 0;
  int mC1Invocations     = 0;
  std::optional<std::string> mCaptured;
};

SubcmdA::SubcmdA(RootCmd& parent)
  : pep::commandline::ChildCommandOf<RootCmd>("subcmd-a", "Subcmd A", parent) {}

pep::commandline::Parameters SubcmdA::getSupportedParameters() const {
  return pep::commandline::ChildCommandOf<RootCmd>::getSupportedParameters()
    + pep::commandline::Parameter("value", "Value").value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("new-param", "New parameter").value(pep::commandline::Value<std::string>())
    + pep::commandline::Parameter("old-param", std::nullopt).value(pep::commandline::Value<std::string>())
        .deprecated("new-param", "Use --new-param instead.")
    + pep::commandline::Parameter("warned-param", std::nullopt).value(pep::commandline::Value<std::string>())
        .deprecated("Use --new-param instead.")
    + pep::commandline::Parameter("gone", std::nullopt).value(pep::commandline::Value<std::string>())
        .noLongerSupported("Use --value instead.");
}

int SubcmdA::execute() {
  auto val = this->getParameterValues().getOptional<std::string>("new-param");
  if (!val) val = this->getParameterValues().getOptional<std::string>("warned-param");
  if (!val) val = this->getParameterValues().getOptional<std::string>("value");
  this->getParent().recordSubcmdInvocation(std::move(val));
  return EXIT_SUCCESS;
}

SubcmdB::SubcmdB(RootCmd& parent)
  : pep::commandline::ChildCommandOf<RootCmd>("subcmd-b", "Subcmd B", parent) {}

pep::commandline::Parameters SubcmdB::getSupportedParameters() const {
  return pep::commandline::ChildCommandOf<RootCmd>::getSupportedParameters()
    + pep::commandline::Parameter("value", "Required value").value(pep::commandline::Value<std::string>().required());
}

int SubcmdB::execute() {
  this->getParent().recordSubcmdInvocation(this->getParameterValues().getOptional<std::string>("value"));
  return EXIT_SUCCESS;
}

SubSubCmdC1::SubSubCmdC1(SubcmdC& parent, RootCmd& root)
  : pep::commandline::ChildCommandOf<SubcmdC>("subsubcmd-c1", "SubSubCmd C1", parent), mRoot(root) {}

pep::commandline::Parameters SubSubCmdC1::getSupportedParameters() const {
  return pep::commandline::ChildCommandOf<SubcmdC>::getSupportedParameters()
    + pep::commandline::Parameter("value", "Value").value(pep::commandline::Value<std::string>());
}

int SubSubCmdC1::execute() {
  mRoot.recordC1Invocation(this->getParameterValues().getOptional<std::string>("value"));
  return EXIT_SUCCESS;
}

pep::commandline::Parameters DeprecatedCmdSelf::getSupportedParameters() const {
  return pep::commandline::DeprecatedChildCommandOf<RootCmd>::getSupportedParameters()
    + pep::commandline::Parameter("value", "Value").value(pep::commandline::Value<std::string>());
}

int DeprecatedCmdSelf::execute() {
  this->getParent().recordSubcmdInvocation(this->getParameterValues().getOptional<std::string>("value"));
  return EXIT_SUCCESS;
}

// ---- Tests ----

TEST(ParameterDeprecation, RenamesParameterAndWarns) {
  RootCmd cmd;
  auto args = ToQueue({"subcmd-a", "--old-param", "value-from-old"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.subcmdInvocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "value-from-old");
  ASSERT_NE(err.find("Warning: '--old-param' is deprecated. Use --new-param instead."), std::string::npos);
}

TEST(ParameterDeprecation, CurrentParameterDoesNotWarn) {
  RootCmd cmd;
  auto args = ToQueue({"subcmd-a", "--new-param", "value-from-new"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.subcmdInvocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "value-from-new");
  ASSERT_TRUE(err.empty());
}

TEST(ParameterDeprecation, RedirectsToNestedCommandAndWarns) {
  RootCmd cmd;
  auto args = ToQueue({"--old-val", "nested-val"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.c1Invocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "nested-val");
  ASSERT_NE(err.find("Warning: '--old-val' is deprecated. Use 'subcmd-c subsubcmd-c1 --value' instead."), std::string::npos);
}

TEST(CommandAliasing, ForwardsWithoutWarning) {
  RootCmd cmd;
  auto args = ToQueue({"alias-a", "--value", "value-from-alias"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.subcmdInvocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "value-from-alias");
  ASSERT_TRUE(err.empty());
}

TEST(CommandDeprecation, WarnsAndForwards) {
  RootCmd cmd;
  auto args = ToQueue({"dep-a", "--value", "value-from-dep"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.subcmdInvocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "value-from-dep");
  ASSERT_NE(err.find("Warning: The command 'dep-a' is deprecated. Use 'subcmd-a' instead."), std::string::npos);
}

TEST(CommandNoLongerSupported, FailsWithError) {
  RootCmd cmd;
  auto args = ToQueue({"removed", "ignored-argument"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_FAILURE);
  ASSERT_EQ(cmd.subcmdInvocations(), 0);
  ASSERT_FALSE(cmd.captured().has_value());
  ASSERT_NE(err.find("Error: The command 'removed' is no longer supported. Use 'subcmd-a' instead."), std::string::npos);
}

TEST(CommandAliasing, ForwardsToDeepDescendantWithoutWarning) {
  RootCmd cmd;
  auto args = ToQueue({"alias-c1", "--value", "deep-alias-val"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.c1Invocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "deep-alias-val");
  ASSERT_TRUE(err.empty());
}

TEST(CommandDeprecation, WarnsAndForwardsToDeepDescendant) {
  RootCmd cmd;
  auto args = ToQueue({"dep-c1", "--value", "deep-dep-val"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.c1Invocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "deep-dep-val");
  ASSERT_NE(err.find("Warning: The command 'dep-c1' is deprecated. Use 'subcmd-c subsubcmd-c1' instead."), std::string::npos);
}

TEST(CommandAliasing, ForwardsRequiredNamedParameter) {
  RootCmd cmd;
  auto args = ToQueue({"alias-b", "--value", "required-val"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.subcmdInvocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "required-val");
  ASSERT_TRUE(err.empty());
}

TEST(CommandAliasing, FailsWhenRequiredNamedParameterMissing) {
  RootCmd cmd;
  auto args = ToQueue({"alias-b"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  testing::internal::GetCapturedStderr(); // discard

  ASSERT_EQ(exitCode, EXIT_FAILURE);
  ASSERT_EQ(cmd.subcmdInvocations(), 0);
}

TEST(ParameterDeprecation, InPlaceWarnsAndKeepsValue) {
  RootCmd cmd;
  auto args = ToQueue({"subcmd-a", "--warned-param", "warned-val"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.subcmdInvocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "warned-val");
  ASSERT_NE(err.find("Warning: '--warned-param' is deprecated. Use --new-param instead."), std::string::npos);
}

TEST(CommandDeprecation, WarnsAndExecutesNormally) {
  RootCmd cmd;
  auto args = ToQueue({"dep-self", "--value", "dep-self-val"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_SUCCESS);
  ASSERT_EQ(cmd.subcmdInvocations(), 1);
  ASSERT_TRUE(cmd.captured().has_value());
  ASSERT_EQ(*cmd.captured(), "dep-self-val");
  ASSERT_NE(err.find("Warning: The command 'dep-self' is deprecated. Use 'subcmd-a' instead."), std::string::npos);
}

TEST(ParameterNoLongerSupported, FailsWithError) {
  RootCmd cmd;
  auto args = ToQueue({"subcmd-a", "--gone", "some-val"});

  testing::internal::CaptureStderr();
  const int exitCode = cmd.process(args);
  const std::string err = testing::internal::GetCapturedStderr();

  ASSERT_EQ(exitCode, EXIT_FAILURE);
  ASSERT_EQ(cmd.subcmdInvocations(), 0);
  ASSERT_NE(err.find("Error: The parameter '--gone' is no longer supported. Use --value instead."), std::string::npos);
}

} // namespace
