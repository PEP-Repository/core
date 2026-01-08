# Coding guidelines for contributing

# Goal

Produce code that
- is easy to debug and understand;
- is modular;
- is maintainable;
- can be read by any team member;
- has been implemented for the long run (maintenaince can be done by different parties).

For this, we require:
- restrictive use of external libraries (see below);
- use of simple mechanisms;
- everything is in C++ (except when an exception is granted by the whole team).

C++ code is subject to guidelines [documented separately](cpp/CONTRIBUTING.md).

---

# Workflow

- all code and associated stuff is version controlled using git and Gitlab.
- when adjusting spacing or trivial name changes, please commit it seperate from other changes (to ease the release process);
- do not commit directly on master/stable/acc/prod (except for the people doing releases/hotfixes);
- do use issues, and create a merge request if implementing new features. Commit changes to the branch accompanying the merge request;
- every morning pull master in your current issue (working) branch;
- if a feature is not finished, do not merge into master from your issue branch;
- please contribute noteworthy changes of release to the [changelog](CHANGELOG) file.

---

# Project structure

We aim to implement the supposedly [canonical C++ project structure](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1204r0.html) (for reasons discussed [here](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2167)). Most notably, this project structure puts our C++ code in a (nested sub)directory named "pep", allowing headers to be found using `#include <pep/subdir/blah.hpp>`. But our project structure does deviate from canon in multiple ways, a.o.

- since the repository also contains stuff other than C++ projects, the canonical project structure can be found in [the `cpp` subdirectory](./cpp) rather than the repository root.
- non-unit tests are located in the *root* level [`tests` subdirectory](./tests).
- unit test sources aren't located next to the corresponding sources, but may instead be found in subdirectories named `tests`.
- unit test sources don't implement standalone executables, but fit into the Google test framework (which puts multiple tests into a single executable).
- library names (commonly) have `lib` as a suffix instead of as a prefix.
- sources of libraries and those of associated executables are located in the same directory instead of separate ones.
- CMake: Use lowercase function names: CMake functions and macros can be called lower or upper case. Always use lower case. Upper case is for variables.

# Documentation

- when simpler parts of the repository may be described by a single `README.md` file, place it in the respective directory.
- all other documentation, e.g. on parts of the `core` library, is stored in the `/docs/` folder, using Markdown files. 
- [Mermaid-js](https://mermaid.live) can be used to insert diagrams in gitlab markdown files. 
- Refer to documentation in the comments of source files, this helps keeping documentation up to date when changing structures.
- Procedures (e.g. on incident handling, support, administration etc.) are published at the wiki.
