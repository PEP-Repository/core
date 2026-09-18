# Contributing to the PEP C++ codebase

This document provides guidelines for the use of C++ when coding for the PEP project. They build upon more general guidelines [documented separately](../CONTRIBUTING.md).

[TOC]

## Cpp Core Guidelines

In principle, we follow all [Cpp Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#main).
These are widely accepted guidelines that cover many topics and are generally uncontroversial.
It is very good baseline to start with for writing high quality code.
We recommend to read through it in full at least once.

Consider the contribution guide that you are reading right now to be an extension to the Core Guidelines.
In the rare case that there are conflicts between this document and the core guidelines, then this document is leading.
It should always be explicitly mentioned when this is the case.

## Note on code reviews

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

- use C++23
- keep the use of templating low
- Use STL containers like `std::vector`, `std::map` instead of rolling your own
- Use `auto` to avoid redundant repetition of type names, ie use `auto` instead of `std::map<RequestType,int>::iterator` [ES.11](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es11-use-auto-to-avoid-redundant-repetition-of-type-names)
- use [range-based for loops](https://en.cppreference.com/w/cpp/language/range-for) `for (auto& e : collection) { ... }` [ES.71](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es71-prefer-a-range-for-statement-to-a-for-statement-when-there-is-a-choice)
- use `nullptr` rather than `NULL` or `0` [ES.47](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es47-use-nullptr-rather-than-0-or-null)
- Prefer `std::unordered_map` and `std::unordered_set` over `std::map` and `std::set` when the order of elements is not important while lookup and insertion performance is.
- Pre-allocate STL containers when you fill them with a loop, by calling (for example) [`std::vector::reserve(new_cap)`](https://en.cppreference.com/w/cpp/container/vector/reserve) [Per.14](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#per14-minimize-the-number-of-allocations-and-deallocations). Don't do this when building a container from a range: [`std::ranges::to`](#on-ranges) already reserves for sized ranges.
- Avoid casts. If you must use a cast, use a type initializer like `int64{1}` when the value can be safely converted or a named cast like `static_cast<T>` if it can not [ES.49](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es49-if-you-must-use-a-cast-use-a-named-cast)
- Prefer `using` over `typedef` for defining aliases [T.43](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#t43-prefer-using-over-typedef-for-defining-aliases
- Prefer a defaulted `[[nodiscard]] auto operator<=>(const T&) const = default` over hand-writing `operator<` with
  `std::tie`.

### On ranges

We use `std::ranges` algorithms and views throughout. Not every C++23 range feature is available to us though: see [Environments we build for](#environments-we-build-for) below for environments we need to support.

#### Algorithms

- Use the `std::ranges` algorithm, not the iterator-pair `std::` one: `sort(v)` rather than `std::sort(v.begin(), v.end())`. Only fall back to iterators for a genuine sub-range, and prefer `views::take`/`views::drop`/`std::ranges::subrange` even then. Ranged algorithms have more benefits than just the syntactic sugar, like support for sentinel iterators and projections.
- Use the projection (`Proj`) parameter to get rid of lambdas. Most `find_if` calls are really a `find` with a projection, and most comparators are really a `sort` with one. Passing `{}` as the comparator means "the default `<` or `==`".
  <details>
    <summary>Projection examples</summary>

  ```c++
  // A comparison lambda is a `find` with a projection
  find_if(items_, [id](const StudyContext& c) { return c.getId() == id; }); // Don't
  find(items_, id, &StudyContext::getId);                                   // Do

  // Searching only to compare against end() is a `contains` with a projection
  find_if(devices.cbegin(), end, [&columnName](const DeviceRegistrationDefinition& d) {
    return d.columnName == columnName; }) != end;                           // Don't
  contains(devices, columnName, &DeviceRegistrationDefinition::columnName); // Do

  // A comparator functor or lambda is a `sort` with a projection
  sort(entries, [](const Entry& lhs, const Entry& rhs) {
    return std::get<0>(lhs).getText() < std::get<0>(rhs).getText(); });     // Don't
  // Note: decltype(auto) is to return a reference if getText() returns a reference, avoiding a copy
  sort(entries, {}, [](const Entry& e) -> decltype(auto) {
    return std::get<0>(e).getText(); });                                    // Do
  ```

  </details>

  A projection may itself be a lambda when the key is a chained getter (`[](const auto& d) { return d.getColumn().getFullName(); }`); that is still clearer than a two-argument comparator.
- A predicate may be a pointer-to-member (field or function) directly, e.g. `any_of(entries_, &Parameter::isRequired)`, because ranges invoke through [`std::invoke`](https://en.cppreference.com/w/cpp/utility/functional/invoke). Consequently, using `std::mem_fn` is not necessary for parameters to functions in `std::ranges`.
  However, taking the address of most standard library functions (e.g. `&std::string::empty`) is [not allowed](https://eel.is/c++draft/namespace.std#def:function,addressable). Taking the address of an *overloaded* function is also not possible (`PEP_WRAP_FN` may be used in some cases).
- Test membership with `contains`, not by comparing a `find` or `count` result, unless the iterator is required: `contains(modes, "read")` rather than `find(...) != end()`, and `map.contains(k)` rather than `map.find(k) != map.end()` or `map.count(k) != 0`.
- Replace a "set a flag and break" search loop with `any_of`/`all_of`/`none_of`.
- Remove elements with [`std::erase`/`std::erase_if`](https://en.cppreference.com/w/cpp/container/vector/erase2) rather than the erase-remove idiom.

#### Hand-written loops

Most remaining hand-written loops are a range algorithm or view in disguise. Even when you think you need an index to index into multiple lists, you may be able to use [`std::views::zip`](https://en.cppreference.com/w/cpp/ranges/zip_view) instead, which produces tuples of references (just use `for (auto [a, b] : views::zip(...))`). If you actually need the index, consider zipping with `views::iota(0uz)` (`views::enumerate` is not available yet on all platforms).

#### Building containers

- Build a container with a view pipeline terminated by [`std::ranges::to`](https://en.cppreference.com/w/cpp/ranges/to), rather than `reserve` plus `std::transform` into a `std::back_inserter`. For a longer chain, put each operation on its own line:
  <details>
    <summary>Pipeline example</summary>

  ```c++
  auto pseudIndices = request->entries
    | views::transform(&DataStoreEntry2::pseudonymIndex)
    | to<std::vector>();
  ```

  </details>
- Pick the conversion spelling that reads best:
  - a trailing `| to<C>()` when there is a pipeline;
  - `C c(std::from_range, r)` or `= {std::from_range, r}` is also an option when constructing a named container from a plain range;
  - a prefix `to<C>(r)` when the range is a single expression that you're already passing as an argument.
- Append or insert whole ranges with `vec.append_range(r)` and `set.insert_range(r)` instead of looping over `push_back`/`insert`. These need the element type to be *implicitly* convertible, so appending `string_view`s to a `vector<string>` still needs a `views::transform`.

#### Mechanics

- For conciseness, put `using namespace std::ranges;` at the top of a `.cpp` file when using multiple ranged functions. Never put it in a header: instead spell out `std::ranges::`/`std::views::`, or scope the using-directive to a function body.
- Boost algorithms may not accept C++20 ranges, so materialize first: `boost::algorithm::join(items | views::transform(...) | to<std::vector>(), ",")`. The converse also holds: several third-party collection types (`boost::urls::params_encoded_view`, sqlite_orm's `mapped_view`) work with a range-based `for` but do *not* satisfy `std::ranges::input_range`, so they can't start a view pipeline.
- A view constructed over an lvalue is lazy and does not own its source. Don't return one, store one, or hand one to Rx: materialize with `| to<std::vector>()` first if the source will not outlive the iteration.

### On Lambdas

- For short instructions, use lambda functions instead of helper functions. E.g., for concise Rx-sequences, and have each new daisy in the chain started at the same position.
  <details>
    <summary>Short lambda example (no Rx)</summary>

  ```c++
  auto candidates = std::vector<int>({ 1,2,3,4,5,6,7,8 });
  auto divisor = 3;
  auto divisible = std::ranges::count_if(candidates,
    [divisor](int i) { return i % divisor == 0; });
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
- always make inheritance visibility explicit (usually `public`), e.g. `class Derived : public Base { ... }`.
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

- Prefer using compiled code over defining (and using) macros.
- Macro names are written in `SCREAMING_SNAKE_CASE` to make them stand out like the sore thumb that they are.
- The names of macros defined in the PEP code base start with a `PEP_` prefix to prevent naming collisions (e.g. with other libraries).
- Things that are not macros have names that are *not* written in `SCREAMING_SNAKE_CASE` to prevent naming collisions with macros from e.g. other libraries.
- Names of `class` and `struct` and `enum` types are written in `PascalCase`, starting with an `U`ppercase `L`etter.
- Variable names are written in `camelCase`, starting with a `l`owerase `l`etter.
- Names of static methods and free functions are written in `PascalCase`, starting with an `U`ppercase `L`etter.
- Instance method names are written in `camelCase`, starting with a `l`owerase `l`etter.
- Enumerator (value) names are written in `PascalCase`, starting with an `U`ppercase `L`etter.
- Names of `constexpr` constants are written in `PascalCase`, starting with an `U`ppercase `L`etter (since they are so similar to enumerator values).
- Names of `const` variables are written in `PascalCase`, starting with an `U`ppercase `L`etter (since they are so similar to `constexpr` constants).
- Prefer proper encapsulation and state management over publically accessible state.
- Properly encapsulating user-defined (non-alias) types are defined using the `class` keyword.
    - Non-const fields of such types are all `private`.
    - Private field names are written in `camelCase_`, starting with a `l`owerase `l`etter, and ending with an underscore `_` character. (The suffix prevents naming collisions with similarly named instance methods, such as getter methods.)
    - Access a private field by its plain name (e.g. `memberVariable_`), not `this->memberVariable_`.
- Property bags (i.e. user-defined aggregate types with public fields) are defined using the `struct` keyword.
    - Such types do not contain instance methods, putting state management responsibilities firmly into the caller's hands.
    - Field names are written in `camelCase`, starting with a `l`owerase `l`etter. Note that these names lack the trailing underscore `_` character that's used for non-public fields.
    - Access a public field by its plain name (e.g. `memberVariable`); use `this->memberVariable` only when the plain name would be ambiguous or less clear (developer discretion).
- Call methods by their plain name (e.g. `name()`); use `this->name()` only when the plain name would be ambiguous or less clear (developer discretion), and `Class::name()` only when actually necessary (e.g. to call a base class implementation).
- Use explicit comparison instead of implicit conversion for integers.
- Implement functions inside the namespace block rather than qualifying the definition, e.g.:

  ```c++
  namespace pep {
  void Fun() {
    ...
  }
  }
  ```

  not

  ```c++
  void pep::Fun() {
    ...
  }
  ```
- Mark future work with `TODO`, use `TODO(workaround)` for temporary workarounds, e.g. waiting for an upstream fix.

- Names of Protobuf `message` and `enum` types are written in `PascalCase`, starting with an `U`ppercase `L`etter.
- Names of Protobuf fields are written in `snake_case`, i.e. fully lowercase with underscores between words.

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
- do **not** use mutable global variables, including non-`const` `static` variables. If you really need this, keep it local to a single translation unit (e.g. in an anonymous namespace) if possible.
- do **not** use magic numbers, *better:*  
  - `sizeof(MessageHeader)` instead of `4`;  
  - or `static const int NumberOfAttempts = 4` (because of scoping, no #define);
- do not use `exit(n)` or `abort()` in your code directly;
- always use a catch all in your `main` function (or even better, use the infrastructure provided by the `pep::Application` class);
- use the macros in [the `BuildFlavor.hpp`](https://gitlab.pep.cs.ru.nl/pep/core/-/blob/main/core/BuildFlavor.hpp) header to differentiate between (debug and release) build types, instead of the canonical but [difficult-to-interpret](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1317) `#ifndef NDEBUG`.
  
### Use smart pointers

<details>
  <summary>Automate lifetime management with e.g. std::shared_ptr</summary>

  They do automatic lifetime management by means of reference counting. So auto freeing.
  
  ```c++
  // variable type
  std::shared_ptr<PepStuff> variable = ...;
  // construct new object
  ... = std::make_shared<PepStuff>(...)
  ```
  
  **Note** avoid circular references.
  
  When using smart pointers (so current object is memory managed by the smart pointer) in combination with lambda functions, always capture `this` using: `[self = shared_from_this()]{ ... };`
  
  A class managed using smart pointers should have a private constructor and a static `Create` method whichs acts like a constructor but returns a shared pointer. Such a class should always implement `std::enable_shared_from_this` to enable the above `shared_from_this()` method. In PEP we use a mixin for the above, called `SharedConstructor`.
</details>

### Logging and severity levels

PEP provides a [logging system](https://gitlab.pep.cs.ru.nl/pep/core/blob/main/core/include/Log.hpp) based on the [Boost.Log library](https://www.boost.org/doc/libs/1_67_0/libs/log/doc/html/index.html). Use the `PEP_LOG` macro to add entries.
<details>
  <summary>Example:</summary>

  ```c++
  PEP_LOG(LogTag, Severity::Error) << "Received an error! (stream id " << dwStreamId << ")";
  ```
  
  The 2nd parameter to the `PEP_LOG` macro specifies the severity level associated with the log entry. Supported levels are defined in enumeration `Severity`:

  - use `Severity::Verbose` for log entries providing rich detail.
  - use `Severity::Debug` for log entries intended to help debugging.
  - use `Severity::Info` for informational messages.
  - use `Severity::Warning` for trouble that can be safely ignored or recovered from.
  - use `Severity::Error` for failures that pose no immediate threat to security, data integrity, or the future functioning of the application.
  - use `Severity::Critical` for end of world events. In most cases the software will need to be terminated after such failures.

</details>

### Doc comments

- Preferred block-comment style is `///`, e.g. `/// This function does stuff`.
- For a member documented after its declaration, use `///<`.
- Use backslash commands (`\brief`, `\param`, `\return`, ...), not the `@`-prefixed equivalents.
- If a doc comment only has a single-line brief description (no `\param`/`\return` etc.), the `\brief` command may be omitted. Otherwise, use an explicit `\brief` command and `\details` when necessary.

## Environments we build for

These are the environments we build for and thus need to support (2026-08-17):

| Platform               | Architecture | Compiler                         | Standard library |
|------------------------|--------------|----------------------------------|------------------|
| Windows 10 build 1809  | x86-64       | MSVC build tools 14.51 (VS 2026) | MS STL           |
| macOS 13.3             | x86-64       | Apple Clang 17                   | Apple libc++     |
| macOS 13.3             | arm64        | Apple Clang 21                   | Apple libc++     |
| Linux Ubuntu 26.04     | x86-64       | Clang 21                         | GCC libstdc++ 15 |
| Linux Flatpak KDE 6.11 | x86-64       | GCC 15                           | GCC libstdc++    |
| Emscripten             | wasm32       | emcc 4.0.22 / Clang 22           | Clang libc++ 20  |

See [cppstat](https://cppstat.org/) for info on supported features.
