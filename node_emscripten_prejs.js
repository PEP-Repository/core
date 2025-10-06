Module.preRun = () => {
  // Inherit environment variables
  Object.assign(ENV, process.env);
};
