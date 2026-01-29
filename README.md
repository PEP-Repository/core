# PEP

PEP is an acronym for "Polymorphic Encryption and Pseudonymization". The software provides a data repository featuring secure and privacy friendly data storage and dissemination. More information is available on the [project home page](https://pep.cs.ru.nl/), which also contains a link to the [user documentation](https://docs.pages.pep.cs.ru.nl/public/core/main/).

## Building

### Building on *nix

1. Install required packages (ubuntu packages in monospace)
   - `git`
   - conan (e.g. via `pipx`)
   - `cmake` 3.28 or newer (see https://apt.kitware.com/ for how to get this for older versions of debian-based OSs)
   - ninja (`ninja-build`)
   - `clang` (v18 is known to work (01-2024); see https://apt.llvm.org/ for how to get recent versions for older versions of debian-based OSs) (`g++` supported but not recommended for compiler performance reasons related to templates & RxCpp)
   - C++ STL, e.g. `libstdc++-dev` (v13 is known to work (01-2024), see <https://launchpad.net/~ubuntu-toolchain-r/+archive/ubuntu/test> for how to get recent versions like `libstdc++-13-dev` on older Ubuntu-based OSs)
   - For Qt GUI: APT packages can be installed via Conan, or pass `-o'&:use_system_qt=False'` to `conan install` to use Conan package instead
   - For Go watchdog:
     - Install the `golang` package (version 1.21 or newer. See <https://launchpad.net/%7Elongsleep/+archive/ubuntu/golang-backports>, if you use a linux distro based on Ubuntu 22.04)
     - set environment variables (e.g. in `.bashrc` or `.zshrc`):

       ```shell
       export GOPATH="$HOME/go"
       export PATH="$GOPATH/bin:$PATH"
       ```

     - install the Go protobuf compiler:

       ```shell
       go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
       ```

2. Clone repository including submodules (`--recurse-submodules`)
3. Create Conan profile:

   ```shell
   CC=clang CXX=clang++ ./docker-build/builder/conan/init_conan_profile.sh
   ```

   Note: Conan <2.3 [may not](https://github.com/conan-io/conan/issues/8866) pick up your default compiler (`cc`/`c++`) without `CC`/`CXX`.
4. Build dependencies and [generate CMake presets](https://docs.conan.io/2/examples/tools/cmake/cmake_toolchain/build_project_cmake_presets.html):

   ```shell
   conan install ./ --build=missing -s:a build_type=Debug
   ```

   - This creates a build folder like `./build/Debug` on your system. `Debug` may also be e.g. `Release`. See [`./conanfile.py`](./conanfile.py) for options that you can pass via `-o name=value`, e.g. `-o with_assessor=False` to disable the Qt GUI. These options can also be put in your profile under `[options]` as e.g. `pep/*:with_assessor=False`.
   - If you are not going to use `windeployqt`/`maydeployqt`, you can also pass `-s build_type=Debug` instead of `-s:a ...`, to build tools like `protoc` as `Release`.
   - It is also possible to build dependencies as `Release` but PEP as `Debug`. For this, use `conan install ./ --build=missing -s:a build_type=Release -s "&:build_type=Debug"` (`&` means the consuming package, so pep).
   - You only need to repeat this command if you: delete the build folder, change `conanfile.py`, change your compiler, or want to update dependencies (pass `--update`).
   - If this fails, you can try using the lockfile that the CI uses, via `--lockfile=./docker-build/builder/conan/conan-ci.lock`.
5. Configure using cmake:

    ```shell
    cmake --preset conan-debug
    ```

   (or e.g. `conan-release`). If you have an older CMake version, you can instead use the CMake configure command that Conan prints at the end.
   - Make sure you see CMake print that the Conan toolchain is used. If it's not, delete `build/Debug/CMakeCache.txt` and reconfigure; this can happen if you configured before without using the Conan preset/toolchain.
6. Build:

    ```shell
    cmake --build --preset conan-debug
    ```

    (Use `--target <...> [...]` to specify targets to build, e.g. just `pepcli`). If you have an older CMake version, you can specify the build folder (e.g. `./build/Debug`) instead of the preset. You could also e.g. call `ninja` in the build folder.
7. _Run servers locally:_

    ```shell
    cd ./build/Debug/cpp/pep/servers/ && ./pepServers
    ```

8. _Interact with cli:_

    ```shell
    ./build/Debug/cpp/pep/cli/pepcli --help
    ```

9. To enable autocompletion for the CLI tools, see [`autocomplete/README.md`](./autocomplete/README.md).

#### Separate build folders

You can create separate build folders using `-o subbuild_name <...>`, for example:

```shell
conan install ./ --build=missing -o subbuild_name=ppp -o cmake_variables="CASTOR_API_KEY_DIR=~/pep/ops/keys/;PEP_INFRA_DIR=~/pep/ppp-config/config/acc/;PEP_PROJECT_DIR=~/pep/ppp-config/config/acc/project/"
```

### Building on Windows

Building on Windows can be done very similar to on \*nix.

1. Install prerequisites such as git and a toolchain.
2. Install Conan: see [Conan documentation](docs/public/development/cpp/conan.md), or via `winget install conan`
3. Optionally (recommended for ease of use) enable creation of symbolic links:
   1. First in the Windows settings by enabling 'Developer Mode'
   2. Then in git via `git config --global core.symlinks true` (if you did this with an existing repo, you may need to remove the `--global` flag and do a `git restore ./conanfile.py` after)
   3. **Alternatively**: use `./docker-build/builder/conan/conanfile.py` instead of `./` in `conan install` (or just use `.\scripts\cmake-vs.bat`)
4. Clone repository including submodules (`--recurse-submodules`)
5. When using Visual Studio, you may use `.\scripts\cmake-vs.bat` inside a Visual Studio command prompt and open the generated solution. Otherwise, proceed like on \*nix
   - To update dependencies, pass `--update` as the `Conan args` when invoking the `cmake-vs.bat` frontend.

If the (Conan-based) meson build fails because your Python executable "is not a valid python or it is missing distutils", then run "python -m pip install --upgrade setuptools" to fix things. See https://stackoverflow.com/a/78050586 .

Multiconfig builds (using the same folder for Debug & Release) are currently not supported, so always pass a build type.

### Useful git config options

To ease the job of working with the docker-build submodule, you may want to execute these git commands:

```shell
git config --global submodule.recurse true
git config --global submodule.propagatebranches true
git config --global status.submodulesummary true
```

## Contributing

See the separate [instructions and guidelines](CONTRIBUTING.md) if you wish to contribute to the PEP project.
