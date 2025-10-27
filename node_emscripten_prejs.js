// Used by CLI applications when compiling for WebAssembly.
Module.preRun = () => {
  // Inherit environment variables
  Object.assign(ENV, process.env);
};
