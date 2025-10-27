Object.assign(Module, {
  /**
   * Used by `ObservableStream.cpp` to make `cancel()` a no-op after completion.
   * @param {ReadableStream} stream
   * @returns {ReadableStream} `stream`
   */
  withIndirectCancel(stream) {
    const originalCancel = stream.cancel;
    stream.cancel = reason => {
      // Only call if still present
      if (stream.cancel) {
        return originalCancel.call(stream, reason);
      } else {
        console.log('Skipping cancel for completed stream', reason);
      }
    };
    return stream;
  },
});
