import path from 'node:path';

/**
 * @param {Object.<string, *>} env
 * @param {import('webpack').Configuration} argv
 * @return {import('webpack').Configuration}
 */
export default (env, argv) => ({
  entry: {
    'pep-repo-client': 'pep-repo-client',
    'pep-repo-client-utils': 'pep-repo-client/utils',
  },
  experiments: {
    outputModule: true,
  },
  // Do not try to bundle stuff for running with Node.js
  externals: /^node:|^(module|worker_threads)$/,
  output: {
    library: {
      type: 'modern-module',
    },
    chunkFormat: 'module',
    path: path.resolve(import.meta.dirname, 'dist'),
  },
  devtool: argv.mode === 'development' ? 'source-map' : false,
  // Enable filesystem cache in non-watch development mode
  ...(argv.mode === 'development' && !argv.watch ? {
    cache: {
      type: 'filesystem',
    }
  } : {}),
});
