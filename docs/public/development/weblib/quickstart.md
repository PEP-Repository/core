# Developing PEP Weblib: Quickstart

This page more concretely lays out the steps to get started developing the PEP Weblib. For more info, see the [_PEP Weblib: development_](./developing.md) document.

## Obtain the Emscripten SDK

!!! bug "Emscripten version"
    Currently, Emscripten ≥4.0.23 blocks too long while waiting for WebSocket traffic, see [this issue](https://github.com/emscripten-core/emscripten/issues/26192). Until this is resolved, use version 4.0.22 instead.

**Option 1: Manual install**

Just follow the steps in the [Emscripten docs](https://emscripten.org/docs/getting_started/downloads.html#installation-instructions-using-the-emsdk-recommended). Make sure to source the environment file in your build shell, and check if the `EMSDK` envvar is indeed set.

**Option 2: Via Conan**

Clone https://github.com/conan-io/conan-toolchains and it as Conan remote as described in their [README](https://github.com/conan-io/conan-toolchains?tab=readme-ov-file#-getting-started).

Now reference the [recipe](https://github.com/conan-io/conan-toolchains/tree/main/conan_config/profiles/emsdk) and inform Conan its compiler version by adding the following to the Conan host profile that you will be using for compiling WebAssembly (see next section). You may want to make a local copy that includes the profile from docker-build.

```ini
{% set compiler_version = '4.0.22' %}

[settings]
# This line should be below `compiler=...`
compiler.version={{compiler_version}}

[tool_requires]
emsdk/{{compiler_version}}
```

Substitute `4.0.22` by the specific version you want to use.

## Install requirements via Conan

As host profile, you can use `docker-build/builder/conan/conan_profile_wasm32`, which automatically detects the EMSDK in your environment.

!!! bug "Emscripten version detection"
  Currently, Emscripten version detection for manually installed EMSDK is broken (1) on Windows or (2) with a fresh install, see [this issue](https://github.com/conan-io/conan/issues/19677). For (1), manually specify `compiler.version`. For (2), the second try it should work.

```shell
conan install \
  --profile=./docker-build/builder/conan/conan_profile_wasm32 \
  -s"&:build_type=Debug" \
  --build=missing \
  -o"&:subbuild_name=wasm32"
```

- `--profile` == `--profile:host`: the target platform
- Also add `--profile:build=./docker-build/builder/conan/conan_profile` if you have no default build profile installed (see pep/core> README)
- `-s"&:build_type=Debug"`: Same as normal, this builds pep as Debug, but dependencies as Release according to the profile
- `subbuild_name` is a pep option: build in subfolder `./build/wasm32/Debug/`

## Configure PEP

```shell
cmake --preset=wasm32-debug -DPKI_DIR=./build/Debug/pki/
```

- `PKI_DIR` should point to an existing `pepServers` build if you want to test with local servers. Without this you will likely see `boost::system::system_error: certificate verify failed (SSL routines)` when connecting.

## Build & run sample page

Install Nginx and the [websockify](https://github.com/novnc/websockify) Python package (e.g. via [pipx](https://pipx.pypa.io/latest/installation/)). For Windows, use `winget install nginxinc.nginx`.

Build Debug & start local servers:

```shell
./weblib/start_dev.sh
```

When EMSDK was installed via Conan, you need to put Node.js in PATH by sourcing the `generators/conanbuild` script in your build folder.

!!! bug "Windows quirks"
    - Nginx may not go to the background or want to shut down with ctrl+C, kill it via the task manager instead.
    - When installing EMSDK via Conan, you may get "Command line too long" while building. In the emsdk recipe, replace the body of `_define_tool_var` with `return f"python -E \"{os.path.join(self._emscripten, f'{value}.py')}\""`.
    - websockify may log `WARNING: no 'resource' module, daemonizing is disabled`. This can be ignored.
