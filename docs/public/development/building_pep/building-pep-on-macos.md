# Setting up a PEP development machine on macOS

## Install Homebrew and required packages

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
    brew install conan
    brew install jq
    brew install golang
    brew install protoc-gen-go
    brew install ccache
    ```

4. You should now be able to build PEP. Continue to the [Getting Started](index.md) guide to clone the repository and build PEP.
