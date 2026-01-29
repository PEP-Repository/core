# Conan

## Overview

- Install [Conan](https://conan.io/) via [pipx](https://pipx.pypa.io/stable/) / (WinGet / Chocolatey / Homebrew / ...)

### On Windows

- `python3 -m pip install --user pipx`
- `python3 -m pipx install conan`

To update Conan (and other stuff managed by `pipx`):

- `python3 -m pipx upgrade-all`

### Then

- Create Conan profile
- Define dependencies in `conanfile.py`
- Execute `conan install`
  - Builds dependencies in `~/.conan2/p/`
  - Installs files in `<build>/generators/`
- Execute CMake configure
- Execute build

See [`README`](https://gitlab.pep.cs.ru.nl/pep/core/-/blob/main/README.md#building) for more details.

If your `conan install` command fails (e.g. saying `Toolset directory for version '14.3' was not found.` when using MSVC on Windows), try the full command line used by the CI logic. At the time of writing, this works out to the following on Windows (to be ran from the root directory of a `pep/core` repository clone):
`conan install .\docker-build\builder\conan\conanfile.py --lockfile=.\docker-build\builder\conan\conan-ci.lock --profile:all=.\docker-build\builder\conan\conan_profile --build=missing -s:a build_type="Debug" -o "&:with_assessor=True" -o "&:with_servers=True" -o "&:with_castor=True" -o "&:with_tests=True" -o "&:with_benchmark=True" -o "&:custom_build_folder=True" --output-folder=.\build\`

### Example dependency from `conanfile.py`

```python
def requirements(self):
    if self.options.with_assessor and not self.options.use_system_qt:
        self.requires('qt/[^6.6]', options={
            'qtnetworkauth': True,
            'qtsvg': True,
            'qttranslations': True,
        })
```

Packages can be found on [ConanCenter](https://conan.io/center). **NOTE THAT** the URL leads to the Web interface, while the executable uses [a different URL](https://center2.conan.io) for access to the actual package(content)s.

## Profiles, settings, config, options

Conan has multiple types of configurations.

| Kind | Purpose | Profile section | Command line switch | Example |
|--|--|--|--|--|
| **[Setting](https://docs.conan.io/2/reference/config_files/settings.html)** | Setting OS, CPU, build type, compiler, ... | `[settings]` | `--setting`/`-s` | `build_type=Release` |
| **[Configuration](https://docs.conan.io/2/reference/config_files/global_conf.html)** | Conan or tool-specific (e.g. CMake) configuration | `[conf]` | `--conf`/`-c` | `tools.cmake.cmaketoolchain:generator=Ninja` |
| **Option** | Package-defined option | `[options]` | `--option`/`-o` | `shared=True` |

### Can be specified per context

- Host context: For the executable that you are building, so the target machine
  - E.g. `--setting:host`/`-s:h` (default, so equivalent to `--setting`/`-s`)
- Build context: For your own machine, so build tools
  - E.g. `--setting:build`/`-s:b`
- Both: E.g. `--setting:all`/`-s:a`

### Can be specified per package

- `openssl/[~3]:no_deprecated=True` (option)
- `pep/*:build_type=Debug` (setting)
- `&:build_type=Debug` (setting, `&` = consuming package, in our case `pep`)

Combine: E.g. build Protobuf compiler as release: `--setting:build "protobuf/*:build_type=Release"`

### Profiles

All can be specified in a [Conan profile](https://docs.conan.io/2/reference/config_files/profiles.html), default `~/.conan2/profiles/default`.

```ini
[settings]
arch=x86_64
build_type=Release
compiler=clang
compiler.cppstd=20
compiler.libcxx=libstdc++11
compiler.version=17
os=Linux

[conf]
tools.build:compiler_executables={'c': 'clang-17', 'cpp': 'clang++-17'}
tools.cmake.cmaketoolchain:generator=Ninja

[options]
qt/*:shared=True
```

Profiles can be chosen on command line via `--profile`/`-pr` (=`--profile:host`) or `--profile:build`/`-pr:b`, etc.

Please note that `compiler` is descriptive, not prescriptive: you indicate to Conan which compiler it is using, not which compiler it should use; that's what `tools.build:compiler_executables` is for. E.g. if you default compiler (`cc`/`c++`) is GCC but you tell Conan it's using clang, it will try to use GCC as if it's Clang, which will cause problems.

[Profiles can contain Jinja2 templates](https://docs.conan.io/2/reference/config_files/profiles.html#profile-rendering), e.g. `arch={{ detect_api.detect_arch() }}`.

## Recipe: `conanfile.py`

Our `conanfile.py` is in `docker-build/builder/conan/conanfile.py`, but symlinked to core repo root for easy use.

### Options

```python
class PepRecipe(ConanFile):
    name = 'pep'
    settings = 'os', 'compiler', 'build_type', 'arch'

    options = {
        # Build pepAssessor GUI (with Qt)
        'with_assessor': [True, False],
        ...: ...
    }
    default_options = {
        'with_assessor': True,
        ...: ...
    }
```

Specified using `--option` and variants.

### Building PEP

```python
def layout(self):
    ...
    cmake_layout(self)

def generate(self):
    tc = CMakeDeps(self)
    tc.generate()

    tc = CMakeToolchain(self)
    ...
    tc.cache_variables['WITH_ASSESSOR'] = self.options.with_assessor
    tc.generate()

def build(self):
    cmake = CMake(self)
    ...
    cmake.configure()
    cmake.build()
```

Where and how to build PEP. `generate` creates files for CMake.

### Requirements (dependencies)

```python
def requirements(self):
    if self.options.with_assessor and not self.options.use_system_qt:
        self.requires('qt/[^6.6]', options={
            'qtnetworkauth': True,
            'qtsvg': True,
            'qttranslations': True,
        })

def system_requirements(self):
    apt = Apt(self)
    ...
    if self.options.with_assessor and self.options.use_system_qt:
        apt.install(['qt6-base-dev', ...])
```

### Build requirements (tools)

```python
def build_requirements(self):
    # Add these to PATH
    self.tool_requires('protobuf/<host_version>')  # protoc
```

## What happens on `conan install`

- Conan checks if existing packages in home folder are compatible with those required by `conanfile.py`.
  - If `--lockfile=...` is passed, recipes must be in that lockfile.
  - If `--update` is passed, it updates to the most recent compatible revision.
- It fetches missing recipes from the remote. The default remote is ConanCenter, which provides a [Web interface](https://conan.io/center) and [**a different** remote URL](https://center2.conan.io).
  - Via the sidebar, one can find the `conanfile.py` of the recipe on the [ConanCenter Index GitHub](https://github.com/conan-io/conan-center-index)
- For missing packages, it checks if prebuilt packages are available from ConanCenter, and downloads those
  - Often this is the case for build tools
  - Configurations with prebuilt packages can be found in the 'Packages' tab on a package in ConanCenter
- It builds missing packages in the [Conan home folder](#conan-home-folder-structure)
- Finally, it creates the [`generators` folder](#generators-folder) in the build folder, and creates/updates `CMakeUserPresets.json`, aggregating all build configurations

### Conan home folder structure

- `~/.conan2/`
  - `profiles/`
  - `p/`: package sources & prebuilt packages
    - _5-char-packagename_ + _random hash_ `/`
    - `b/`: packages built on this system
      - _5-char-packagename_ + _random hash_ `/`

### `generators` folder

When running `conan install`, it creates a `generators` folder in your build folder, which contains:

- _packagename_ `-config.cmake` / _packagename_ `Config.cmake` and/or `Find` _packagename_ `.cmake` for all dependencies
  - Used by CMake's `find_package(` _packagename_ `)`
- `conan_toolchain.cmake`
  - Set compiler options
  - Set `CMAKE_*_PATH` to the right folders
  - Set `CMAKE_VS_DEBUGGER_ENVIRONMENT` so that Visual Studio can find DLLs when running a program
  - Set `CONAN_RUNTIME_LIB_DIRS` to library binaries
- `conanbuild.sh`: Source the script (`.`) to add build tools to path, e.g. `macdeployqt`:
  - `. ./generators/conanbuild.sh; macdeployqt cpp/pep/assessor/pepAssessor`
- `conanrun.sh`: Source the script to add runtime libraries to path, e.g. `QtCore`:
  - `. ./generators/conanrun.sh; cpp/pep/assessor/pepAssessor`
- `CMakePresets.json`
  - Set toolchain to `conan_toolchain.cmake`
  - Set some cache variables like `WITH_ASSESSOR` corresponding to `with_assessor` recipe option
  - Set `PATH` for build tools
  - Set `LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH` for libraries
  - ...
  - This file is included from `/CMakeUserPresets.json`

The actual binaries and library include files of dependencies stay in `~/.conan2`.

## Adding a new dependency

- Find recipe in [ConanCenter's Web interface](https://conan.io/center)
- Use the information provided to add to our `conanfile.py` in `requirements`/`build_requirements` depending on if it's a build tool or not. Use a sensible version range (e.g. pin major version)
- Look at the recipe's `conan graph info` (`--requires=package/version`) or the `conanfile.py` on GitHub. Are there `options` we should specify? Put those in our `conanfile.py` (optional options disabling features we don't need should go though `_optional_opts`)
- Add the `find_package` from the bottom of the ConanCenter page to our `CMakeLists.txt`. Case is important here! If we require specific components, you can require these using `find_package`'s `COMPONENTS` as well. If this info is missing on the web page, look at what `conan install` prints at the end.
- Link to our target in `CMakeLists.txt` using the `target_link_libraries` from the bottom of the ConanCenter page, or specify more specific components if we only need those. Note that the target name can differ from the package name, and case is also important here! If this info is missing on the web page, look at what `conan install` prints at the end.
- Re-run `conan install`, CMake configure, build
- When pushing, make sure to update [docker-build](#docker-build) as well

## Versioning

- Exact recipe can be identified with: (most fields are optional)

  ```plaintext
  name/version@user/channel#recipe_revision%recipe_timestamp
  ```

  - E.g. in `conan.lock`: `qt/6.7.1#3ce06ebc401937e53710f271ce89be6d%1717666193.215`
  - Often versions can be [ranges](https://docs.conan.io/2/tutorial/versioning/version_ranges.html#tutorial-version-ranges-expressions), e.g. `qt/[~6]` (6.x) or `qt/[^6.7]` (6.x but at least 6.7)
  - [Revisions](https://docs.conan.io/2/tutorial/versioning/revisions.html) are changes without the underlying source (e.g. `qt`) changing, e.g. a change in the build method or available options
- Exact built package can be identified by appending suffix to recipe:

  ```plaintext
  :package_id#package_revision%package_timestamp
  ```

  - E.g. logged by `conan install` for a cached package: `qt/6.7.1#3ce06ebc401937e53710f271ce89be6d:1167fcab096cf9e0f7e72334b61a4bc00b23eaf4#1eb51fdab5617db31950c7d7de5886f4`
  - Depends on [some settings & options](https://docs.conan.io/2/reference/binary_model/settings_and_options.html)
    - Important: if you change a *conf*, e.g. `tools.build:cxxflags` packages will *not* necessarily be rebuilt! Add the names of these confs to `tools.info.package_id:confs` to make sure they are taken into account. As a crude way you can also pass e.g. `--build='boost/*'` to force a rebuild.
- Versions can be pinned using [lockfiles](https://docs.conan.io/2/tutorial/versioning/lockfiles.html)
  - Exact dependencies can differ per platform/configuration
  - Our Conan lockfile is at `docker-build/builder/conan/conan-ci.lock` and not automatically used on devboxes

## `docker-build`

Our [`conanfile.py`](https://gitlab.pep.cs.ru.nl/pep/docker-build/-/blob/main/builder/conan/conanfile.py) is in the `docker-build` submodule, which builds Docker images containing the profile and built dependencies in the home folder. These images are linked to from core via `RUNNER_IMAGE_TAG` in `/ci_cd/docker-common.yml`, which should correspond with the submodule commit.

Scheduled pipelines for updates run on the `main`, `stable`, and `latest-release` branches of docker-build, and on the various `release-X.Y` branches. `main`/`stable`/`latest-release`/`release-X.Y` branches of core should thus always refer to the corresponding docker-build branches, to pick up the updated images. Keep in mind that merging in docker-build may create an extra merge commit, and core should then refer to that commit.
If new revisions are available, the docker-build CI commits the updated `conan-ci.lock` and updates core as well.

## Dynamic linking & CMake install

Use `-o 'pkg/*:shared=True'` to link _pkg_ dynamically. `cmake --install` can be used to copy our binaries plus required dynamic libraries (excluding Qt plugins).

## Platform requires

Use [`[platform_tool_requires]`](https://docs.conan.io/2/reference/config_files/profiles.html#platform-tool-requires) in the profile to prevent Conan from installing tools already present on your system, such as CMake or automake. However, this can give problems when using lockfiles and sometimes versions conflict or recipes won't work.

## Show info on dependencies

Use `conan graph info` to show info on all dependencies. E.g.:

```shell
conan graph info ./ --format=html >./conan.html
```

## Inherit from presets

You can inherit from presets generated by Conan in your `CMakeUserPresets.json` file. E.g.:

```json
{"configurePresets": [
 {
  "name": "ppp-acc-debug",
  "displayName": "PPP Acc + Debug",
  "inherits": "conan-debug",
  "cacheVariables": {
   "PEP_INFRA_DIR": "~/pep/ppp-config/config/acc"
  }
 }
]}
```

## Troubleshooting

_Please expand if you find a tricky issue!_

- `QtHostInfo` not found: make sure the `QT_HOST_PATH` environment variable is not set when doing a `conan install` (e.g. from calling `build/generators/conanbuild`). Open a new terminal to be sure.

Many CMake configure-time issues are solved by deleting `build/CMakeCache.txt` and `build/generators/`, then rerunning `conan install` and then again configuring with the generated CMake preset.

- If a library (version) dependency cannot be satisfied on a specific machine, ensure (that your Conan software is up to date and) that you're using the correct address for the remote: `conan remote update conancenter --url https://center2.conan.io` at the time of writing. See [this ticket](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2616) for details (and a rant).


## Reference & issues

Conan docs: https://docs.conan.io/2/index.html.
<br/>Full reference: https://docs.conan.io/2/reference.html.

File issues with the Conan tool at https://github.com/conan-io/conan/issues.
<br/>File issues with ConanCenter packages at https://github.com/conan-io/conan-center-index/issues.
