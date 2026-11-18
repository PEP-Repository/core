/**
 * @file TODO(workaround) Workarounds for missing types
 */

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

/** @see https://developer.mozilla.org/en-US/docs/Web/API/Window/showSaveFilePicker */
declare function showSaveFilePicker(options?: SaveFilePickerOptions): Promise<FileSystemFileHandle>;
