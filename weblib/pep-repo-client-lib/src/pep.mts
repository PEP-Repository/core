import initModule, {MainModule, Weblib} from 'pep-repo-client-wasm';
import type * as rawTypes from 'pep-repo-client-wasm';

type ConfigServerName =
    | 'AccessManager'
    | 'StorageFacility'
    | 'KeyServer'
    | 'Transcryptor'
    | 'RegistrationServer'
    | 'Authserver'
    ;

interface ConfigServer {
  Name: string;
  Address: string;
  Port: number;
}

interface ConfigFiles {
  CACertificateFile: string;
  ShadowPublicKeyFile: string;
  ProjectConfigFile: string;
}

export type ClientConfig = {
  [server in ConfigServerName]: ConfigServer;
} & ConfigFiles & {
  KeysFile: string;
  PublicKeyData: string;
  PublicKeyPseudonyms: string;
  AuthenticationServer: {
    RequestURL: string;
    TokenURL: string;
    CaCertFilePath: string;
  };
  Castor: {
    BaseURL: string;
  };
};


export interface InitConfig {
  clientConfig: URL | ClientConfig;
  configFileContentOverrides?: { [key in keyof ConfigFiles]?: string | undefined };
  authLandingPage?: URL | null;
  onOut?: ((msg: string) => void) | null;
  onError?: ((msg: string) => void) | null;
}

export interface EnrolledUser {
  user: string;
  userGroup: string;
}

export interface ColumnGroup {
  name: string;
  columns: string[];
}

export interface SubjectGroup {
  name: string;
}

export interface ParticipantPersonalia {
  firstName: string;
  middleName: string;
  lastName: string;
  dateOfBirth: Date;
}

export interface ListQuery {
  subjectGroups?: string[] | undefined;
  subjects?: string[] | undefined;
  columnGroups?: string[] | undefined;
  columns?: string[] | undefined;
}

/**
 * @warning This object must be deleted after use
 */
export interface CellEntry extends rawTypes.CellEntry {
  /**
   * Get partial (unencrypted) cell metadata.
   * The `Uint8Array`s have the same lifetime as the `CellEntry`.
   * Entries are `undefined` if still encrypted.
   */
  partialMetadataView(): Map<string, Uint8Array<SharedArrayBuffer> | undefined>;
}

/**
 * A buffer containing binary data.
 * @warning This object must be deleted after use
 */
export interface Buffer extends rawTypes.Buffer {
  /** The `Uint8Array` has the same lifetime as the `Buffer`. */
  view(): Uint8Array<SharedArrayBuffer>;
}

/**
 * @warning This object must be deleted after use
 */
export interface CellData extends rawTypes.CellData {
  readonly entry: CellEntry;

  /**
   * Get the cell content. The chunks concatenated form the whole content.
   */
  readonly content: ReadableStream<Uint8Array<ArrayBuffer>>;
}

type AuthenticationChannelMessage =
    | { code: string }
    | { error: string, errorDescription: string | null }
    ;

function toDdMmYyyy(date: Date) {
  return `${date.getFullYear().toString().padStart(4, '0')}${date.getMonth().toString().padStart(2, '0')}${date.getDay().toString().padStart(2, '0')}`;
}

/** Are we using Emscripten EH instead of WASM EH? */
const EmscriptenExceptionHandling = false;

/**
 * Transforms a WASM exception into an Error with the correct message, and stack when available.
 * After obtaining message, frees the WASM exception object.
 */
function handleWasmExceptionForModule(mod: MainModule, wasmEx: WebAssembly.Exception): Error {
  if (EmscriptenExceptionHandling) {
    //XXX When using Emscripten EH, we should call incrementExceptionRefcount first,
    // see https://github.com/emscripten-core/emscripten/issues/17115
    mod.incrementExceptionRefcount(wasmEx);
  }
  try {
    const [type, message] = mod.getExceptionMessage(wasmEx) as [string, string];
    const error = new Error(message || type, {cause: wasmEx});
    if (wasmEx.stack) {
      error.stack = wasmEx.stack;
    }
    console.warn(`WebAssembly.Exception: ${type}: ${message}${wasmEx.stack ? `\n${wasmEx.stack}` : ''}`);
    return error;
  } finally {
    mod.decrementExceptionRefcount(wasmEx);
  }
}

function throwPotentialWasmException(mod: MainModule, ex: WebAssembly.Exception | Error | unknown): never {
  if (ex instanceof WebAssembly.Exception) {
    throw handleWasmExceptionForModule(mod, ex);
  }
  throw ex;
}

function runHandleWasmExceptionForModule<Ret>(mod: MainModule, fun: () => Ret): Ret {
  try {
    const ret = fun();
    if (ret instanceof Promise) {
      return (ret as Ret & Promise<Ret>)
          .catch(ex => throwPotentialWasmException(mod, ex)) as Ret & Promise<Ret>;
    }
    return ret;
  } catch (ex) {
    throwPotentialWasmException(mod, ex);
  }
}

export default class Pep {
  readonly #config: InitConfig;
  #mod: MainModule;
  #client: Weblib;
  #busy: number = 0;
  #onBusyChange: ((busy: boolean) => void) | null = null;

  private constructor(config: InitConfig, wasm: MainModule) {
    this.#config = config;
    this.#mod = wasm;
    this.#client = this.runHandleWasmException(() => new wasm.Weblib());
  }

  static async #addConfigFile(
      Module: MainModule,
      baseUrl: URL | null,
      contentOverrides: InitConfig['configFileContentOverrides'],
      configFiles: ConfigFiles,
      key: keyof ConfigFiles,
      fileName: string
  ) {
    const contentOverride = contentOverrides?.[key];
    if (contentOverride) {
      Module.FS.createDataFile('/', fileName, contentOverride, true, false, false);
    } else {
      const url = configFiles[key];
      if (url) {
        await new Promise<void>((resolve, reject) => {
          Module.FS_createPreloadedFile('/', fileName, new URL(url, baseUrl ?? undefined).href, true, false, resolve, reject, false, false, undefined);
        });
      }
    }
    configFiles[key] = fileName;
  }

  static async create(config: InitConfig) {
    const clientConfig = config.clientConfig instanceof URL
        ? await (await fetch(config.clientConfig)).json() as ClientConfig
        : config.clientConfig;
    // TODO should this be here?
    // We let Websockify listen on 15xxx ports instead of 16xxx, so now we need to connect to those
    for (const server of ['AccessManager', 'StorageFacility', 'KeyServer', 'Transcryptor', 'RegistrationServer', 'Authserver'] as const) {
      clientConfig[server].Port -= 1000;
    }
    // Persist ClientKeys.json to keep user logged in
    clientConfig.KeysFile = '/persist/ClientKeys.json';

    let Module: MainModule | { thisProgram: string, print?: (...msg: any[]) => void; printErr?: (...msg: any[]) => void; } = {
      thisProgram: 'pepWeblib',
    };

    if (config.onOut) {
      const onOut = config.onOut;
      Module.print = (...msg) => onOut(msg.join(' '));
    }
    if (config.onError) {
      const onError = config.onError;
      Module.printErr = (...msg) => onError(msg.join(' '));
    }

    Module = await initModule(Module) as MainModule;

    const {FS, IDBFS} = Module;

    const baseUrl = config.clientConfig instanceof URL ? config.clientConfig : null;
    await Promise.all([
      this.#addConfigFile(Module, baseUrl, config.configFileContentOverrides, clientConfig, 'CACertificateFile', 'rootCA.cert'),
      this.#addConfigFile(Module, baseUrl, config.configFileContentOverrides, clientConfig, 'ShadowPublicKeyFile', 'ShadowAdministration.pub'),
      // Skip ProjectConfig.json for now as it's not used
    ]);

    FS.createDataFile('/', 'ClientConfig.json', JSON.stringify(clientConfig), true, false, false);

    FS.mkdir("/persist");
    // In browser
    if ('window' in globalThis) {
      // Persist this directory via IndexedDB
      FS.mount(IDBFS, {autoPersist: true}, '/persist');
    }

    // Populate folder from IndexedDB
    // Promises are currently not supported in preInit, see https://github.com/emscripten-core/emscripten/issues/14520
    await new Promise<void>((resolve, reject) => FS.syncfs(true, (err: Error | null) => {
      err ? reject(err) : resolve();
    }));
    // Call main now that ClientKeys.json is loaded
    runHandleWasmExceptionForModule(Module, () => Module.callMain());
    return new Pep(config, Module);
  }

  #wrapExec<Ret>(fun: () => Ret): Ret {
    ++this.#busy;
    this.#onBusyChange?.(true);
    let ret;
    try {
      ret = fun();
      if (ret instanceof Promise) {
        return (ret as Ret & Promise<Ret>)
            .finally(() => this.#onBusyChange?.(--this.#busy > 0)) as Ret & Promise<Ret>;
      }
      return ret;
    } finally {
      if (!(ret instanceof Promise)) {
        this.#onBusyChange?.(--this.#busy > 0);
      }
    }
  }

  delete() {
    const client = this.#client;
    this.#client = null!; // Prevent usage
    this.#wrapExec(() => {
      client.delete();
      try {
        this.#mod.exit();
      } catch (ex) {
        if (ex instanceof this.#mod.ExitStatus && ex.status === 0) {
          console.log(ex.message);
        } else {
          throw ex;
        }
      } finally {
        this.#mod = null!; // Prevent usage
      }
    });
  }

  onBusyChange(callback: ((busy: boolean) => void) | null) {
    this.#onBusyChange = callback;
  }

  onStatusChange(callback: (connected: boolean) => void) {
    this.#wrapExec(() => this.#client.onStatusChange(callback));
  }

  getEnrolledUser(): Promise<EnrolledUser | undefined> {
    return this.#wrapExec(() => this.#client.getEnrolledUser());
  }

  unenroll() {
    return this.#wrapExec(() => this.#client.unenroll());
  }

  async authenticate(): Promise<void> {
    const landingPage = this.#config.authLandingPage;
    if (!landingPage) {
      throw new Error("authLandingPage config was not set");
    }

    let oauthClient: rawTypes.OAuthClient | undefined;
    try {
      await this.#wrapExec(() => new Promise<void>((resolve, reject) => {
        const authChan = new BroadcastChannel('pep-auth-code-' + crypto.randomUUID());
        authChan.addEventListener('message', ev => {
          const msg = ev.data as AuthenticationChannelMessage;
          if ('code' in msg) {
            oauthClient!.completeAuthentication(msg.code)
                .then(resolve, reject);
          } else {
            oauthClient!.failAuthentication(msg.error, msg.errorDescription ?? '')
                .then(resolve, reject);
          }
        });

        const fullLandUrl = new URL(landingPage);
        fullLandUrl.searchParams.append('chan', authChan.name);
        oauthClient = this.#client.authenticate(fullLandUrl.href,
            (authUrl: string) => {
              open(authUrl, '_blank', 'popup,top=100,left=100,width=600,height=900');
            });
      }));
    } finally {
      oauthClient?.delete();
    }
  }

  authenticateWithToken(token: string) {
    return this.#wrapExec(() => this.#client.authenticateWithToken(token));
  }

  internalGenerateToken(tokenSecret: string, userGroup: string) {
    return this.#wrapExec(() => this.#client.internalGenerateToken(tokenSecret, userGroup));
  }

  listColumns(): Promise<ColumnGroup[]> {
    return this.#wrapExec(() => this.#client.listColumns());
  }

  listSubjectGroups(): Promise<SubjectGroup[]> {
    return this.#wrapExec(() => this.#client.listSubjectGroups());
  }

  registerParticipant(personalia: ParticipantPersonalia, isTestParticipant: boolean = false): Promise<string> {
    return this.#wrapExec(() => this.#client.registerParticipant({
      ...personalia,
      dateOfBirth: toDdMmYyyy(personalia.dateOfBirth)
    }, isTestParticipant));
  }

  list(query: ListQuery) : ReadableStream<CellEntry> {
    //@ts-ignore TODO
    return this.#wrapExec(() => this.#client.list(query));
  }

  retrieve(entries: CellEntry[]) : ReadableStream<CellData> {
    return this.#wrapExec(() => this.#client.retrieve(entries));
  }

  /** @see {@link handleWasmExceptionForModule} */
  handleWasmException(ex: WebAssembly.Exception) {
    return handleWasmExceptionForModule(this.#mod, ex);
  }

  /** @see {@link runHandleWasmExceptionForModule} */
  runHandleWasmException<Ret>(fun: () => Ret): Ret {
    return runHandleWasmExceptionForModule(this.#mod, fun);
  }
}
