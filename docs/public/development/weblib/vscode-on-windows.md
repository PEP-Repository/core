# Building

1. Download and install VSCode from https://code.visualstudio.com/download# . Choose the "System Installer" to prevent every user from having to store an instance in their `AppData` directory.
1. Install [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) and [WebAssembly DWARF Debugging](https://marketplace.visualstudio.com/items?itemName=ms-vscode.wasm-dwarf-debugging) extensions
1. Discard existing `build` directory contents.
1. `Install requirements via Conan` and `Configure PEP` as detailed in our `quickstart.md`.
1. Run `build\wasm32\Debug\generators\conanbuild.bat` to set environment variables, then start VSCode from that same command line: "C:\Program Files\Microsoft VS Code\Code.exe" . Unfortunately it'll spam some (apparently error) logging to this console.
1. Open the project directory in VSCode. Wait until the `OUTPUT` for `[cmake]` completes with `Build files have been written [...]`.
1. Click the `Build` button on the CMake toolbar (at the bottom of the IDE). In the same toolbar you can select the target that the `Build` button applies to.

# Debugging

1. Click the "Run and Debug" icon in the LHS panel, then click the "Create a launch.json file" link. Choose "Node.js" debugger from dropdown. (If the dropdown doesn't contain this entry, close all open documents and try again.)
1. Find the "sample `launch.json` configuration" in `developing.md` and paste its contents into the `launch.json` file as an entry in the `configurations` array.
1. Check the `Uncaught Exceptions` checkbox in the `RUN AND DEBUG` panel's `BREAKPOINTS` section. You may also want to set a breakpoint somewhere in (PEP) code.
    - Note that the debugger (apparently) takes a while to attach, causing breakpoints to be skipped if they are set on code that's executed early in the application lifecycle. You may want to delay execution of the code you're interested in, e.g. by adding a `std::this_thread::sleep_for(std::chrono::seconds(1));` call. Keep in mind that some (a.o. static initialization) code runs before entering the `main` function.
1. Click the `Start debugging` button from the main toolbar (at the top of the IDE). (Do not use the `Launch the debugger[...]` button on the CMake toolbar, since it'll produce a popup saying `No compiler found in cache file`.)
1. If you've also enabled breakpoints for `Caught Exceptions` (i.e. you've checked that checkbox), the debugger will break on a bunch of exceptions thrown from bootstrapping code: `throw new FS.ErrnoError(44);` . These exceptions are eaten by calling code (in `Object.mayCreate`) and can be ignored.
