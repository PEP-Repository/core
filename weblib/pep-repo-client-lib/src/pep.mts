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

/** Configuration to initialize Pep */
export interface InitConfig {
  clientConfig: URL | ClientConfig;
  configFileContentOverrides?: { [key in keyof ConfigFiles]?: string | undefined };
  authLandingPage?: URL | null;
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
  /** Time part is ignored */
  dateOfBirth: Date;
}

export interface ListQuery {
  subjectGroups?: string[] | undefined;
  /** Loose subjects to request (any format taht would be recognized by pepcli) */
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
 * @warning This object must be deleted after use
 */
export interface CellData extends rawTypes.CellData {
  /** Reference to the original CellEntry */
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

type WasmException =
    | WebAssembly.Exception // WASM EH (what we normally use)
    | number /*pointer*/ // Emscripten EH
    | Error /*CppException*/ // Emscripten EH + assertions

function mayBeWasmException(ex: WasmException | unknown): ex is WasmException {
  return ex instanceof WebAssembly.Exception
      || (typeof ex === 'number' && ex > 0)
      || (ex instanceof Error && Object.getPrototypeOf(ex).constructor.name === 'CppException');
}

/**
 * Inspects the exception via `callback` and then frees it.
 */
function consumeWasmException<TReturn, TException extends WasmException>(
    mod: MainModule, wasmEx: TException,
    callback: (wasmEx: TException) => TReturn): TReturn {
  try {
    return callback(wasmEx);
  } finally {
    mod.decrementExceptionRefcount(wasmEx);
  }
}

/**
 * Transforms a WASM exception into an Error with the correct message, and stack when available.
 * After obtaining message, frees the WASM exception object.
 */
function handleWasmExceptionForModule(mod: MainModule, wasmEx: WasmException): Error {
  return consumeWasmException(mod, wasmEx, () => {
    const [type, message] = mod.getExceptionMessage(wasmEx) as [string, string];
    const error = new Error(message || type, {cause: wasmEx});
    const stack = typeof wasmEx === 'object' && 'stack' in wasmEx
        ? wasmEx.stack as string : undefined;
    if (stack) {
      error.stack = stack;
    }
    console.warn(`WebAssembly Exception: ${type}: ${message}${stack ? `\n${stack}` : ''}`);
    return error;
  });
}

function throwPotentialWasmException(mod: MainModule, ex: WasmException | Error | unknown): never {
  if (mayBeWasmException(ex)) {
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
      clientConfig[server].Port -= 1_000;
    }
    // Persist ClientKeys.json to keep user logged in
    clientConfig.KeysFile = '/persist/ClientKeys.json';

    // These members must be listed in -sINCOMING_MODULE_JS_API
    let Module: MainModule | { thisProgram: string } = {
      thisProgram: 'pepWeblib',
    };

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

  // Should we remove this? It only covers functions directly on Pep
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

  /** Stop event loop and delete instance */
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

  // Symbol.dispose may be null when unsupported
  [Symbol.dispose]() { this.delete(); }

  /** Register handler to observe when methods are executing */
  onBusyChange(callback: ((busy: boolean) => void) | null) {
    this.#onBusyChange = callback;
  }

  /** Register handler to observe when connection drops or restores */
  onStatusChange(callback: (connected: boolean) => void) {
    this.#wrapExec(() => this.#client.onStatusChange(callback));
  }

  /** Get currently enrolled (logged-in) user. May persist between sessions. */
  getEnrolledUser(): Promise<EnrolledUser | undefined> {
    return this.#wrapExec(() => this.#client.getEnrolledUser());
  }

  /** Un-enroll (log out) current user */
  unenroll() {
    return this.#wrapExec(() => this.#client.unenroll());
  }

  /** Start OAuth authentication to enroll user */
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

  /** Enroll user with OAuth token */
  authenticateWithToken(token: string) {
    return this.#wrapExec(() => this.#client.authenticateWithToken(token));
  }

  /**
   * For development: Generate OAuth token from secret
   * @internal
   */
  internalGenerateToken(tokenSecret: string, userGroup: string) {
    return this.#wrapExec(() => this.#client.internalGenerateToken(tokenSecret, userGroup));
  }

  listColumns(): Promise<ColumnGroup[]> {
    return this.#wrapExec(() => this.#client.listColumns());
  }

  listSubjectGroups(): Promise<SubjectGroup[]> {
    return this.#wrapExec(() => this.#client.listSubjectGroups());
  }

  /** Register a subject */
  registerParticipant(personalia: ParticipantPersonalia, isTestParticipant: boolean = false): Promise<string> {
    return this.#wrapExec(() => this.#client.registerParticipant({
      ...personalia,
      dateOfBirth: toDdMmYyyy(personalia.dateOfBirth)
    }, isTestParticipant));
  }

  /**
   * List cell entries matching query.
   * @warning Objects will be leaked when `cancel()` is called on the stream,
   *  including when prematurely exiting a `for await` loop.
   *  There is currently no elegant solution to this, except iterating over
   *  `stream.values({preventCancel: true})` and iterating again to delete remaining objects.
   */
  list(query: ListQuery) : ReadableStream<CellEntry> {
    //XXX Cast to Required<ListQuery> because of https://github.com/emscripten-core/emscripten/issues/25978,
    // remove it with EMSDK 5.0.0+
    return this.#wrapExec(() => this.#client.list(query as Required<ListQuery>));
  }

  /**
   * Retrieve cell contents. Entries must be obtained from the same `list()` call.
   * @warning Objects will be leaked when `cancel()` is called on the stream,
   *  including when prematurely exiting a `for await` loop.
   *  There is currently no elegant solution to this, except iterating over
   *  `stream.values({preventCancel: true})` and iterating again to delete remaining objects.
   */
  retrieve(entries: CellEntry[]) : ReadableStream<CellData> {
    return this.#wrapExec(() => this.#client.retrieve(entries));
  }

  /** @see {@link handleWasmExceptionForModule} */
  handleWasmException(ex: WasmException) {
    return handleWasmExceptionForModule(this.#mod, ex);
  }

  /** @see {@link runHandleWasmExceptionForModule} */
  runHandleWasmException<Ret>(fun: () => Ret): Ret {
    return runHandleWasmExceptionForModule(this.#mod, fun);
  }

  static mayBeWasmException(ex: WasmException | unknown) {
    return mayBeWasmException(ex);
  }
}
