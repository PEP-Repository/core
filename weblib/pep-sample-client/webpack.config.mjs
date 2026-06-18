import path from 'node:path';
import webpack from 'webpack';
import CompressionPlugin from 'compression-webpack-plugin';

/**
 * @param {Object.<string, *>} env
 * @param {webpack.Configuration} argv
 * @return {webpack.Configuration}
 */
export default (env, argv) => ({
  entry: {
    'pep-repo-client': 'pep-repo-client',
    'pep-repo-client-utils': 'pep-repo-client/utils',
  },
  experiments: {
    outputModule: true,
  },
  output: {
    clean: {
      // Delete only temporary files (with hex names and .gz from CompressionPlugin)
      keep: /^(?![a-h0-9]{20}\.)(?!.*\.gz$)/,
    },
    library: {
      type: 'modern-module',
    },
    chunkFormat: 'module',
    path: path.resolve(import.meta.dirname, 'dist'),
  },
  module: {
    parser: {
      javascript: {
        // Leave import.meta.url alone
        importMeta: false,
      },
    },
  },
  devtool: 'source-map',
  // Enable filesystem cache in non-watch development mode
  ...(argv.mode === 'development' && !argv.watch ? {
    cache: {
      type: 'filesystem',
    }
  } : {}),
  plugins: [
    new webpack.DefinePlugin({
      // Optimization: Make ENVIRONMENT_IS_NODE evaluate to false at compile time
      'globalThis.process': undefined,
    }),
    // Do not try to bundle stuff for running with Node.js
    new webpack.IgnorePlugin({
      resourceRegExp: /^node:|^(module|worker_threads)$/,
    }),
    ...(argv.mode === 'production' ? [
      // Pre-compress for Nginx `gzip_static`
      new CompressionPlugin({
        threshold: 1 << 20, // 1 MiB
      }),
    ] : []),
  ],
});
