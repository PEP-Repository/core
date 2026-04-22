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

void Command::showParameterDeprecationWarnings(const Parameters& parameters) {
  for (const auto& param : parameters) {
    if (param.getDeprecationMessage().has_value() && mParameterValues->has(param.getName())) {
      std::cerr << "Warning: '--" << param.getName() << "' is deprecated. " << *param.getDeprecationMessage() << std::endl;
    }
  }
}

std::optional<int> Command::applyParameterTransformations(const Parameters& parameters, std::queue<std::string>& remainingArgs) {
  Command* dispatchAncestor = nullptr;
  bool anyTransformed = false;
  NamedValues mergedToAdd;
  CommandPath mergedChildPath;

  for (const auto& param : parameters) {
    if (!param.hasTransformer() || !mParameterValues->has(param.getName())) {
      continue;
    }
    auto transformResult = param.transform(*this, *mParameterValues);

    if (transformResult.ancestor != nullptr) {
      assert((dispatchAncestor == nullptr || dispatchAncestor == transformResult.ancestor)
        && "Programmer error: Multiple transformed parameters specified conflicting dispatch ancestors.");
      dispatchAncestor = transformResult.ancestor;
    }

    if (!transformResult.childPath.empty()) {
      assert((mergedChildPath.empty() || mergedChildPath == transformResult.childPath)
        && "Programmer error: Multiple transformed parameters specified conflicting child paths.");
      mergedChildPath = transformResult.childPath;
    }

    for (const auto& [key, vals] : transformResult.toAdd) {
      mergedToAdd[key] = vals;
    }

    mParameterValues->erase(param.getName());
    anyTransformed = true;
  }

  if (!anyTransformed) {
    return std::nullopt;
  }

  Command* ancestor = (dispatchAncestor != nullptr) ? dispatchAncestor : this;

#ifndef NDEBUG
  auto hasValidChildPath = [](Command* root, const CommandPath& path) {
    Command* current = root;
    std::shared_ptr<Command> currentOwner;
    for (const auto& segment : path.segments) {
      auto children = current->createChildCommands();
      auto it = std::find_if(children.cbegin(), children.cend(), [&segment](const std::shared_ptr<Command>& child) {
        return child->getName() == segment;
      });
      if (it == children.cend()) {
        return false;
      }
      currentOwner = *it;
      current = currentOwner.get();
    }
    return true;
  };
  assert(hasValidChildPath(ancestor, mergedChildPath)
    && "Programmer error: parameter transformation specified an invalid child path.");
#endif

  NamedValues leafValues = *mParameterValues;
  for (const auto& [key, vals] : mergedToAdd) {
    leafValues[key] = vals;
  }
  return ancestor->dispatchTo(mergedChildPath, std::move(leafValues), std::move(remainingArgs));
}

int Command::dispatchTo(CommandPath childPath, NamedValues leafValues, std::queue<std::string> leafArgs) {
  if (!childPath.empty()) {
    // Routing: finalize this level with existing values, then recurse into the named child.
    if (!mParameterValues.has_value()) {
      mParameterValues.emplace();
    }
    
#ifndef NDEBUG
    // Check if leafArgs contain parameters that would forward to a different command
    // This catches the case where an alias command forwards, but parameters try to forward elsewhere
    if (!leafArgs.empty()) {
      try {
        auto myParams = this->getSupportedParameters();
        auto lexed = myParams.lex(leafArgs);
        // Check if any lexed parameters have transformations that forward to different commands
        for (const auto& [paramName, values] : lexed) {
          auto paramIt = std::find_if(myParams.begin(), myParams.end(), 
                                       [&paramName](const Parameter& p) { return p.getName() == paramName; });
          if (paramIt != myParams.end() && paramIt->hasTransformer()) {
            // Simulate the transformation to see if it tries to forward to a different command
            NamedValues tempValues;
            for (const auto& val : values) {
              tempValues[paramName].add(val);
            }
            auto transformResult = paramIt->transform(*this, tempValues);
            assert((transformResult.childPath.empty() || transformResult.childPath == childPath) && 
                   "Programmer error: When using an alias/forwarding command, parameters cannot forward to a different command. "
                   "The alias command specifies one target, but the parameter tries to forward to another. "
                   "Either remove the parameter's command forwarding or use the target command directly.");
          }
        }
      }
      catch (...) {
        // If lexing fails, let it fail at the child level with proper error handling
      }
    }
#endif
    
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
    const std::string childName = childPath.segments.front();
    CommandPath remaining;
    if (childPath.segments.size() > 1U) {
      remaining.segments.assign(childPath.segments.begin() + 1, childPath.segments.end());
    }
    auto it = std::find_if(children.cbegin(), children.cend(), [&childName](const std::shared_ptr<Command>& c) {
      return c->getName() == childName;
    });
    if (it == children.cend()) {
      return this->issueCommandLineHelp("Unsupported subcommand '" + childName + "' for " + this->getName());
    }
    
    // Validate final target when forwarding: must not be an alias/forwarding-deprecated/removed command
    // Note: In-place deprecated commands (that don't forward) are allowed as targets
    if (remaining.empty()) {
      const auto& target = *it;
      assert(target->getDescription().find("Alias for:") != 0 && 
             "Command/parameter forwarding cannot target an alias or forwarding-deprecated command. This creates a forwarding chain. Refactor to point directly to the ultimate target.");
      assert(target->getDescription().find("No longer supported.") != 0 && 
             "Command/parameter forwarding cannot target a no-longer-supported command. This makes no sense.");
      
      // Show deprecation warning for the target command if it's in-place deprecated
      if (auto warning = target->getDeprecationWarning()) {
        std::cerr << "Warning: The command '" << target->getName() << "' is deprecated. " << *warning << std::endl;
      }
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
  
  // Check target parameters that were added via transformation
  auto leafParams = this->getSupportedParameters();
  for (const auto& param : leafParams) {
    if (leafValues.has(param.getName())) {
      // Target parameter exists in leafValues (was added via transformation)
      
      // Assert if target has a transformer (is an alias or forwarding parameter)
      assert(!param.hasTransformer() && 
             "Programmer error: Parameter forwarding cannot target a parameter that itself has a transformer (alias/forwarding). "
             "This creates a parameter forwarding chain. Refactor to point directly to the ultimate target.");
      
      // Assert if target is no-longer-supported
      assert(!param.isNoLongerSupported() && 
             "Programmer error: Parameter forwarding cannot target a no-longer-supported parameter. This makes no sense.");
      
      // Show deprecation warning if target parameter is deprecated (in-place)
      if (param.getDeprecationMessage().has_value()) {
        std::cerr << "Warning: '--" << param.getName() << "' is deprecated. " << *param.getDeprecationMessage() << std::endl;
      }
    }
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
      if (auto exitCode = this->checkNoLongerSupportedParameters(leafParams)) {
        return *exitCode;
      }
      this->showParameterDeprecationWarnings(leafParams);
      if (auto exitCode = this->applyParameterTransformations(leafParams, leafArgs)) {
        return *exitCode;
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

    // Report "unsupported parameter" if we received arguments that we can't pass to a child. See #2041
    if (children.empty() && !arguments.empty()) {
      return this->issueCommandLineHelp("Unrecognized command line parameter(s) issued to '" + this->getName() + "', starting with '" + arguments.front() + "'" + GetGlobWarning(arguments.front()));
    }

    // Apply defaults and check validity
    mParameterValues = parameters.parse(lexed);
    this->finalizeParameters();
    assert(mParametersFinalized);

    // Check no-longer-supported parameters FIRST, before showing any warnings or forwarding
    if (auto exitCode = this->checkNoLongerSupportedParameters(parameters)) {
      // Fail fast: don't show deprecation warnings if we have a no-longer-supported error
      return *exitCode;
    }

    // Now show deprecation warning for the command (after no-longer-supported check)
    if (auto warning = this->getDeprecationWarning()) {
      std::cerr << "Warning: The command '" << this->getName() << "' is deprecated. " << *warning << std::endl;
    }

    // Show parameter deprecation warnings
    this->showParameterDeprecationWarnings(parameters);

    // Apply parameter transformations (which may forward to other commands)
    if (auto exitCode = this->applyParameterTransformations(parameters, arguments)) {
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
