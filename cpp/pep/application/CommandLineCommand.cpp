#include <pep/application/Application.hpp>
#include <pep/application/CommandLineAutocomplete.hpp>
#include <pep/versioning/Version.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <vector>

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

namespace pep {
namespace commandline {

namespace {

std::string GetGlobWarning(const std::string& unrecognizedParameter) {
  if (!std::filesystem::exists(unrecognizedParameter)) {
    return std::string();
  }
  return "\n'" + unrecognizedParameter + "' is a file on your computer, indicating you may have tried to pass * as a parameter. If so, consider escaping the asterisk parameter by using \\* instead.";
}

}

int Command::issueCommandLineHelp(const std::optional<std::string>& error) {
  auto channel = Application::CreateNotificationChannel(error.has_value());
  auto& destination = channel->stream();

  struct InvocationLevel {
    std::string command;
    bool hasRequiredParameters;
  };
  std::vector<InvocationLevel> parents;
  for (auto parent = this->getParentCommand(); parent != nullptr; parent = parent->getParentCommand()) {
    parents.push_back({ parent->getName(), parent->getSupportedParameters().hasRequired() });
  }

  auto self = this->getName();
  auto fullSelf = self;
  for (auto parent : parents) {
    fullSelf = parent.command + ' ' + fullSelf;
  }

  std::reverse(parents.begin(), parents.end());

  auto parameters = this->getSupportedParameters();
  auto children = this->createChildCommands();
  assert(!parameters.empty()); // Should have at least the --help switch

  if (error.has_value()) {
    destination << fullSelf << ": invalid invocation: " << *error << '\n'
      << "See \"" << fullSelf << " --help\" for command line help." << std::endl;
    return EXIT_FAILURE;
  }

  destination << fullSelf << ": " << this->getDescription();
  auto additional = this->getAdditionalDescription();
  if (additional.has_value()) {
    destination << '\n' << *additional;
  }
  destination << std::endl;

  auto arguments = parameters.getInvocationSummary();
  if (!children.empty()) {
    char pre = '[', post = ']';
    if (std::all_of(children.cbegin(), children.cend(), [](std::shared_ptr<Command> child) {
      assert(!child->getSupportedParameters().empty()); // Should have at least the --help switch
      return child->hasRequiredArgument(); })) {
      pre = '<';
      post = '>';
    }
    arguments.push_back(std::string("<command> ") + pre + "..." + post);
  }
  if (!arguments.empty()) {
    destination << "\nUsage: ";
    for (auto parent : parents) {
      char pre = '[', post = ']';
      if (parent.hasRequiredParameters) {
        pre = '<';
        post = '>';
      }
      destination << parent.command << ' ' << pre << "..." << post << ' ';
    }
    destination << self << " " << boost::algorithm::join(arguments, " ") << std::endl;
  }

  if (!children.empty()) {
    destination << "\nCommands:\n";
    for (auto child : children) {
      if(!child->isUndocumented()) {
        WriteHelpItem(destination, child->getName(), static_cast<Command &>(*child).getDescription());
      }
    }
  }

  parameters.writeHelpText(destination);

  auto relative = this->getRelativeDocumentationUrl();
  if (relative != std::nullopt) {
    auto version = BinaryVersion::current.getSemver();
    // Note: this link will point to "release-X.Y" regardless of the branch for which the software was built, which has a multitude of problems
    // See #2734 for (some) details and a suggestion for improvement
    destination << "\nDocumentation for \"" << fullSelf << "\" is located at https://docs.pages.pep.cs.ru.nl/public/core/"
      << "release-" << version.getMajorVersion() << '.' << version.getMinorVersion()
      << "/user_documentation/" << *relative;
    destination << std::endl;
  }

  return EXIT_SUCCESS;
}

Parameters Command::getSupportedParameters() const {
  return Parameters()
    + Parameter("help", "Produce command line help and exit").shorthand('h')
    + Parameter("autocomplete", {});
}

int Command::execute() {
  throw std::runtime_error("Derived class should have overridden the \"execute\" method since it didn't produce child commands");
}

const NamedValues& Command::getParameterValues() const {
  if (!mParameterValues.has_value()) {
    throw std::runtime_error("Switch values cannot be obtained before a command line has been parsed");
  }
  return *mParameterValues;
}

bool Command::hasRequiredArgument() {
  if (this->getSupportedParameters().hasRequired()) {
    return true;
  }
  return !this->createChildCommands().empty();
}

std::optional<int> Command::processLexedParameters(const LexedValues& lexed) {
  assert(!mParametersLexed);
  if (lexed.find("help") != lexed.cend()) {
    return this->issueCommandLineHelp(std::nullopt);
  }
  mParametersLexed = true;
  return std::nullopt;
}

void Command::finalizeParameters() {
  assert(!mParametersFinalized); // Prevent this method from being invoked multiple times
  this->getSupportedParameters().finalize(*mParameterValues);
  mParametersFinalized = true;
}

int Command::dispatch(std::vector<std::shared_ptr<Command>> children, std::queue<std::string>& remaining) {
  assert(std::all_of(children.cbegin(), children.cend(), [this](const std::shared_ptr<Command>& child) { return child->getParentCommand() == this; }));
  if (!children.empty()) {
    if (remaining.empty()) {
      return this->issueCommandLineHelp("No command specified.");
    }
    std::string command = remaining.front();
    remaining.pop();
    auto child = std::find_if(children.cbegin(), children.cend(), [&command](const std::shared_ptr<Command>& child) {
      return child->getName() == command;
    });
    if (child == children.cend()) {
      return this->issueCommandLineHelp("Unsupported command '" + command + "' issued to " + this->getName() + GetGlobWarning(command));
    }
    return (*child)->process(remaining);
  }

  assert(remaining.empty()); // due to unsupported parameter check above
  return this->execute();
}

std::optional<int> Command::checkNoLongerSupportedParameters(const Parameters& parameters) {
  for (const auto& param : parameters) {
    if (param.isNoLongerSupported() && mParameterValues->has(param.getName())) {
      std::cerr << "Error: The parameter '--" << param.getName() << "' is no longer supported. " << *param.getNoLongerSupportedMessage() << std::endl;
      return EXIT_FAILURE;
    }
  }
  return std::nullopt;
}

std::optional<int> Command::applyParameterDeprecations(const Parameters& parameters) {
  Command* dispatchAncestor = nullptr;
  bool anyDeprecated = false;
  NamedValues mergedToAdd;
  std::string mergedChildPath;

  for (const auto& param : parameters) {
    if (!param.getDeprecationMessage().has_value() || !mParameterValues->has(param.getName())) {
      continue;
    }
    std::cerr << "Warning: '--" << param.getName() << "' is deprecated. " << *param.getDeprecationMessage() << std::endl;
    if (!param.hasDeprecationTransformer()) {
      continue;
    }
    auto depResult = param.transformDeprecated(*this, *mParameterValues);

    if (depResult.ancestor != nullptr) {
      assert((dispatchAncestor == nullptr || dispatchAncestor == depResult.ancestor)
        && "Multiple deprecated parameters specified conflicting dispatch ancestors \u2014 programmer error");
      dispatchAncestor = depResult.ancestor;
    }

    if (!depResult.childPath.empty()) {
      assert((mergedChildPath.empty() || mergedChildPath == depResult.childPath)
        && "Multiple deprecated parameters specified conflicting child paths \u2014 programmer error");
      mergedChildPath = depResult.childPath;
    }

    for (const auto& [key, vals] : depResult.toAdd) {
      mergedToAdd[key] = vals;
    }

    mParameterValues->erase(param.getName());
    anyDeprecated = true;
  }

  if (!anyDeprecated) {
    return std::nullopt;
  }

  Command* ancestor = (dispatchAncestor != nullptr) ? dispatchAncestor : this;
  NamedValues leafValues = *mParameterValues;
  for (const auto& [key, vals] : mergedToAdd) {
    leafValues[key] = vals;
  }
  return ancestor->dispatchTo(mergedChildPath, std::move(leafValues));
}

int Command::dispatchTo(const std::string& childPath, NamedValues leafValues, std::queue<std::string> leafArgs) {
  if (!childPath.empty()) {
    // Routing: finalize this level with existing values, then recurse into the named child.
    if (!mParameterValues.has_value()) {
      mParameterValues.emplace();
    }
    mParametersLexed = true;
    mParametersFinalized = false;
    try {
      this->finalizeParameters();
      assert(mParametersFinalized);
    }
    catch (const std::exception& error) {
      return this->issueCommandLineHelp(error.what());
    }

    auto children = this->createChildCommands();
    assert(std::all_of(children.cbegin(), children.cend(), [this](const std::shared_ptr<Command>& child) { return child->getParentCommand() == this; }));
    const auto pos = childPath.find(' ');
    const std::string childName = (pos == std::string::npos) ? childPath : childPath.substr(0, pos);
    const std::string remaining = (pos == std::string::npos) ? std::string{} : childPath.substr(pos + 1);
    auto it = std::find_if(children.cbegin(), children.cend(), [&childName](const std::shared_ptr<Command>& c) {
      return c->getName() == childName;
    });
    if (it == children.cend()) {
      return this->issueCommandLineHelp("Unsupported subcommand '" + childName + "' for " + this->getName());
    }
    return (*it)->dispatchTo(remaining, std::move(leafValues), std::move(leafArgs));
  }

  // Leaf: merge values, optionally lex extra args, finalize, execute.
  if (!mParameterValues.has_value()) {
    mParameterValues.emplace();
  }
  for (const auto& [key, vals] : leafValues) {
    (*mParameterValues)[key] = vals;
  }
  mParametersLexed = true;
  mParametersFinalized = false;

  if (!leafArgs.empty()) {
    try {
      auto leafParams = this->getSupportedParameters();
      auto lexed = leafParams.lex(leafArgs);
      auto parsed = leafParams.parse(lexed);
      for (const auto& [key, vals] : parsed) {
        (*mParameterValues)[key] = vals;
      }
    }
    catch (const std::exception& error) {
      return this->issueCommandLineHelp(error.what());
    }
  }
  try {
    this->finalizeParameters();
    assert(mParametersFinalized);
  }
  catch (const std::exception& error) {
    return this->issueCommandLineHelp(error.what());
  }
  return this->execute();
}

int Command::process(std::queue<std::string>& arguments) {
  auto children = this->createChildCommands();

  try {
    auto parameters = this->getSupportedParameters();
    auto argumentsCopy = arguments;
    auto lexed = parameters.lex(arguments);
    if (lexed.find("autocomplete") != lexed.end()) {
      return this->printAutocompleteInfo(argumentsCopy);
    }
    if (auto result = this->processLexedParameters(lexed)) {
      return *result;
    }
    assert(mParametersLexed);

    if (auto warning = this->getDeprecationWarning()) {
      std::cerr << "Warning: The command '" << this->getName() << "' is deprecated. " << *warning << std::endl;
    }

    // Report "unsupported parameter" if we received arguments that we can't pass to a child. See #2041
    if (children.empty() && !arguments.empty()) {
      return this->issueCommandLineHelp("Unrecognized command line parameter(s) issued to '" + this->getName() + "', starting with '" + arguments.front() + "'" + GetGlobWarning(arguments.front()));
    }

    // Apply defaults and check validity
    mParameterValues = parameters.parse(lexed);
    this->finalizeParameters();
    assert(mParametersFinalized);

    if (auto exitCode = this->checkNoLongerSupportedParameters(parameters)) {
      return *exitCode;
    }
    if (auto exitCode = this->applyParameterDeprecations(parameters)) {
      return *exitCode;
    }
  }
  catch (const std::exception& error) {
    return this->issueCommandLineHelp(error.what());
  }

  return this->dispatch(std::move(children), arguments);
}

int Command::printAutocompleteInfo(std::queue<std::string>& arguments) {
  const auto children = this->createChildCommands();

  const auto parameters = this->getSupportedParameters();
  bool terminated{};
  // Lex (possibly again)
  const auto lexed = parameters.lex(arguments, &terminated);

  if (!arguments.empty()) {
    // Not everything was lexed
    return this->autocompleteChildCommand(arguments);
  }

  // Everything was lexed

  Autocomplete complete;

  const Parameter* paramAcceptingValue = parameters.currentSwitchRequiringValue(lexed);
  // First complete child commands if we are done or no parameter accepts a value at this position
  const bool completeChildCommands = terminated || !paramAcceptingValue;
  if (completeChildCommands && !children.empty()) {
    complete.childCommands(children);
  }
  if (!terminated) {
    if (paramAcceptingValue) {
      // Complete current parameter value
      complete.parameterValues(*paramAcceptingValue);
    }
    else {
      if (auto positional = parameters.firstPositional(lexed)) {
        complete.parameterValues(*positional);
      }

      // Complete parameter switches
      auto completeParams = parameters.getSwitchesToAutocomplete(lexed);
      // Put required parameters first
      std::ranges::stable_sort(completeParams, std::greater{}, &Parameter::isRequired);
      complete.parameters(completeParams);
    }
  }
  if (!children.empty() && !terminated /*no "--" seen*/ && paramAcceptingValue && paramAcceptingValue->isPositional()) {
    // Are we completing a positional parameter? Then we could stop processing with "--" if a value is not required,
    // or already specified, when multiple are allowed
    if (!paramAcceptingValue->isRequired() ||
      (paramAcceptingValue->allowsMultiple() && lexed.contains(paramAcceptingValue->getName()))) {
      complete.stopProcessingMarker();
    }
  }
  complete.write(std::cout);
  return EXIT_SUCCESS;
}

/// \brief Handle autocompletion of a potential child command
/// \param arguments Arguments that could not be lexed for the current Command
/// \return Exit code
int Command::autocompleteChildCommand(std::queue<std::string>& arguments) {
  assert(!arguments.empty() && "Only call this function if not all arguments could be lexed");

  const auto children = this->createChildCommands();
  if (children.empty()) { // We have no child commands
    std::cerr << "Cannot autocomplete unknown parameter " << arguments.front() << '\n';
    return EXIT_FAILURE;
  }
  else { // We have child commands
    std::string command = arguments.front();
    arguments.pop();
    auto child = std::ranges::find_if(children, [&command](const std::shared_ptr<Command>& child) {
      return child->getName() == command && !child->isUndocumented();
    });
    if (child == children.cend()) {
      std::cerr << "Cannot autocomplete unknown child command " << command << '\n';
      return EXIT_FAILURE;
    }
    // Recurse: complete parameters of child command
    return (*child)->printAutocompleteInfo(arguments);
  }
}

}
}
