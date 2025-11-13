#include <pep/application/Application.hpp>
#include <pep/application/CommandLineAutocomplete.hpp>
#include <pep/versioning/Version.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <filesystem>

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
    // Note: this link will produce a 404 for old (unsupported) release branches, and for feature branches (if documentation was not explicitly published).
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

int Command::process(std::queue<std::string>& arguments) {
  auto children = this->createChildCommands();

  try {
    // Read-and-eat strings from the arguments queue
    auto parameters = this->getSupportedParameters();
    auto argumentsCopy = arguments;
    auto lexed = parameters.lex(arguments);
    if (lexed.find("autocomplete") != lexed.end()) {
      return this->printAutocompleteInfo(argumentsCopy);
    }
    auto result = this->processLexedParameters(lexed);
    if (result.has_value()) {
      return *result;
    }
    assert(mParametersLexed);

    // Report "unsupported parameter" if we received arguments that we can't pass to a child. See #2041
    if (children.empty() && !arguments.empty()) {
      return this->issueCommandLineHelp("Unrecognized command line parameter(s) issued to '" + this->getName() + "', starting with '" + arguments.front() + "'" + GetGlobWarning(arguments.front()));
    }

    // Apply defaults and check validity
    mParameterValues = parameters.parse(lexed);
    this->finalizeParameters();
    assert(mParametersFinalized);
  }
  catch (const std::exception& error) {
    return this->issueCommandLineHelp(error.what());
  }

  assert(std::all_of(children.cbegin(), children.cend(), [this](const std::shared_ptr<Command>& child) { return child->getParentCommand() == this; }));
  if (!children.empty()) {
    if (arguments.empty()) {
      return this->issueCommandLineHelp("No command specified.");
    }
    std::string command = arguments.front();
    arguments.pop();
    auto child = std::find_if(children.cbegin(), children.cend(), [&command](const std::shared_ptr<Command>& child) {
      return child->getName() == command;
    });
    if (child == children.cend()) {
      return this->issueCommandLineHelp("Unsupported command '" + command + "' issued to " + this->getName() + GetGlobWarning(command));
    }
    return (*child)->process(arguments);
  }

  assert(arguments.empty()); // due to unsupported parameter check above
  return this->execute();
}

int Command::printAutocompleteInfo(std::queue<std::string>& arguments) {
  const auto children = this->createChildCommands();

  const auto parameters = this->getSupportedParameters();
  bool terminated;
  // Lex (possibly again)
  const auto lexed = parameters.lex(arguments, &terminated);

  if (!arguments.empty()) {
    // Not everything was lexed
    return this->autocompleteChildCommand(arguments);
  }

  // Everything was lexed

  Autocomplete complete;

  const Parameter* paramAcceptingValue = parameters.firstAcceptingValue(lexed);
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
      // Complete parameter switches
      auto completeParams = parameters.getParametersToAutocomplete(lexed);
      // Put required parameters first
      std::stable_sort(completeParams.begin(), completeParams.end(), [](const Parameter* a, const Parameter* b) {
        return a->isRequired() > b->isRequired();
      });
      complete.parameters(completeParams);
    }
  }
  if (!children.empty() && !terminated /*no "--" seen*/ && paramAcceptingValue && paramAcceptingValue->isPositional()) {
    // Are we completing a positional parameter? Then we could stop processing with "--" if a value is not required,
    // or already specified, when multiple are allowed
    if (!paramAcceptingValue->isRequired() ||
      (paramAcceptingValue->allowsMultiple() && lexed.find(paramAcceptingValue->getName()) != lexed.cend())) {
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
    auto child = std::find_if(children.cbegin(), children.cend(), [&command](const std::shared_ptr<Command>& child) {
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
