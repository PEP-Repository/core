# Setting up a PEP development machine on Windows

When working on MS-Windows, you'll probably want to develop using Microsoft Visual Studio. This page describes how to get such a dev box set up.

## Get privileged

While day-to-day development does not require special privileges, you'll need an Administrator account to install (some) of the required software. And as described under [the "Run" heading](#run), you'll also need administrative privileges once if you intend to run any PEP server processes.

On top of default (non-administrator) user privileges, all git users must have the ability to create symbolic links on the file system. Windows Administrators have this privilege by default, so you'll need no special settings if you plan to use git from an elevated context. Otherwise, allow git to create symlinks by enabling [developer mode](https://www.howtogeek.com/292914/what-is-developer-mode-in-windows-10/), which is available on most (or all?) editions of Windows 10, including Home Edition. On Enterprise or Ultimate editions, you can also assign the privilege to other users and/or groups:

- Start `gpedit.msc` (e.g. from the command line). The Local Group Policy Editor window appears.
- In the tree view, open category `Local Computer Policy` -> `Computer Configuration` -> `Windows Settings` -> `Security Settings` -> `Local Policies` -> `User Rights Assignment`.
- In the list of policies, open the `Create symbolic links` policy.
- Click the `Add User or Group...` button to bring up the `Select Users or Groups` dialog.

Use the `Select Users or Groups` dialog to look up the user(s) and/or group(s) that will need the privilege. Note that the `Object Types...` filter excludes groups by default, so you'll need to change it if you want to add e.g. the `Users` group.

The new policy will be applied when affected users log on the next time, so be sure to have users (such as yourself) log out. Also restart affected (service) processes such as Gitlab runner.

## Install

PEP requires multiple 3rd-party libraries and tools that are not included in the source repository. These should be downloaded and installed separately:

- [Microsoft Visual Studio](https://www.visualstudio.com/) (e.g. 2022 Community Edition), used to edit and debug the PEP code. Ensure you include the various C++ components in the installation.
  - Make sure you choose a Visual Studio version that's supported by (all) required libraries, such as Boost, OpenSSL, and QT. Acquiring older Visual Studio (Community Edition) versions may require some hoop jumping.
- [git](https://git-scm.com/downloads), used for source control. Configure it to
  - `Associate .sh files to be run with Bash`, and
  - `Checkout as-is, commit Unix-style` line endings, and
  - `Enable symbolic links`.
- Optional: the [Sourcetree](https://www.sourcetreeapp.com/) GUI frontend for git. Configure it to use the standalone git you just installed.
- [CMake](https://cmake.org/), used to generate the build system (i.e. Visual Studio projects on Windows). Depending on your Visual Studio version and the installed packages and/or workloads, a CMake version may be supplied by your Visual Studio installation. Run `where cmake` from a Visual Studio Developer Command Prompt to find out. Note that some of Visual Studio's CMake versions (e.g. the one included with VS2017 Community Edition) are incompatible with all versions of Boost. (Technically: Visual Studio's FindBoost.cmake supports Boost versions up to and including 1.65.1; Boost 1.65.1 with default settings supports compiler versions up to and including 19.11.25506; Visual Studio 2017 provides compiler version 19.13.26131.1.) If you install a standalone CMake version, make sure to let the installer add the binary directory to your path.
- [Go](https://golang.org), used to compile server monitoring utilities.

Some CI scripts will use Bash, where they first try Git Bash and otherwise Bash in path, see `/scripts/invoke-sh.ps1`. Note that by default the `bash` in path may be a wrapper around WSL.

If you also want to build MSI installer packages on your dev box, you'll also need

- [Windows Installer XML (WiX) toolset](http://wixtoolset.org/) which, at the time of writing, requires the .NET Framework 3.5, which can be installed as an optional feature in Windows 10.
- [jq](https://jqlang.github.io/jq/download/)
  - Use the winget, scoop, or choco commands to install jq
OR:
  - Download one of the executables.
  - Rename the downloaded exe to jq.exe
  - Create a folder for your app. e.g. C:\program files\jq
  - Copy jq.exe to your new folder
  - Open up your Windows system properties/environment variables and add your new folder to the PATH environment variable

## Configure

Add the `bin` directory for Git to your path. For example, if you installed Git to its default location `C:\Program Files\Git`, you'd add `C:\Program Files\Git\bin` to your path.

If you didn't let CMake's installer update your system's path, you may find it convenient to do so manually, allowing CMake to be invoked from a vanilla command prompt (as opposed to a Visual Studio Developer command prompt).

Install Go support for Google Protocol buffers: open a command line and run `go install github.com/golang/protobuf/protoc-gen-go@latest`. (The command used to be `go get github.com/golang/protobuf/protoc-gen-go` but `go get` has been [deprecated](https://go.dev/doc/go-get-install-deprecation)).

## Get source and create build system

(Use e.g. SourceTree to) clone the following repositories to local directories:

- `https://gitlab.pep.cs.ru.nl/pep/core` and
- `https://gitlab.pep.cs.ru.nl/pep/ops`.

Make sure to "Recurse submodules" (found under the "Advanced Options" in SourceTree's "Clone" dialog.) It's probably easiest if you clone to a path without spaces, e.g. `C:\Source\pep\core`. Note that PEP creates several (levels of) subdirectories at run time, which may cause paths to exceed the [maximum path length of 255 characters](https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file) if you clone into a directory that has a long-ish path to begin with.

Use CMake to generate the Visual Studio projects and solution that define the build procedure. Batch file (shell script) `cmake-vs.bat` takes care of most of the tedium for you, generating a 64-bit build environment. (We build for 64-bits because QT provides libraries only for that platform.)

To prevent the repository from getting cluttered with generated files, you'll want to drop the project files into a subdirectory:

```shell
cd PATH\TO\pep\core
mkdir build & cd build
..\scripts\cmake-vs.bat ..
```

Invoke the command without parameters to get command line help.

## Build

Open the generated `pep.sln` file in Visual Studio and build the solution.

All projects have a dependency on `ZERO_CHECK`, which updates affected project file(s) when the corresponding `CMakeLists.txt` has changed. The project will then be built using the updated project file, but Visual Studio won't prompt to reload affected project(s) until after the build has been completed. This will make it look like the build was based on outdated project file(s), but it's only the GUI that's lagging behind: your project outputs are up-to-date and you don't need to build again.

## Run

The `pepAssessor` project is the Windows client you'll probably want to run, so set it as your startup project. To allow debugging of the entire system, you'll want to run the various services from the debugger as well. This requires generation of the build system (as described under the `Prepare` header) for the `local` infrastructure. Then:

- Either start the individual service executables:
  - `pepAccessManager`
  - `pepKeyServer`
  - `pepRegistrationServer`
  - `pepStorageFacility`
  - `pepTranscryptor`
- Or start the `pepServers` execuable, which hosts all of the PEP service objects.

In the `local` configuration (i.e. on dev boxes after having built with default settings), `StorageFacility.json` is configured to use a built-in page store (large data storage). If you switch to an S3-backed page store (as hinted at within the `.json` file), you'll need to run an executable that provides the S3 interface on the configured `EndPoint`.

<!-- @@@ provide shell script to provide an S3 interface onto the file system @@@ -->

The various servers write their logging to the Windows Event Log under an event source named `pep`. If the source is not present (yet), the server will attempt to create the source during initialization. Since the creation of event sources requires administrative privileges, start a server process "as an administrator" one time. Once the event source has been created, server processes do not need to run in an administrator context.

## Work

While PEP's own C++ code can be maintained using Visual Studio, several projects use additional code that is generated by other tools, QT among them. Use QT Creator (which is part of the QT installation) to edit/design the `.ui` files found in directory `client/clientlib/ui_headers`. The corresponding `ui\_\*.cpp` and `ui\_\*.h` sources are then updated when building the `pepAssessor` project.
