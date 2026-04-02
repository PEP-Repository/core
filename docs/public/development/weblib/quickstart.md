# Developing PEP Weblib: Quickstart

This page more concretely lays out the steps to get started developing the PEP Weblib. For more info, see the [_PEP Weblib: development_](./developing.md) document.

## Install prerequisites

Install Nginx and the [websockify](https://github.com/novnc/websockify) Python package (e.g. via [pipx](https://pipx.pypa.io/latest/installation/)). For Windows, use `winget install nginxinc.nginx` (or `freenginx.nginx`).

## Obtain the Emscripten SDK

!!! bug "Emscripten version"
    Currently, Emscripten ≥4.0.23 blocks too long while waiting for WebSocket traffic, see [this issue](https://github.com/emscripten-core/emscripten/issues/26192). Until this is resolved, use version 4.0.22 instead.

!!! info "Node.js"
    When Node.js is already installed on the system (`node` is in PATH), EMSDK will not add its version to PATH, and we assume `npm` also available with your existing installation.

**Option 1: Manual install**

Just follow the steps in the [Emscripten docs](https://emscripten.org/docs/getting_started/downloads.html#installation-instructions-using-the-emsdk-recommended). Make sure to source the environment file in your build shell, and check if the `EMSDK` envvar is indeed set.

**Option 2: Via Conan**

Clone https://github.com/conan-io/conan-toolchains and set it up as a local recipe index repository as described in their [README](https://github.com/conan-io/conan-toolchains?tab=readme-ov-file#-getting-started).

Create a Conan host profile that you will be using to compile WebAssembly. E.g. place a file `wasm32` in `~/.conan2/profiles/` and give it the following contents:

```ini
{% set compiler_version = '4.0.22' %}

include(C:\PepSrc\pep\core-3\docker-build\builder\conan\conan_profile_wasm32)

[settings]
# This line should be below `compiler=...`
compiler.version={{compiler_version}}

[tool_requires]
emsdk/{{compiler_version}}
```

@@@ NOTE THAT I also put these settings directly in my C:\PepSrc\pep\core-3\docker-build\builder\conan\conan_profile_wasm32 file, so those may have been required for `conan install` (below) to work. @@@

Replace the `include` path by the actual path to the `conan_profile_wasm32` file in your `core/docker_build` subdirectory. Substitute `4.0.22` by the specific version you want to use.

!!! info "Adapting Conan profiles from docker-build"
    When adapting Conan profiles in docker-build, you can either copy them (plus included files) to `~/.conan2/profiles/`, or create a new profile there and include the one from docker-build via `include(/path/to/docker-build/conan_profile)`.

## Install requirements via Conan

As host profile, you can use or include `docker-build/builder/conan/conan_profile_wasm32`, which automatically detects the EMSDK in your environment.

!!! warning "Emscripten version detection"
    Please make sure you have Conan 2.27 or later for `detect_emcc_compiler` to work correctly.

!!! warning "Windows quirks"
    - You may get "Command line too long", e.g. when it calls `emar` while building OpenSSL. Work around this by editing the emsdk recipe in `<your-conan2-dir>/p/emsdkXXXXXXXXXXXX/e/conanfile.py` and replacing the body of `_define_tool_var` with `return f"python \"{os.path.join(self._emscripten, f'{value}.py')}\""`.
    - If the workaround for "Command line too long" has been applied, you may get "Could NOT find Threads (missing: Threads_FOUND)", e.g. when building protobuf. In this case, revert the body of `_define_tool_var` to its original state. In my case:

```python
    def _define_tool_var(self, value):
        suffix = ".bat" if self.settings.os == "Windows" else ""
        path = os.path.join(self._emscripten, f"{value}{suffix}")
        return path
```

From your git repository's root directory:

```shell
conan install \
  --profile=wasm32 \
  -s"&:build_type=Debug" \
  --build=missing \
  -o"&:subbuild_name=wasm32"
```

- `--profile` == `--profile:host`: the target platform
- Also add `--profile:build=./docker-build/builder/conan/conan_profile` if you have no default build profile installed (see pep/core> README)
- `-s"&:build_type=Debug"`: Same as normal, this builds pep as Debug, but dependencies as Release according to the profile
- `subbuild_name` is a pep option: build in subfolder `./build/wasm32/Debug/`

## Configure PEP

First configure PEP as you would normally do to develop PEP on your platform. E.g. on Windows:

```shell
cd build & ..\scripts\cmake-vs.bat .. ..\..\ops\keys
```

Then (`cd` back to the repo root and) configure the WASM build as shown below. The value for `PKI_DIR` should point to the PKI directory of the regular build that you just configured. This will allow you to test with local servers, without which you will likely see `boost::system::system_error: certificate verify failed (SSL routines)` when connecting.

```shell
cmake --preset=wasm32-debug -DPKI_DIR=./build/pki/
```

## Build & run sample page

!!! warning "Windows quirks"
    - To use scripts like `start_dev.sh`, you'll need to open e.g. Git Bash.
    - The `build\wasm32\Debug\generators\conanbuild.bat` script generated will not be compatible with Git Bash. Instead, source (run) it in the appropriate Windows shell and then start Git Bash from that same shell by invoking `"C:\Program Files\Git\bin\bash"`.
    - When using PowerShell, sourcing `conanbuild.bat` will not work. Instead, set e.g. `-c tools.env.virtualenv:powershell=...` according to [the docs](https://docs.conan.io/2/reference/config_files/global_conf.html) and source `conanbuild.ps1`.
    - Nginx may not go to the background or want to shut down with ctrl+C, kill it via the task manager instead.
    - When installing EMSDK via Conan, you may get "Command line too long", e.g. when it calls `emar` while building OpenSSL. In the emsdk recipe, patch this line in their `conanfile.py`:
      ```diff
      104c104
      <         self.buildenv_info.define_path("AR", self._define_tool_var("emar"))
      ---
      >         self.buildenv_info.define_path("AR", f'"{os.path.join(self.package_folder, "bin", "upstream", "bin", "llvm-ar")}"')
      ```
      And then add `--build="esmdk/*"` to the `conan install` command.
    - websockify may log `WARNING: no 'resource' module, daemonizing is disabled`. This can be ignored.

If you installed EMSDK via Conan, you need to put Node.js in PATH by sourcing the `generators/conanbuild` script from your `wasm32` build folder.

Build Debug & start local servers:

```shell
./weblib/pep-sample-client/start_dev.sh
```

Now you can finally open http://localhost:2280/weblib/pep-sample-client/ in your browser. Try to log in as Research Assessor and list columns, for example.
