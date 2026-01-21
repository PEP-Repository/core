/**
 * @file This is run in integration.sh
 */

import {describe} from 'mocha';
import {expect} from 'chai';
import Pep, {CellEntry, ClientConfig} from "./pep.mjs";
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import consumers from 'node:stream/consumers';
import {binaryToString, concatStringsAsync} from "./utils.mjs";

let mainExited = false;
process.on('beforeExit', () => {
  if (!mainExited) {
    process.exitCode ??= 13;
    console.error('\nUnexpected exit: Either Node.js was terminated or ' +
        'it prevented a hang due to a Promise that can never be fulfilled');
  }
});

interface OAuthTokenSecretFile {
  OAuthTokenSecret: string;
}

describe('Pep', function () {
  let pep!: Pep;
  before(async function () {
    console.debug('Loading config');
    const [
      clientConfig,
      CACertificateFile,
      ShadowPublicKeyFile,
      tokenSecretFile
    ] = await Promise.all([
      consumers.json(fs.createReadStream(new URL('../test_config/ClientConfig.json', import.meta.url), 'utf8')) as Promise<ClientConfig>,
      fsp.readFile(new URL('../test_config/rootCA.cert', import.meta.url), 'utf8'),
      fsp.readFile(new URL('../test_config/ShadowAdministration.pub', import.meta.url), 'utf8'),
      consumers.json(fs.createReadStream(new URL('../test_config/OAuthTokenSecret.json', import.meta.url), 'utf8')) as Promise<OAuthTokenSecretFile>,
    ]);
    clientConfig.ProjectConfigFile = '';

    console.debug('Initializing Pep');
    pep = await Pep.create({
      clientConfig,
      configFileContentOverrides: {
        CACertificateFile,
        ShadowPublicKeyFile,
      },
    });
    console.debug('Pep initialized');

    await pep.runHandleWasmException(async () => {
      console.debug('Generating token');
      const token = pep.internalGenerateToken(tokenSecretFile.OAuthTokenSecret, 'Research Assessor');
      console.debug('Authenticating with token');
      await pep.authenticateWithToken(token);
      console.debug('Authenticated');
    });
  });
  after(function () {
    pep?.delete();
    mainExited = true;
  });

  describe('#listColumns()', function () {
    it('should list columns and column groups', async function () {
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

  describe('#listSubjectGroups()', function () {
    it('should list subject groups', async function () {
      expect(await pep.runHandleWasmException(() => pep.listSubjectGroups()))
          .to.containSubset([
            {name: '*'},
            {name: 'WasmTestSubjectGroup'},
          ]);
    });
  });

  describe('#list()', function () {
    it('should list cells given subject groups', function () {
      return pep.runHandleWasmException(async () => {
        const entries = await Array.fromAsync(pep.list({
          subjectGroups: ['WasmTestSubjectGroup'],
          columnGroups: ['WasmTestColumnGroup'],
        }));
        try {
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
            expect(entriesSimple).satisfies(function entriesShouldHaveRightColumn(entries: typeof entriesSimple) {
              return entries.every(entry => entry.column === 'WasmTestColumn');
            });
            expect(entriesSimple).satisfies(function someEntryShouldHaveSmallExtensionAndRightSize(entries: typeof entriesSimple) {
              return entries.some(entry =>
                  entry.partialMetadata['fileExtension'] === '.small'
                  && entry.fileSize === BigInt('Some small test data!'.length));
            });
            expect(entriesSimple).satisfies(function someEntryShouldHaveLargeExtensionAndRightSize(entries: typeof entriesSimple) {
              return entries.some(entry => entry.partialMetadata['fileExtension'] === '.large'
                      && entry.fileSize === BigInt('Larger test data!\n'.length) * 700_000n,
                  'One entry should have fileExtension .large and specific file size');
            });
          } catch (ex) {
            console.log(entriesSimple);
            throw ex;
          }
        } finally {
          entries.forEach(entry => entry.delete());
        }
      });
    });
    it('should list cells given subject origin ID', function () {
      return pep.runHandleWasmException(async () => {
        const entries = await Array.fromAsync(pep.list({
          subjects: ['WasmTestSubjectSmall'], // Pass origin ID
          columnGroups: ['WasmTestColumnGroup'],
        }));
        try {
          expect(entries).has.length(1);
          const [entry] = entries as [CellEntry];
          expect(binaryToString(entry.partialMetadataView().get('fileExtension') ?? new Uint8Array()))
              .equals('.small', 'Got back wrong fileExtension');
        } finally {
          entries.forEach(entry => entry.delete());
        }
      });
    });
  });

  describe('#retrieve()', function () {
    it('should retrieve cells', function () {
      return pep.runHandleWasmException(async () => {
        const entries = await Array.fromAsync(pep.list({
          subjectGroups: ['WasmTestSubjectGroup'],
          columnGroups: ['WasmTestColumnGroup'],
        }));
        try {
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
                expectedContent = Array(700_000).fill('Larger test data!\n').join('');
                break;
            }
            const content = await concatStringsAsync(data.content.pipeThrough(new TextDecoderStream()));
            expect(content).equals(expectedContent!, 'Wrong cell data');
          }
        } finally {
          entries.forEach(entry => entry.delete());
        }
      });
    });
  });
});
