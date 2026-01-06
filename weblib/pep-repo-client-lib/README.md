# WebAssembly library: TypeScript wrapper

- The C++ WebAssembly library gets built in [`/cpp/pep/weblib/`](../../cpp/pep/weblib/README.md).
- With the `pepWeblibJs` target, this build folder gets symlinked to `/weblib/pep-repo-client-wasm`, and consumed by `pep.mts`. Build output is in `./dist/`.

Make sure to set `-DPKI_DIR` to point to the `pki/` build folder from the server.
