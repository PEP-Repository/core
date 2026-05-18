# Contributing to the PEP C++ codebase

This document provides guidelines for the use of C++ when coding for the PEP project. They build upon more general guidelines [documented separately](../CONTRIBUTING.md).

[TOC]

## Cpp Core Guidelines

In principle, we follow all [Cpp Core Guideline](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#main).
These are widely accepted guidelines that cover many topics and are generally uncontroversial.
It is very good baseline to start with for writing high quality code.
We recommend to read through it in full at least once.

Consider the contribution guide that you are reading right now to be an extension to the [core_guidelines].
In case there happen to be conflicts between our own guidelines
We recommend that you read through these at least once so that you at least have a vague idea of the contents.

In the rare case that there are conflicts between this document and the core guidelines, then this document is leading.
It should always be explicitly mentioned when this is the case.

**Note on code reviews**
We encourage using links to related core guidelines sections in code reviews. This helps keeping review comments concise and saves you from having to explain the same thing in multiple reviews.

- **Example, bad**

  ```plaintext
  You should not use a singleton here, because... [lengthy text follows]
  ... global state ...
  ... race condition!

  Instead you should... [more lengthy text follows].
  ```

- **Example, good**

  ```plaintext
  See [I.3: Avoid singletons](link), which also lists some alternatives.
  ```

## PEP C++ Guidelines

This section lists our own coding guidelines, which we apply on top of the more generic [core guidelines](#cpp-core-guidelines).

### Use of C++ basics

- use C++20
- keep the use of templating low
- Use STL containers like `std::vector`, `std::map` instead of rolling your own
- Use `auto` to avoid redundant repetition of type names, ie use `auto` instead of `std::map<RequestType,int>::iterator` [ES.11](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es11-use-auto-to-avoid-redundant-repetition-of-type-names)
- use [range-based for loops](https://en.cppreference.com/w/cpp/language/range-for) `for (auto& e : collection) { ... }` [ES.71](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es71-prefer-a-range-for-statement-to-a-for-statement-when-there-is-a-choice)
- use `nullptr` rather than `NULL` or `0` [ES.47](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es47-use-nullptr-rather-than-0-or-null)
- Prefer `std::unordered_map` and `std::unordered_set` over `std::map` and `std::set` when the order of elements is not important while lookup and insertion performance is.
- Pre-allocate STL containers wherever possible by calling (for example) [`std::vector::reserve(new_cap)`](https://en.cppreference.com/w/cpp/container/vector/reserve) [Per.14](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#per14-minimize-the-number-of-allocations-and-deallocations)
- Avoid casts. If you must use a cast, use a type initializer like `int64{1}` when the value can be safely converted or a named cast like `static_cast<T>` if it can not [ES.49](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es49-if-you-must-use-a-cast-use-a-named-cast)
- Prefer `using` over `typedef` for defining aliases [T.43](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#t43-prefer-using-over-typedef-for-defining-aliases

### On Lambdas

- For short instructions, use lambda functions instead of helper functions. E.g., for concise Rx-sequences, and have each new daisy in the chain started at the same position.
  <details>
    <summary>Short lambda example (no Rx)</summary>

  ```c++
  auto candidates = std::vector<int>({ 1,2,3,4,5,6,7,8 });
  auto divisor = 3;
  auto divisible = std::count_if(candidates.begin(), candidates.end(),
    [&divisor](int& i) { return (i % divisor == 0); });
  ```

  </details>

- For more elaborate instructions, consider helper functions to avoid cluttering.
- When operations contain multiple lambdas, start each lambda with a line of comment introducing it's purpose.
- When daisy chaining operations, consider adding comments in between the commands if the lines would become too long to append them with comments.
  <details>
    <summary>Daisy chain with comments example</summary>
    (a daisy chain is a chain of operations, each returning the original or an adapted version of that accepting the next command)

    ```c++
    // Perform the adding of participants operations (and return 0..n FakeVoid items)
    return this->addParticipantsToGroupsForRequest(request) 
      // Perform the removal of participants operations (and return 0..n FakeVoid items)
      .concat(this->removeParticipantsFromGroupsForRequest(request)) 
      // Ignore earlier items and just return a _single_ FakeVoid, so that we know that the concat_map (below) is invoked only once
      .op(RxInstead(FakeVoid())) 
      // Return a (single) AmaMutationResponse (serialized and converted to obs<obs<shared>>)
      .concat_map([](FakeVoid){return TLSMessageServer::Just(AmaMutationResponse());}); 
    ```

    ([source](https://gitlab.pep.cs.ru.nl/pep/core/-/blob/676983999b11c5172db41da64c41ad3c07d4aa7a/cpp/pep/accessmanager/AccessManager.cpp#L995))
</details>

### On Polymorphism

- use `override` to indicate a method that overrides one in a base class:
  <details>
    <summary>Example</summary>
  
  ```c++
  virtual rxcpp::observable<std::string>
            receivedRequest(std::string&& message)
            override {
      ...
  }
  ```
  </details>
- use `final` to indicate a class or method can not be extended;
- always refer to the source object, also when it's `this`, with the exception of m-prefixed member attributes.
- introduce functions/methods that reveal the intentions of operations and may structure elaborated or cluttered code...
- ... but when creating new methods/functions while refacturing classes:
  - consider the readability of code: static methods tells the calling method 'where it belongs to', but they require header changes whereas anonymous namespaces don't.
  - consider that all libraries that use a library require recompiling when headers change.
  - the [pImpl pattern](https://en.cppreference.com/w/cpp/language/pimpl) has more code-overhead (but hardly any performance loss) can be considered:
    <details>
    <summary>pImpl pattern basics example:</summary>

    ```
    # .hpp:
    class UseMe {
    private:
      class impl;
      shared_ptr<Impl> mImpl;
    public:
      void doSomething();
    ...

    #.cpp
    class UseMe:: Impl {
      public:
      void actuallyDoSomething();
    ...
    ```
    </details>

  <details>
  <summary>Using interfaces</summary>

  - Use interfaces (I-prefixed) using multiple inheritance to ensure compatibity of different objects.
    - Use virtual functions only
    - Do not add attributes via interfaces
    - Example at  https://stackoverflow.com/a/1216758
  </details>

### Use of libraries

- Try to use the [C++ standard library](https://en.cppreference.com/w/cpp/standard_library) instead of rolling your own.
- If not in the C++ STL, look at the [Boost libraries](https://www.boost.org/).
- Do not silently introduce a dependency on an external library unless it's a team decision.
- Our dependencies are managed through Conan, a detailed list presented in our [conanfile.py](/conanfile.py).

#### On Rx observables

<details>
  <summary>Do not assume behaviour and prevent over-subscribing.</summary>

  We do most processing on Rx `observable<>` instances without knowing whether the observable is [hot or cold, connectable or not](http://reactivex.io/documentation/observable.html), so one should take care not to make assumptions about an observable's behavior. Special care should be taken not to subscribe to the same observable multiple times, since the observable may or may not emit items the second time, and it may or may not re-emit the same sequence of items. A second subscription may even "corrupt" external state, causing the observable to emit unexpected items, e.g.

  - if a source re-emits its items upon every subscription, and
  - if an RX pipeline uses the `.reduce` operator to collect source emissions into a (single) `vector<>`, and
  - if the result is subscribed to multiple times,
  
  then the source emissions will be re-added to the same `vector<>` for every subscription! Note that multiple subscriptions may not always be immediately evident in code. E.g. when [creating a Cartesian product as described on StackOverflow](https://stackoverflow.com/a/26588822), the `letters` observable is subscribed to as many times as there are items in the `number` observable.

  Please prevent trouble and **don't subscribe to the same observable multiple times**. The most straightforward way to prevent multiple subscriptions is to forego storage of observables in (local, captured or member) variables. Instead just return (the result of transformations on) incoming observables:

  ```c++
  template <typename TOut, typename TIn>
  rxcpp::observable<TOut> MyTransformation(rxcpp::observable<TIn> input) {
    return input
      .op(ToIntermediate)
      // ... more transformations
      .op(ToOut);
  }
  ```

  </details>

### Assumptions

<details>
  <summary>Explicit assumes via Static and Dynamic asserts</summary>

  Should be explicitly in the code. There are two options:

  - statically assert (at compile time)

  ```c++
  static_assert(sizeof(int) == 4,
                "we need this because of ...");
  ```

  - dynamically assert (at runtime)

  ```c++
  assert(datastructure.size() == 0);
  ```

</details>

### Style

- Use spaces for indentation instead of tabs.
- Use two spaces per indentation level.
- Variable names start with a `l`owerase `l`etter;
- Names of static methods and free functions start with an `U`ppercase `L`etter;
- Instance method names start with a `l`owerase `l`etter;
- Use curly braces `{}` for loop code and condition branches:

```
if (extinctionLevelEvent) {
  while (stillAlive) {
    doPanic();
  }
}
else {
  carryOn();
}
```

### Various

- do use tested design patterns and principles;
- do **not** use any pointer magic;
- do **not** use global variables;
- do **not** use magic numbers, *better:*  
  - `sizeof(MessageHeader)` instead of `4`;  
  - or `static const int NUMBER_OF_ATTEMPTS = 4` (because of scoping, no #define);
- do not use `exit(n)` or `abort()` in your code directly;
- always use a catch all in your `main` function (or even better, use the infrastructure provided by the `pep::Application` class);
- use the macros in [the `BuildFlavor.hpp`](https://gitlab.pep.cs.ru.nl/pep/core/-/blob/main/core/BuildFlavor.hpp) header to differentiate between (debug and release) build types, instead of the canonical but [difficult-to-interpret](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1317) `#ifndef NDEBUG`.
  
### Use smart pointers

<details>
  <summary>Automate lifetime management with e.g. std::shared_ptr</summary>

  They do automatic lifetime management by means of reference counting. So auto freeing.
  
  ```c++
  // variable type
  std::shared_ptr<PEPStuff> variable = ...;
  // construct new object
  ... = std::make_shared<PEPStuff>(...)
  ```
  
  **Note** avoid circular references.
  
  When using smart pointers (so current object is memory managed by the smart pointer) in combination with lambda functions, always capture `this` using: `[self = shared_from_this()]{ ... };`
  
  A class managed using smart pointers should have a private constructor and a static `Create` method whichs acts like a constructor but returns a shared pointer. Such a class should always implement `std::enable_shared_from_this` to enable the above `shared_from_this()` method. In PEP we use a mixin for the above, called `SharedConstructor`.
</details>

### Logging and severity levels

PEP provides a [logging system](https://gitlab.pep.cs.ru.nl/pep/core/blob/main/core/include/Log.hpp) based on the [Boost.Log library](https://www.boost.org/doc/libs/1_67_0/libs/log/doc/html/index.html). Use the `LOG` macro to add entries.
<details>
  <summary>Example:</summary>

  ```c++
  LOG(LOG_TAG, severity_level::error) << "Received an error! (stream id " << dwStreamId << ")";
  ```
  
  The 2nd parameter to the `LOG` macro specifies the severity level associated with the log entry. Supported levels are defined in enumeration `severity_level`:

  - use `severity_level::debug` for log entries intended to help debugging.
  - use `severity_level::info` for informational messages.
  - use `severity_level::warning` for trouble that can be safely ignored or recovered from.
  - use `severity_level::error` for failures that pose no immediate threat to security, data integrity, or the future functioning of the application.
  - use `severity_level::critical` for end of world events. In most cases the software will need to be terminated after such failures.

</details>
