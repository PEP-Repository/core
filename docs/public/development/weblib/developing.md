---
# You can convert this to an HTML slide deck with Marp: https://marp.app/
# Add --html for data-marpit-fragment support
marp: true
headingDivider: 2
#language=css
style: |
  section {
    tab-size: 2;
    align-content: unsafe center; /* Center overfull slides vertically */
  }
  .columns {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 1rem;
  }
---

# <u>PEP Weblib: development</u>

Get started with the [Quickstart manual](./quickstart.md).

For using the Weblib, see [_PEP Weblib: using the JavaScript API_](../../user_documentation/using-weblib.md).

# [WebAssembly](https://webassembly.org/)

<img alt="WebAssembly logo" src="https://webassembly.org/css/webassembly.svg" height=100 />

- Cross-architecture machine code
- Instructions for virtual stack machine
- Lowered into instructions for your machine
- Sandboxed
- `.wasm` file [loaded](https://developer.mozilla.org/en-US/docs/WebAssembly/Guides/Loading_and_running) in browser using JavaScript `WebAssembly` API

# C++ SDK: [Emscripten](https://emscripten.org/)

<img alt="Emscripten logo" src="https://emscripten.org/_static/Emscripten_logo_full.png" height=100 />

Install [Emscripten SDK](https://github.com/emscripten-core/emsdk) (EMSDK) and activate in shell.

Build tools:

* LLVM Clang compiler with `WebAssembly` backend
* Supporting binaries
  - E.g. `wasm-ld` (linker), `wasm-emscripten-finalize`, etc.
  - `wasm-opt` [Binaryen](https://github.com/WebAssembly/binaryen) WebAssembly optimizer
* `emcc` & `em++` Emscripten compiler: Python wrappers around Clang
* CMake toolchain `Emscripten.cmake`

---
Runtime:

- LLVM libc++ STL, musl libc
- Emscripten runtime (with C++ & JS components)
- Node.js JavaScript runtime

---
Note: We're using 32-bit WASM.

- 64-bit is new, unstable, and unnecessary
- `std::size_t` is 32-bit and only for sizes *in memory*

# Compiling to WebAssembly

`hello.cpp`
```cpp
#include <iostream>
int main() { std::cout << "Hello, World!" << std::endl; }
```

**Targeting Node.js:**

```shell
em++ ./hello.cpp -o./hello
```

Outputs JavaScript file with Node.js shebang + `.wasm` file.

```
$ ./hello
Hello, World!
```

---
**Targeting browser:**

```shell
em++ ./hello.cpp -o./hello.html
```

Outputs HTML shell loading `.js` file + `.wasm` file.
Opening in browser via `file:` URL does not work, use [emrun](#emrun) or a local webserver instead.

For production, use `-o./hello.js` or `-o./hello.mjs` (module) instead, using custom HTML.

# Using with Conan

Use [`docker-build/builder/conan/conan_profile_wasm32`](https://gitlab.pep.cs.ru.nl/pep/docker-build/-/blob/wasm/builder/conan/conan_profile_wasm32?ref_type=heads)

- Sets compiler, flags, CMake toolchain, etc.
- Possible to install EMSDK via new [conan-toolchains `emsdk`](https://github.com/conan-io/conan-toolchains/tree/main/conan_config/profiles/emsdk) build tool recipe
  - Add remote, then `emsdk/<version>` under `[tool_requires]`

## Installing requirements via Conan

```shell
# --profile == --profile:host: the target platform
#   Also add --profile:build=./docker-build/builder/conan/conan_profile
#   if you have no default profile installed
# subbuild_name: pep option: build in ./build/wasm32/Debug/
conan install \
  --profile=./docker-build/builder/conan/conan_profile_wasm32 \
  -s"&:build_type=Debug" \
  --build=missing \
  -o'&:subbuild_name=wasm32'
```

# Building PEP

```shell
# PKI_DIR should point to the pepServers build
cmake --preset=wasm32-debug -DPKI_DIR=./build/Debug/pki/
cmake --build --preset=wasm32-debug
```

Building on pep-build9-devbox recommended.

# CMake targets

- `pepWeblib`: `/cpp/pep/weblib/Weblib.cpp` class
  - Compiled to `pepWeblib.wasm` WebAssembly file with `pepWeblib.mjs` JavaScript module
* `pepWeblibJs`: `/weblib/pep-repo-client-lib/` TypeScript wrapper
* `pepWeblibSampleClient`: `/weblib/pep-sample-client/` sample client page

# Node.js packages

- NPM: Node package manager (abbreviation unrelated)
* `package.json` (+ `package-lock.json`)
  - (dev)dependencies
  - Scripts (e.g. `npm run build:watch`)
* Builds in source folder (under `dist/`)
* `pepWeblibJs` (NPM `pep-repo-client-lib`)
  - Depends on e.g. `/build/wasm32/Debug/cpp/pep/weblib/` symlinked to `/weblib/pep-repo-client-wasm`
  - Compile TypeScript to JavaScript
  - Future work: publish
    - `npm pack`: Bundle `pepWeblibJs` files into tarball
    - SemVer

---
- `pepWeblibSampleClient` (NPM `pep-sample-client`)
  - Depends on `/weblib/pep-repo-client-lib/`
  - Type-checks JavaScript `client.mjs`
  - Bundles with Webpack:
    - `pepWeblib.mjs`, `pepWeblib.wasm`, compiled `pep.mts` into `pep-repo-client.mjs` (+ included chunks)
    - compiled `utils.mts` into `pep-repo-client-utils.mjs`
  - `*.mjs.map` map to pre-bundled files

# [C++ ↔︎ JS interop](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/)

We're mostly using [Embind](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html)

## Exposing C++ functions to JS

`bind.cpp`
```cpp
#include <emscripten/bind.h>

static int add42(int i) { return i + 42; }

EMSCRIPTEN_BINDINGS(myBindings) {
  emscripten::function("add42", add42);
}
```

`em++ -lembind ./bind.cpp -o./bind.js`

<div data-marpit-fragment markdown>

```shell
$ node
> Module = require('./bind.js')
> Module.add42(37)
79
```

In browser `Module` is global (no `require` needed).
</div>

---
### Exposing as JS Module (ESM)

`em++ -lembind ./bind.cpp -o./bind.mjs`

Plus `app.mjs`
```js
import loadModule from './bind.mjs';
const Module = await loadModule();
console.log(Module.add42(37));
```

```shell
$ node ./app.mjs
79
```

## Value structs

```cpp
struct Subject {
  std::string firstName, lastName;
};

static void printSubject(const Subject &subject) {
  std::cout << subject.firstName << ' ' << subject.lastName << std::endl;
}

Subject makeSubject() { return {"Peppa", "Pig"}; }
```

<div data-marpit-fragment markdown>

```cpp
EMSCRIPTEN_BINDINGS(subject) {
  emscripten::value_object<Subject>("Subject")
    .field("firstName", &Subject::firstName)
    .field("lastName", &Subject::lastName)
  ;
  emscripten::function("printSubject", printSubject);
  emscripten::function("makeSubject", makeSubject);
}
```
</div>

---
```js
// Weird Duck
Module.printSubject({firstName: "Weird", lastName: "Duck"});
// { firstName: 'Peppa', lastName: 'Pig' }
console.log(Module.makeSubject());
```

Embind marshalls arguments.

## Classes

```cpp
class Widget {
  unsigned frobCount_{};
public:
  ~Widget() { std::clog << "~Widget\n"; }
  unsigned frobCount() const noexcept { return frobCount_; }
  void frob() { ++frobCount_; }
};
```

<div data-marpit-fragment markdown>

```cpp
EMSCRIPTEN_BINDINGS(widget) {
  emscripten::class_<Widget>("Widget")
    .constructor()
    .function("frob", &Widget::frob)
    .property("frobCount", &Widget::frobCount)
  ;
}
```

JS object just wraps C++ object allocated on heap.
</div>

---
```js
const widget = new Module.Widget();
try {
  for (let i = 0; i < 42; ++i) widget.frob();
  console.log(widget.frobCount); // 42
} finally {
  widget.delete(); // ~Widget
}
```

Or when [supported](https://caniuse.com/mdn-javascript_statements_using) by the browser:
```js
{
  using widget = new Module.Widget();
  for (let i = 0; i < 42; ++i) widget.frob();
  console.log(widget.frobCount); // 42
} // ~Widget
```

## Accessing JS objects from C++

Embind [`val`](https://emscripten.org/docs/api_reference/val.h.html):

```cpp
#include <emscripten/val.h>

void doIt(emscripten::val fun, emscripten::val arg) {
  emscripten::val::global("console").call<void>("log",
      fun(arg["arg"].as<int>() + 1));
}

EMSCRIPTEN_BINDINGS(doIt) {
  emscripten::function("doIt", doIt);
}
```

<div data-marpit-fragment markdown>

```js
Module.doIt(i => `Called with ${i}`, {arg: 41}); // Called with 42
```

`val` can only be accessed from thread that created it.
</div>

## Custom bindings

We have custom bindings to convert between:

- C++ `std::vector` ↔︎ JS `Array`
- C++ `std::unordered_map`/`std::map` ↔︎ JS `Map`

This unfortunately uses an internal Emscripten API.

# [Exceptions](https://emscripten.org/docs/porting/exceptions.html)

- Multiple EH models available (see [extra info](#extra-info))
- In our case JS catches `WebAssembly.Exception` (non-`Error`)
- `pep.handleWasmException`
  - Obtains error message (also in Release)
  - Converts to regular JS `Error`
  - Deletes backing C++ exception object

# Unix/POSIX API

- Emscripten presents itself as UNIX (e.g. defines `__unix__`)
- Runtime implements Unix/POSIX syscalls like `open`/`select`/`socket`/`pthread_create`/...

# [Threads](https://emscripten.org/docs/porting/pthreads.html)

JavaScript is single-threaded. JS variables can only be accessed by one thread.
But...

Web Workers exist

- Run in the background
- Communication via messages or `SharedArrayBuffer` (used for heap)

Emscripten maps PThreads to Workers.

## Threads in PEP weblib

- Should not block main browser thread
- `io_context` runs on background thread
- `main` exits, but all state remains and other threads remains alive until JS calls an exit function

# Asynchronous code

- C++ PEP: `rxcpp::observable`
  - `.subscribe` with callbacks
* JS single item: [`Promise`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)
  - `.then` with callbacks
  - `await` can be used in `async` function
* JS multi item: [`ReadableStream`](https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream) (more generally: [async iterable](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols#the_async_iterator_and_async_iterable_protocols))
  - e.g. `for await`

## C++ single-value `rxcpp::observable` to JS `Promise`

* Use Emscripten's `val` coroutine, which creates `Promise`
* Await `rxcpp::observable` via new observable awaiter implementation
* Modify the `val` coroutine such that it calls `.observe_on(browser main)` on each awaited observable

<div data-marpit-fragment markdown>

```cpp
WeblibApiPromise listColumns() {
  co_return co_await onIoThread()
    .flat_map([](const std::shared_ptr<Weblib>& self) {
      return self->client_->getAccessManagerProxy()
        ->getAccessibleColumns(true, { "read" });
    })
    .map([](const ColumnAccess& access) -> std::vector<ColumnGroup> {
      return ...;
    });
}
```

(Reverse not yet implemented)
</div>

## C++ multi-value `rxcpp::observable` to JS `ReadableStream`

- `pep::weblib::CreateReadableStream` returns `ReadableStream` `val`
- Stream source calls `controller.enqueue(v)` for each item
- No flow control: RxCpp lacks support

<div data-marpit-fragment markdown>

```cpp
auto list(ListQuery query) {
  return CreateReadableStreamOnMain(
    onIoThread()
    .flat_map([...](...) -> CellEntry { ... })
  );
}
```
</div>

# TypeScript wrapper

Emscripten compiler already outputs crude typing via [`--emit-tsd`](https://emscripten.org/docs/tools_reference/emcc.html#emcc-emit-tsd), e.g.:

```ts
export interface CellEntry extends ClassHandle {
  readonly fileSize: number;
  readonly subjectLocalPseudonym: string;
  readonly column: string;
  ...
  partialMetadataView(): any;
}
```

<div data-marpit-fragment markdown>

Some types missing, thus we add:

```ts
export interface CellEntry extends rawTypes.CellEntry {
  partialMetadataView(): Map<string, Uint8Array<SharedArrayBuffer> | undefined>;
}
```
</div>

---
For functions:

```ts
export default class Pep {
  ...
  list(query: ListQuery): ReadableStream<CellEntry> {
    return this.#wrapExec(() => this.#client.list(query));
  }
}
```

(`wrapExec` tracks busy state, may be removed.)

# [Debugging](https://emscripten.org/docs/porting/Debugging.html)

## Debug info

- DWARF (`-g`)
  - In `.wasm` or separate `.wasm.debug.wasm` via `-gseparate-dwarf`
* Source map (`-gsource-map`)
  - `.wasm.map` maps offsets to original files & lines
* Name section in `.wasm` includes function names
* For mangled names, use [demangler.io](https://demangler.io/) or `llvm-cxxfilt`

## In browser

- [Chromium](https://developer.chrome.com/docs/devtools/wasm/): use DWARF via extension
  - Substitute paths via extension settings (see [extra info](#extra-info))
- Firefox/Safari: use source maps (for now)

## Node.js

- [VSCode](https://code.visualstudio.com/docs/nodejs/nodejs-debugging#_debugging-webassembly): use DWARF via extension
  - Substitute paths via `sourceMapPathOverrides`
  - See [extra info](#extra-info) for sample `launch.json` configuration
- Attaching Chrome debugger also possible via `node --inspect-wait`, but doesn't seem to find source files

## Assertions

Emscripten [compiler settings](https://emscripten.org/docs/tools_reference/settings_reference.html) start with `-s`.

- `-sASSERTIONS=2` (`CMAKE_EXE_LINKER_FLAGS_DEBUG`)
* `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG` (`CMAKE_CXX_FLAGS_DEBUG`)
* Clang sanitizers (ASan seems broken for us currently?)

## Logging

- `-sEXCEPTION_DEBUG`, `-sPTHREADS_DEBUG`, `-sSYSCALL_DEBUG`, `-sFETCH_DEBUG`, `-sSOCKET_DEBUG`, `-sWEBSOCKET_DEBUG` (`CMAKE_EXE_LINKER_FLAGS_DEBUG`)
- Enable 'debug' log level in browser console for verbose PEP logs

# [Sockets](https://emscripten.org/docs/porting/networking.html#emscripten-websockets-api)

- Emscripten translates POSIX sockets to WebSockets
- Direct sockets not supported in browser ([yet?](https://github.com/WICG/direct-sockets/blob/main/docs/explainer.md))
- WebSockets: connect with HTTP, then switch protocols
- Proxied by websockify
- PEP speaks TLS (via OpenSSL) over plaintext WebSocket (`ws://`)

<div data-marpit-fragment markdown>

Future work:

- Support WebSockets directly in PEP
- Use secure WebSockets (requires [trusted CA](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2768))
</div>

---
Current issues:

- <4.0.23: High CPU usage because waiting for data [does not block](https://github.com/emscripten-core/emscripten/issues/25759)
- ≥4.0.23: [Does not unblock](https://github.com/emscripten-core/emscripten/issues/26192) when data is received

# Tests

- Existing unit tests
  - Run with Node.js or in browser (`-DPEP_EMSCRIPTEN_BROWSER=ON`)
* Weblib C++ unit tests
* Weblib (+ TS wrapper) integration tests
  - Mocha + Node.js
  - Ran from `integration.sh`
  - Browser also supported

# Developing

- Build `pepWeblibSampleClient` (incl. `pepWeblib` + `pepWeblibJs`)
  - Uses `emcc`, TypeScript, Webpack
- Start Nginx
- Start websockify
- Start `pepServers`

<div data-marpit-fragment markdown>

Convenience script: `start_dev.sh`

- Watches JS/TS sources & `pepWeblib`: Only explicitly build `pepWeblib`
</div>

## Remote development

Tip: Use pep-build9-devbox with VSCode / CLion remote IDE.

Forward ports via SSH to access from local browser:

```shell
ssh -L localhost:XXX:localhost:XXX -L ... you@pep-build9-devbox
```

Or via config: `LocalForward localhost:XXX localhost:XXX`.

Ports: 2280, 8080, 15501, 15519, 15511, 15516, 15518, 15512.

(More reliable than IDE forward.)

# OAuth

- As mentioned in [_PEP Weblib: using the JavaScript API_](../../user_documentation/using-weblib.md), we use popup + `BroadcastChannel`
- C++ uses Web API to request `/token` on PEP Authserver
  - [CORS](https://developer.mozilla.org/en-US/docs/Glossary/CORS): Server sends `Access-Control-Allow-Origin`
  - Allowed origins from `ExtraRedirectUris` in `Authserver.json`
  - Future work: Use global metadata

# Finally

- Filing issues: https://github.com/emscripten-core/emscripten/issues

---
# Extra info

- Browser support: https://webassembly.org/features/

# [Exception](https://emscripten.org/docs/porting/exceptions.html) handling

Multiple models:

- No exceptions (`-fno-exceptions`)
* Emscripten EH (`-fexceptions`)
  - Implemented via JS, overhead
* [WASM legacy EH](https://github.com/WebAssembly/exception-handling/blob/main/proposals/exception-handling/legacy/Exceptions.md) (`-fwasm-exceptions`)
  - Built-in WASM instructions (with C++/JS glue)
  - What we're using now
* [WASM standard EH](https://github.com/WebAssembly/exception-handling/blob/main/proposals/exception-handling/Exceptions.md) a.k.a. `try_table`/`exnref`
  (`-fwasm-exceptions -sNO_WASM_LEGACY_EXCEPTIONS`)
  - Most major browsers support as of late 2025

## Caught on JS side as...

| | `-sEXCEPTION_STACK_TRACES` | `-sNO_EXCEPTION_STACK_TRACES` |
|-|-|-|
| Emscripten EH | `CppException` (an `Error`) | number (pointer) |
| WASM EH | `WebAssembly.Exception`<br/>(+ `stack` & `message`) | `WebAssembly.Exception` |

`-sEXCEPTION_STACK_TRACES` is implied by `-sASSERTIONS` (default for unoptimized).

<div data-marpit-fragment markdown>

- Use `Module.getExceptionMessage` to obtain message
- Free C++ object by `Module.decrementExceptionRefcount`
- `pep.handleWasmException` does both and returns as `Error`

Reverse: C++ [*cannot*](https://github.com/emscripten-core/emscripten/issues/11496) catch JS exceptions using normal `try`-`catch`.
</div>

## `std::terminate`

- `std::set_terminate` handler [may receive `nullptr`](https://emscripten.org/docs/porting/exceptions.html#limitations-regarding-std-terminate) in some cases
- Solutions:
  - Exception breakpoints
  - Switching to Emscripten EH (needs workaround for [Boost issue](https://github.com/conan-io/conan-center-index/issues/28702))
    - Use `conan_profile_wasm32_emscripten_eh`

# [Blocking asynchronous code](https://emscripten.org/docs/porting/asyncify.html)

It's possible to make C++ code block on JS Promise.

- Emscripten: Asyncify
- JSPI (experimental)

But we already have async code (using `rxcpp::observable`), so we're not using this.

# Debugging

## In Chromium: redirect paths

You may need to substitute paths, especially when debugging remotely. E.g.:

- `/emsdk` → `http://localhost:2280/emsdk`
- `/home/you/core` → `http://localhost:2280`
- `/home/you/.conan2` → `http://localhost:2280/.conan2`

## Debugging Node.js with VSCode: sample `launch.json` configuration

```json
{
  "name": "pepUtilsUnitTests",
  "program": "${workspaceFolder}/build/wasm32/Debug/cpp/pep/utils/pepUtilsUnitTests",

  "outputCapture": "std",
  "type": "node",
  "request": "launch",
  "skipFiles": ["<node_internals>/**"],
  "sourceMapPathOverrides": {
    "file:///emsdk/emscripten/*": "${env:EMSDK}/upstream/emscripten/*",
    "file:///emsdk/*": "${env:EMSDK}/*"
  }
}
```

This assumes `node` is in your `PATH` and `EMSDK` is also in your environment. Most convenient is to start VSCode from a shell where this is the case.

Note: Extension doesn't seem to like `-gseparate-dwarf`.

## `emrun`

Run JS + WASM in browser from command line, capturing output.

- `-DPEP_EMSCRIPTEN_BROWSER=ON -DPEP_EMSCRIPTEN_EMRUN=ON`
- `emrun ./build/wasm32/Debug/cpp/pep/utils/pepUtilsUnitTests -- --gtest_filter='Base64*'`
