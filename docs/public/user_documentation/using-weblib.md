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

# <u>PEP Weblib: using the JavaScript API</u>

# Structure

- C++ `/cpp/pep/weblib/Weblib.cpp` class
- Compiled to `pepWeblib.wasm` WebAssembly file with `pepWeblib.mjs` JavaScript module
  - `.mjs` loads `.wasm` and exposes functions and contains Emscripten runtime (supporting functionality, e.g. spawning threads, JS interop, etc.)
* `/weblib/pep-repo-client-lib/`
  - `pep.mts` TypeScript wrapper exposing `Pep` class
    - Also has supporting functionality (e.g. fetching config, opening popup, etc.)
  - `utils.mts` stream utilities etc.

# Customer perspective

- (Future work:) We publish package with `.wasm` + TypeScript wrapper & utils (compiled to JS)
- Customer installs with NPM
- Hosts package files (may bundle with Webpack) or uses CDN + SubResource Integrity
- Imports package files in their script
- Points script to `ClientConfig.json`
- Script connects to PEP servers via WebSockets

# Hosting details

- [Required](https://web.dev/articles/coop-coep) HTTP headers (for `SharedArrayBuffer`):
  - `Cross-Origin-Embedder-Policy: require-corp`
  - `Cross-Origin-Opener-Policy: same-origin`
  - *Don't* use `<meta http-equiv=... />`
* Compression recommended
  - On-the-fly GZip fast:
    - Release: 9.6 MiB → 3.3 MiB
    - Debug: 68 + 304 MiB → 11 + 58 MiB
  - Precompressed recommended for high traffic (e.g. [Nginx `gzip_static`](https://nginx.org/en/docs/http/ngx_http_gzip_static_module.html#gzip_static))
* Sample Nginx & Webpack setup with compression in `/weblib/pep-sample-client/`
  - Used for development & demo

## Content-Security-Policy

- When using CSP, WASM compilation requires `script-src 'wasm-unsafe-eval';`
- Emscripten uses `eval`/`Function` to generate bindings for performance reasons
  - Requires `script-src 'unsafe-eval';`
  - We could disable, with performance cost

# Initialization

```js
import Pep from 'pep-repo-client';

const pep = await Pep.create({
  clientConfig: new URL('dist/ClientConfig.json', location.href),
  authLandingPage: new URL('pepWasmTest-auth-landing', location.href),
});
// Log in via popup
await pep.authenticate();
```

# Authentication

> `pep.authenticate()`

Opens Authserver dialog in popup, redirecting to `authLandingPage` afterwards.
This sends OAuth code or error back via [`BroadcastChannel`](https://developer.mozilla.org/en-US/docs/Web/API/BroadcastChannel) in `chan` URL parameter.

`ClientKeys.json` persisted via `IndexedDB`.

# Enumerate

```js
const entriesStream = pep.list({
  columnGroups: (await pep.listColumns()).map(cg => cg.name),
  subjectGroups: (await pep.listSubjectGroups()).map(sg => sg.name),
});
for await (using entry of entriesStream) {
  console.log(`${entry.subjectLocalPseudonym} ${entry.column}: ${entry.fileSize} B`)
}
```

# Retrieve

```js
import {byteStreamToString} from 'pep-repo-client/utils';

const entries = await Array.fromAsync(pep.list(...));
const dataStream = pep.retrieve(entries);
for await (using data of dataStream) {
  console.log(`${data.entry.subjectLocalPseudonym} ${data.entry.column}:`,
      await byteStreamToString(data.content));
}
```

Content should be read immediately: RxCpp will not stop sending.

Future work: Pause between file batches until reader catches up (pausing between pages impossible with RxCpp).

# Memory management

> `using obj = ...;`

Deletes backing C++ object on scope end.
Unsupported in Safari: instead, use `try`-`finally` with `obj.delete()`, or backport with TypeScript/Babel.

<div data-marpit-fragment markdown>

Stream loops should ideally look like this:

```js
import {deleteObjectsAsync} from 'pep-repo-client/utils';
try {
  for await (using obj of stream.values({preventCancel: true})) {
    ...
  }
} finally {
  await deleteObjectsAsync(stream);
}
```

Unfortunately, remaining objects otherwise leak when loop exits prematurely (e.g. exception).
</div>

# Saving large file: Chromium

```js
import {binaryToString} from 'pep-repo-client/utils';

const fileExtensionBin = entry.partialMetadataView().get('fileExtension');
const suggestedName = 'file' + (fileExtensionBin ? binaryToString(fileExtensionBin) : '');
```

```js
const file = await showSaveFilePicker({suggestedName});
await data.content.pipeTo(await file.createWritable());
```

# Saving large file: Firefox & Safari

```js
// Origin-Private File System (virtual)
const rootDir = await navigator.storage.getDirectory();
const tmpDir = await rootDir.getDirectoryHandle('tmp-retrieve', {create: true});
const tmpFile = await tmpDir.getFileHandle(`file-${crypto.randomUUID()}`, {create: true});
```

<div data-marpit-fragment markdown>

```js
await data.content.pipeTo(await tmpFile.createWritable());
```
</div>

<div data-marpit-fragment markdown>

```js
const blob = await tmpFile.getFile();
const blobUrl = URL.createObjectURL(blob);
const a = document.createElement('a');
a.href = blobUrl;
a.download = suggestedName;
a.click();
```
</div>

<div data-marpit-fragment markdown>

```js
// Unclear if a delay is required
setTimeout(() => {
  URL.revokeObjectURL(blobUrl);
  // Might want to make sure by deleting tmp-retrieve/ on page load
  void tmpDir.removeEntry(tmpFile.name);
}, 1_000);
```
</div>

---
Future work: Compare with (more complex) Service Worker implementation from SURFfilesender.

# Exceptions

Weblib functions may throw C++ exceptions represented in JS by `WebAssembly.Exception`.

<div data-marpit-fragment markdown>

Pass to `pep.handleWasmException(ex)`:

- Obtains error message
- Converts to regular JS `Error`
- Deletes backing C++ exception object

Or use wrapper: `pep.runHandleWasmException(() => pep.listColumns())`.

Future work: add extra wrapping layer that does this automatically?
</div>

# More future work

- Data Administrator functionality
- More cell operations:
  - File upload
  - Retrieve encrypted cell metadata
  - ⇒ Should API be split up in explicit `getTicket` (with modes) → `getKeys` → ... after all?
- Downloading folders (via ZIP/TAR if needed)
- Structure metadata
- Publishing package (and semver)

# Debugging

[Chromium](https://developer.chrome.com/docs/devtools/wasm/): Supports DWARF debug info from `pepWeblib.wasm.debug.wasm` (separated for performance) via extension.

Firefox & Safari: Support source map from `pepWeblib.wasm.map`. No variable names.
