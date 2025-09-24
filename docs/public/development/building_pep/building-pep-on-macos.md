# Building PEP on macOS

## The following instructions were successful on a M2 mac, with macOS Sonoma 14.1.1

1. Download Homebrew by copying the following line from <https://brew.sh> to Terminal:

    ```plaintext
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    ```

    Homebrew will also install the Command Line Tools, which includes software essential for developers such as Git and clang.

2. After completing the install, run the following two commands in your terminal to add Homebrew to your PATH:

    ```plaintext
    (echo; echo 'eval "$(/opt/homebrew/bin/brew shellenv)"') >> /Users/<username>/.zprofile
    eval "$(/opt/homebrew/bin/brew shellenv)"
    ```

    where `<username>` should be replaced with your username. These commands export environment variables needed for brew to work to .zprofile, a file that runs in your (Z-)shell when signing in as user.

3. Next, the following packages should be installed:

    ```plaintext
    brew install cmake
    brew install ninja
    brew install openssl
    brew install qt@6
    brew install boost
    brew install jq
    ```

4. You should now be able to build PEP.

## Additional tweaks

To speed up recompilation time run:

```plaintext
brew install ccache
```

Set cmake to use ccache as compiler-launcher by running the following line from the build directory:

```plaintext
cmake -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache ..
```

## The following instructions were successful on macOS Sierra

1. Install [brew](https://brew.sh/), xcode (currently macports has an outdated version of the boost lib).
2. Though brew install the following packages:

    ```plaintext
    brew install protobuf
    brew install qt6
    brew install boost
    brew install cmake
    brew install openssl
    ```

    Or use MacPorts: protobuf3-cpp, qt5, boost -no_static, cmake.

3. From the core directory run:

    ```plaintext
    git submodule sync
    git submodule update --init
    ```

4. Start compiling.

## Potential errors when building PEP core

## Incorrect homebrew path

```plaintext
(echo; echo 'eval "$(/usr/local/bin/brew shellenv)"') >> /Users/<username>/.zprofile
eval "$(/usr/local/bin/brew shellenv)"
```

### Library location conflict (OpenSSL and Qt5)

There can be conflicts between the OpenSSL and Qt6 libraries provided by the macOS system and those available through Homebrew. Explicitly specify the libraries' locations to resolve these conflicts.

```plaintext
cmake \
    -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) \
    -DOPENSSL_CRYPTO_LIBRARY=$(brew --prefix openssl)/lib/libcrypto.dylib \
    -DOPENSSL_SSL_LIBRARY=$(brew --prefix openssl)/lib//libssl.dylib \
    -DQt6_DIR=$(brew --prefix qt)/lib/cmake/Qt6 \
     <source directory>
```

or setup the environment variable:

```plaintext
CMAKE_PREFIX_PATH=/usr/local/opt/qt:/usr/local/opt/openssl
```

MacPorts has a similar issue; in this case setting `CMAKE_PREFIX_PATH=/opt/local` does the trick.

If you don't want to do it explicitly via CMake, you can set up global environment variables for finding the path to OpenSSL and Qt5 libraries. You can set these variables in the `.zprofile` file in your home directory. You can access this file via your command line. Add

```plaintext
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
export Qt6_DIR=$(brew --prefix qt6)/lib/cmake/Qt6
```

to this file.

### arm64 architecture not supported

#### ninja

If you use a newer Mac, it can be the case that your CPU has an arm64 architecture, instead of x86_64. This is a problem, because there is a bug in ninja that doesn't support arm64. See the bug explained here: <https://discourse.cmake.org/t/building-application-using-externalproject-add-on-m1-cpu/4931/7>. This bug has already been evaded in the code base, by setting the CMake generator to your default generator instead of using ninja. But it can be the case that your IDE negates this. If you use VS code, you can change this in the settings.

1. Go to Extensions \>\> CMake Tools
2. Click the small gear, and go to Extension Settings
3. Search for "Generator"
4. Fill in `Unix Makefiles` in the CMake: Generator field.

#### boost

If you use the `boost::filesystem` library you can get the error `Undefined symbols for architecture arm64`. This is because the boost filesystem is not linked. You can link it in your `tasks.json` file:

Add `"-lboost_system", "-lboost_filesystem","-I/opt/homebrew/include", "-L/opt/homebrew/lib",` as arguments to `"args"`.

### Include errors

If you use VS code, it can be the case that not all CMake headers will be included. Add

```plaintext
"configurationProvider": "ms-vscode.cmake-tools"
```

as an argument in `configurations` in the `c_cpp_properties.json` file. You can find this file in your `.vscode` folder.

You can also get the warning:

`include file not found in browse.path`

add

```plaintext
  "browse": {
  "path": ["${workspaceFolder}/**"],
  "limitSymbolsToIncludedHeaders": true,
  "databaseFilename": ""
}
```

as an argument in the `c_cpp_properties.json` file.
