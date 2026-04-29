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

void Command::finalizeParameters(bool isForwardingDispatch) {
  assert(!mParametersFinalized); // Prevent this method from being invoked multiple times
  this->getSupportedParameters().finalize(*mParameterValues);
  mParametersFinalized = true;
}


#ifndef NDEBUG
void Command::validateNoConflictingParameterForwards(const std::queue<std::string>& leafArgs, const CommandPath& childPath) {
  if (leafArgs.empty()) {
    return;
  }
  
  try {
    auto myParams = this->getSupportedParameters();
    // Need to make a copy since lex() modifies the queue
    auto argsCopy = leafArgs;
    auto lexed = myParams.lex(argsCopy);
    
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

std::optional<int> Command::applyParameterTransformations(const Parameters& parameters, std::queue<std::string>& remainingArgs) {

  // Step 1: Initialize accumulators for merging transformation results
  Command* dispatchAncestor = nullptr;
  bool anyTransformed = false;
  NamedValues mergedToAdd;
  CommandPath mergedChildPath;

  // Step 2: Apply transformations for each parameter that has one
  for (const auto& param : parameters) {
    // Skip parameters without transformers or not present in command line
    if (!param.hasTransformer() || !mParameterValues->has(param.getName())) {
      continue;
    }
    
    // Apply the transformation (e.g., forwarding alias)
    auto transformResult = param.transform(*this, *mParameterValues);

    // Step 3: Merge transformation results, ensuring no conflicting ancestors
    if (transformResult.ancestor != nullptr) {
      assert((dispatchAncestor == nullptr || dispatchAncestor == transformResult.ancestor)
        && "Programmer error: Multiple transformed parameters specified conflicting dispatch ancestors.");
      dispatchAncestor = transformResult.ancestor;
    }

    // Ensure no conflicting child paths
    if (!transformResult.childPath.empty()) {
      assert((mergedChildPath.empty() || mergedChildPath == transformResult.childPath)
        && "Programmer error: Multiple transformed parameters specified conflicting child paths.");
      mergedChildPath = transformResult.childPath;
    }

    // Merge parameters, ensure no conflicting parameter additions (i.e., same parameter added by multiple transformations)
    for (const auto& [key, vals] : transformResult.toAdd) {
      assert((mergedToAdd.find(key) == mergedToAdd.end())
             && "Programmer error: Multiple transformed parameters specified conflicting parameter additions.");
      mergedToAdd[key] = vals;
    }

    // Remove the transformed parameter from current values
    mParameterValues->erase(param.getName());
    anyTransformed = true;
  }

  // Step 4: If no transformations occurred, continue normal processing
  if (!anyTransformed) {
    return std::nullopt;
  }

  // Step 5: Determine the command to dispatch from (default to this command)
  Command* ancestor = (dispatchAncestor != nullptr) ? dispatchAncestor : this;

  // Step 7: Merge current parameter values with transformed values
  NamedValues leafValues = *mParameterValues;
  for (const auto& [key, vals] : mergedToAdd) {
    leafValues[key] = vals;
  }
  
  // Step 8: Dispatch to the target command with merged values
  return ancestor->dispatchTo(mergedChildPath, std::move(leafValues), std::move(remainingArgs));
}

int Command::dispatchTo(CommandPath childPath, NamedValues leafValues, std::queue<std::string> leafArgs) {

#ifndef NDEBUG
  // Check if leafArgs contain parameters that would forward to a different command
  // This catches the case where an alias command forwards, but parameters try to forward elsewhere
  this->validateNoConflictingParameterForwards(leafArgs, childPath);
#endif

  // Ensure mParameterValues is initialized for intermedaite command.
  if (!mParameterValues.has_value()) {
    mParameterValues.emplace();
  }

  // This isn't the leaf command yet, forward to child specified by childPath
  if (!childPath.empty()) {
    return routeToDescendant(std::move(childPath), std::move(leafValues), std::move(leafArgs));
  }

  // Final target must not be a forwarding command or no-longer-supported command
  assert(!this->isForwardingCommand() && 
         "Command/parameter forwarding cannot target a forwarding command. "
         "This creates a forwarding chain. Refactor to point directly to the ultimate target.");
  assert(!this->isNoLongerSupported() && 
         "Command/parameter forwarding cannot target a no-longer-supported command. This makes no sense.");
  
  // Delegate to process(), with some special handling
  return this->process(leafArgs, true, std::move(leafValues));
}

int Command::routeToDescendant(CommandPath childPath, NamedValues leafValues, std::queue<std::string> leafArgs) {

  auto children = this->createChildCommands();
  const std::string childName = childPath.segments.front();
  CommandPath remaining;
  if (childPath.segments.size() > 1U) {
    remaining.segments.assign(childPath.segments.begin() + 1, childPath.segments.end());
  }
  auto child = std::find_if(children.cbegin(), children.cend(), [&childName](const std::shared_ptr<Command>& c) {
    return c->getName() == childName;
  });

  assert(child != children.cend() && "Programmer error: a command is forwarded to an invalid child path.");
  
  return (*child)->dispatchTo(remaining, std::move(leafValues), std::move(leafArgs));
}

int Command::process(std::queue<std::string>& arguments, bool isLeafDispatch, std::optional<NamedValues> preMergedValues) {
  auto children = this->createChildCommands();

  try {
    auto parameters = this->getSupportedParameters();

    if (!mParameterValues.has_value()) {
      mParameterValues.emplace();
    }
    // Step 1: Leaf dispatch mode: merge pre-built values from transformations
    if (isLeafDispatch) {
      assert(preMergedValues.has_value() && "Leaf dispatch requires pre-merged values");
      for (const auto& [key, vals] : *preMergedValues) {
        (*mParameterValues)[key] = vals;
      }
      // Validate parameters that came from transformations
      for (const auto& param : parameters) {
        if (preMergedValues->has(param.getName())) {
          assert(!param.hasTransformer() && 
                 "Programmer error: Parameter forwarding cannot target a parameter that itself has a transformer (alias/forwarding). "
                 "This creates a parameter forwarding chain. Refactor to point directly to the ultimate target.");
          assert(!param.isNoLongerSupported() && 
                 "Programmer error: Parameter forwarding cannot target a no-longer-supported parameter. This makes no sense.");
        }
      }
      mParametersFinalized = false; 
    }
    
    // Step 2: Lex and parse remaining arguments
    if (!arguments.empty()) {
      auto argumentsCopy = arguments;
      auto lexed = parameters.lex(arguments);

      // Step 3: Handle autocomplete requests
      if (!isLeafDispatch && lexed.find("autocomplete") != lexed.end()) {
        return this->printAutocompleteInfo(argumentsCopy);
      }

      // Step 4: Process special parameters (help, windows-only switches)
      if (auto result = this->processLexedParameters(lexed)) {
        return *result;
      }
      assert(mParametersLexed);

      // Step 5: Validate that extra arguments can be passed to a child command. See #2041.
      if (!isLeafDispatch && children.empty() && !arguments.empty()) {
        return this->issueCommandLineHelp("Unrecognized command line parameter(s) issued to '" + this->getName() + "', starting with '" + arguments.front() + "'" + GetGlobWarning(arguments.front()));
      }

      // Step 6: Parse lexed values and merge into parameter values
      auto parsed = parameters.parse(lexed);
      if (isLeafDispatch) {
        // Leaf dispatch: merge parsed values into existing mParameterValues
        for (const auto& [key, vals] : parsed) {
          (*mParameterValues)[key] = vals;
        }
        mParametersLexed = true;
      } else {
        // Normal mode: replace mParameterValues and finalize
        mParameterValues = std::move(parsed);
      }
    } else {
      // Leaf dispatch with no arguments: just mark as lexed
      mParametersLexed = true;
    }

    // Step 7: Check for no-longer-supported parameters (fail fast before warnings)
    for (const auto& param : parameters) {
      if (param.isNoLongerSupported() && mParameterValues->has(param.getName())) {
        std::cerr << "Error: The parameter '--" << param.getName() << "' is no longer supported. " << *param.getNoLongerSupportedMessage() << std::endl;
        return EXIT_FAILURE;
      }
    }

    // Step 8: Show deprecation warning for the command itself
    if (auto warning = this->getDeprecationWarning()) {
      std::cerr << "Warning: The command '" << this->getName() << "' is deprecated. " << *warning << std::endl;
    }

    // Step 9: Show deprecation warnings for parameters
    for (const auto& param : parameters) {
      if (param.getDeprecationMessage().has_value() && mParameterValues->has(param.getName())) {
        std::cerr << "Warning: '--" << param.getName() << "' is deprecated. " << *param.getDeprecationMessage() << std::endl;
      }
    }

    // Step 10: Apply parameter transformations (may forward to other commands)
    if (auto exitCode = this->applyParameterTransformations(parameters, arguments)) {
      return *exitCode;
    }

    // Step 11: Finalize parameters (no more transformations allowed after this)
    this->finalizeParameters(isLeafDispatch);
    assert(mParametersFinalized);

  }
  catch (const std::exception& error) {
    return this->issueCommandLineHelp(error.what());
  }

  // Step 12: Optionally dispatch to child command
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
