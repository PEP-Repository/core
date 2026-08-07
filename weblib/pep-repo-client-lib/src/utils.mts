import type {ClassHandle} from 'pep-repo-client-wasm';

export function deleteObjects(objects: Iterable<ClassHandle>) {
  for (const obj of objects) {
    obj.delete();
  }
}

export async function deleteObjectsAsync(objects: AsyncIterable<ClassHandle>): Promise<void> {
  for await (const obj of objects) {
    obj.delete();
  }
}

/** Decode `data`, which may be backed by a SharedArrayBuffer, as UTF-8 */
export function binaryToString(data: Uint8Array): string {
  const constBuffer = data.buffer instanceof SharedArrayBuffer
    ? new Uint8Array(data)
    : data;
  // Decode UTF-8
  return new TextDecoder().decode(constBuffer);
}

export async function concatStringsAsync(stream: AsyncIterable<string>): Promise<string> {
  let content = '';
  for await (const chunk of stream) {
    content += chunk;
  }
  return content;
}

/** Decode UTF-8 byte stream a single string */
export function byteStreamToString(stream: ReadableStream<Uint8Array<ArrayBuffer>>) {
  return concatStringsAsync(stream.pipeThrough(new TextDecoderStream()));
}
