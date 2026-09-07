/**
 * @file This is run in integration.sh, which prepares the structure and data
 */

import {expect} from 'chai';
import Pep, {CellEntry, ClientConfig, InitConfig} from "./pep.mjs";
import {binaryToString, concatStringsAsync, deleteObjects} from "./utils.mjs";

const isNode = !!globalThis.process;

let mainExited = false;
if (isNode) {
  process.on('beforeExit', () => {
    if (!mainExited) {
      process.exitCode ??= 13;
      console.error('\nUnexpected exit: Either Node.js was terminated or ' +
          'it prevented a hang due to a Promise that can never be fulfilled');
    }
  });
}

interface OAuthTokenSecretFile {
  OAuthTokenSecret: string;
}

describe('Pep', () => {
  let pep!: Pep;
  before(async () => {
    console.debug('Loading config');
    let pepCreateArgs: InitConfig;
    let tokenSecretObj: OAuthTokenSecretFile;
    if (isNode) {
      // Node.js cannot `fetch` `file:` URLs (yet)
      const [fs, fsp, consumers] = await Promise.all([
          import('node:fs'),
          import('node:fs/promises'),
          import('node:stream/consumers'),
      ]);
      const [
        clientConfig,
        CaCertificateFile,
        ShadowPublicKeyFile,
        tokenSecretObj0
      ] = await Promise.all([
        consumers.json(fs.createReadStream(new URL('../dist-test/ClientConfig.json', import.meta.url), 'utf8')) as Promise<ClientConfig>,
        fsp.readFile(new URL('../dist-test/rootCA.cert', import.meta.url), 'utf8'),
        fsp.readFile(new URL('../dist-test/ShadowAdministration.pub', import.meta.url), 'utf8'),
        consumers.json(fs.createReadStream(new URL('../dist-test/OAuthTokenSecret.json', import.meta.url), 'utf8')) as Promise<OAuthTokenSecretFile>,
      ]);
      tokenSecretObj = tokenSecretObj0;
      clientConfig.ProjectConfigFile = '';

      pepCreateArgs = {
        clientConfig,
        configFileContentOverrides: {
          CaCertificateFile,
          ShadowPublicKeyFile,
        },
      };
    } else {
      tokenSecretObj = await fetch(new URL('../dist-test/OAuthTokenSecret.json', import.meta.url))
          .then(r => r.json()) as OAuthTokenSecretFile;
      pepCreateArgs = {
        clientConfig: new URL('../dist-test/ClientConfig.json', import.meta.url),
      };
    }

    console.debug('Initializing Pep');
    pep = await Pep.create(pepCreateArgs);
    console.debug('Pep initialized');

    await pep.runHandleWasmException(async () => {
      console.debug('Generating token');
      const token = pep.internalGenerateToken(tokenSecretObj.OAuthTokenSecret, 'Research Assessor');
      console.debug('Authenticating with token');
      await pep.authenticateWithToken(token);
      console.debug('Authenticated');
    });
  });
  after(() => {
    pep?.delete();
    mainExited = true;
  });

  describe('#listColumns()', () => {
    it('should return the name and columns for every column group', async () => {
      expect(await pep.runHandleWasmException(() => pep.listColumns()))
          .to.containSubset([
        {
          name: 'WasmTestColumnGroup',
          columns: ['WasmTestColumn'],
        },
        {
          name: 'ParticipantInfo',
          columns: ['ParticipantInfo'],
        },
        {
          name: 'ParticipantIdentifier',
          columns: ['ParticipantIdentifier'],
        },
      ]);
    });
  });

  describe('#listSubjectGroups()', () => {
    it('should list the name of each subject group', async () => {
      expect(await pep.runHandleWasmException(() => pep.listSubjectGroups()))
          .to.containSubset([
            {name: '*'},
            {name: 'WasmTestSubjectGroup'},
          ]);
    });
  });

  describe('#list()', () => {
    context('given a subject group and column group', () => {
      it('should list cells with metadata', async () => {
        let entries: CellEntry[] | undefined;
        after(() => entries && deleteObjects(entries));

        return pep.runHandleWasmException(async () => {
          entries = await Array.fromAsync(pep.list({
            subjectGroups: ['WasmTestSubjectGroup'],
            columnGroups: ['WasmTestColumnGroup'],
          }));
          const entriesSimple = entries.map(entry => ({
            id: entry.id,
            subjectLocalPseudonym: entry.subjectLocalPseudonym,
            column: entry.column,
            fileSize: entry.fileSize,
            partialMetadata: Object.fromEntries(
                [...entry.partialMetadataView().entries()]
                    .map(([key, value]) =>
                        [key, value !== undefined ? binaryToString(value) : null])
            ),
          }));
          try {
            expect(entriesSimple).satisfies(
                function entries_should_have_correct_column_name(entries: typeof entriesSimple) {
              return entries.every(entry => entry.column === 'WasmTestColumn');
            });
            expect(entriesSimple).satisfies(
                function includes_entry_with_the_data_for_the_small_subject(entries: typeof entriesSimple) {
              return entries.some(entry =>
                  entry.partialMetadata['fileExtension'] === '.small'
                  && entry.fileSize === 'Some small test data!'.length);
            });
            expect(entriesSimple).satisfies(
                function includes_entry_with_the_data_for_the_large_subject(entries: typeof entriesSimple) {
              return entries.some(entry =>
                  entry.partialMetadata['fileExtension'] === '.large'
                  && entry.fileSize === 'Larger test data!\n'.length * 120_000);
            });
          } catch (ex) {
            console.log(entriesSimple); // For debugging
            throw ex;
          }
        });
      });
    });

    context('given a subject Origin ID and column group', () => {
      it('should list cells with metadata', async () => {
        let entries: CellEntry[] | undefined;
        after(() => entries && deleteObjects(entries));

        return pep.runHandleWasmException(async () => {
          entries = await Array.fromAsync(pep.list({
            subjects: ['WasmTestSubjectSmall'], // Pass origin ID
            columnGroups: ['WasmTestColumnGroup'],
          }));
          expect(entries).has.length(1);
          const [entry] = entries as [CellEntry];
          expect(binaryToString(entry.partialMetadataView().get('fileExtension') ?? new Uint8Array()))
              .equals('.small', 'Got back wrong fileExtension');
        });
      });
    });
  });

  describe('#retrieve()', () => {
    context('given a subject group and column group', () => {
      it('should retrieve cells with data', () => {
        let entries: CellEntry[] | undefined;
        after(() => entries && deleteObjects(entries));

        return pep.runHandleWasmException(async () => {
          entries = await Array.fromAsync(pep.list({
            subjectGroups: ['WasmTestSubjectGroup'],
            columnGroups: ['WasmTestColumnGroup'],
          }));
          const smallEntry = entries.find(entry => binaryToString(entry.partialMetadataView().get('fileExtension') ?? new Uint8Array()) === '.small'),
              largeEntry = entries.find(entry => binaryToString(entry.partialMetadataView().get('fileExtension') ?? new Uint8Array()) === '.large');
          expect(smallEntry).to.exist;
          expect(largeEntry).to.exist;

          for await (using data of pep.retrieve([smallEntry!, largeEntry!])) {
            let expectedContent: string;
            switch (data.entry.id) {
              case smallEntry!.id:
                expectedContent = 'Some small test data!';
                break;
              case largeEntry!.id:
                expectedContent = Array(120_000).fill('Larger test data!\n').join('');
                break;
            }
            const content = await concatStringsAsync(data.content.pipeThrough(new TextDecoderStream()));
            expect(content).equals(expectedContent!, 'Wrong cell data');
          }
        });
      });
    });
  });
});
