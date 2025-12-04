///XXX Workarounds for missing types

declare namespace WebAssembly {
  /** @see https://developer.mozilla.org/en-US/docs/WebAssembly/Reference/JavaScript_interface/Exception */
  class Exception {
    stack?: string | undefined;
    message?: [string, string] | unknown;
  }
}

declare interface AllowedFileType {
  description?: string;
  accept: Record<string, string[]>;
}

declare interface SaveFilePickerOptions {
  excludeAcceptAllOption?: boolean;
  id?: any;
  startIn?: FileSystemHandle;
  suggestedName?: string;
  types?: AllowedFileType[];
}

declare function showSaveFilePicker(options?: SaveFilePickerOptions): Promise<FileSystemFileHandle>;
