import path from 'node:path';
import webpack from 'webpack';

/**
 * @param {Object.<string, *>} env
 * @param {webpack.Configuration} argv
 * @return {webpack.Configuration}
 */
export default (env, argv) => ({
  entry: [
      './src/pep.spec.mts',
  ],
  output: {
    clean: {
      // Delete only temporary files
      keep: /^(?![a-h0-9]{20}\.\w+$)/,
    },
    filename: 'tests.js',
    path: path.resolve(import.meta.dirname, 'dist-test'),
  },
  externals: [
    // Keep consistent with tests.html
    {
      mocha: 'script mocha@https://unpkg.com/mocha@11/mocha.js',
    },
  ],

  module: {
    rules: [
      {
        test: /\.m?ts$/,
        use: 'ts-loader',
        exclude: /node_modules/,
      },
    ],
  },
  resolve: {
    extensions: [".js", ".mjs", ".ts", ".mts"],
    extensionAlias: {
      ".js": [".ts", ".js"],
      ".mjs": [".mts", ".mjs"],
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
  ],
});
