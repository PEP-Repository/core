/**
 * @file This is run in integration.sh, which prepares the structure and data
 */

import {expect} from 'chai';
import Pep, {CellEntry, ClientConfig, getColumnModes, InitConfig, ListColumnsQuery} from "./pep.mjs";
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
  let tokenSecretObj!: OAuthTokenSecretFile;

  async function authenticateAs(userGroup: string) {
    await pep.runHandleWasmException(async () => {
      console.debug(`Authenticating as ${userGroup}`);
      await pep.authenticateWithToken(pep.internalGenerateToken(tokenSecretObj.OAuthTokenSecret, userGroup));
      console.debug('Authenticated');
    });
  }

  before(async () => {
    console.debug('Loading config');
    let pepCreateArgs: InitConfig;
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

    await authenticateAs('Research Assessor');
  });
  after(() => {
    pep?.delete();
    mainExited = true;
  });

  describe('#listColumns()', () => {
    /** @returns The modes per test column group returned by listColumns */
    async function listTestColumnGroupModes(query?: ListColumnsQuery) {
      const columnGroups = await pep.runHandleWasmException(() => pep.listColumns(query));
      return Object.fromEntries(columnGroups
          .filter(group => ['WasmTestColumnGroup', 'WasmTestMetaColumnGroup'].includes(group.name))
          .map(group => [group.name, group.modes]));
    }

    it('should return the name, modes and columns for every column group', async () => {
      const columnGroups = await pep.runHandleWasmException(() => pep.listColumns());
      expect(columnGroups).to.containSubset([
        {
          name: 'WasmTestColumnGroup',
          columns: ['WasmTestColumn'],
        },
        {
          name: 'ParticipantInfo',
          modes: {read: true, readMeta: true},
          columns: ['ParticipantInfo'],
        },
        {
          name: 'ParticipantIdentifier',
          modes: {read: true, readMeta: true},
          columns: ['ParticipantIdentifier'],
        },
      ]);
    });

    it('should return all accessible column groups with implied modes by default', async () => {
      expect(await listTestColumnGroupModes()).to.deep.equal({
        WasmTestColumnGroup: {read: true, readMeta: true, write: true, writeMeta: false},
        WasmTestMetaColumnGroup: {read: false, readMeta: true, write: true, writeMeta: true},
      });
    });

    it('should leave out implied modes when includeImplicitlyGranted is false', async () => {
      expect(await listTestColumnGroupModes({includeImplicitlyGranted: false})).to.deep.equal({
        WasmTestColumnGroup: {read: true, readMeta: false, write: true, writeMeta: false},
        WasmTestMetaColumnGroup: {read: false, readMeta: true, write: false, writeMeta: true},
      });
    });

    it('should only return column groups with all required modes', async () => {
      expect(Object.keys(await listTestColumnGroupModes({requireModes: {write: true, writeMeta: true}})))
          .to.deep.equal(['WasmTestMetaColumnGroup']);
    });
  });

  describe('getColumnModes()', () => {
    it('should combine the modes of all column groups containing a column', () => {
      const none = {read: false, readMeta: false, write: false, writeMeta: false};
      expect(getColumnModes([
        {name: 'GroupA', modes: {...none, readMeta: true}, columns: ['y', 'x']},
        {name: 'GroupB', modes: {...none, write: true}, columns: ['x']},
      ])).to.deep.equal([
        {name: 'x', modes: {...none, readMeta: true, write: true}},
        {name: 'y', modes: {...none, readMeta: true}},
      ]);
    });
  });

  describe('#listSubjectGroups()', () => {
    it('should list the name and modes of each subject group', async () => {
      const subjectGroups = await pep.runHandleWasmException(() => pep.listSubjectGroups());
      expect(subjectGroups).to.containSubset([
        {name: '*', modes: {access: true}},
        {name: 'WasmTestSubjectGroup', modes: {access: true, enumerate: true}},
      ]);
      expect(subjectGroups.find(group => group.name === 'WasmTestEnumerateSubjectGroup')?.modes)
          .to.deep.equal({access: false, enumerate: true});
    });
  });

  describe('#list()', () => {
    context('given a subject group and column group', () => {
      let entries: CellEntry[] | undefined;
      after(() => entries && deleteObjects(entries));

      it('should list cells with metadata', async () => {
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

    context('given mode read-meta and a column group without read access', () => {
      let entries: CellEntry[] | undefined;
      after(() => entries && deleteObjects(entries));

      it('should list cells', async () => {
        return pep.runHandleWasmException(async () => {
          entries = await Array.fromAsync(pep.list({
            subjectGroups: ['WasmTestSubjectGroup'],
            columnGroups: ['WasmTestMetaColumnGroup'],
            mode: 'read-meta',
          }));
          expect(entries.map(entry => binaryToString(entry.partialMetadataView().get('fileExtension') ?? new Uint8Array())))
              .to.include('.meta');
        });
      });
    });

    context('given a subject Origin ID and column group', () => {
      let entries: CellEntry[] | undefined;
      after(() => entries && deleteObjects(entries));

      it('should list cells with metadata', async () => {
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
      let entries: CellEntry[] | undefined;
      after(() => entries && deleteObjects(entries));

      it('should retrieve cells with data', () => {
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

  describe('user management', () => {
    const suffix = crypto.randomUUID();
    const uid = `weblib-test-user-${suffix}`;
    const otherUid = `weblib-test-user-other-${suffix}`;
    const userGroup = `WeblibTestUserGroup-${suffix}`;
    const expiration = new Date(Date.now() + 24 * 60 * 60 * 1000);

    /** @returns The identifiers of the test user */
    async function listTestUserIds() {
      const {users} = await pep.listUsers({userFilter: suffix});
      return users.map(({displayId, primaryId, otherUids}) => ({displayId, primaryId, otherUids}));
    }

    before(() => authenticateAs('Access Administrator'));
    after(async () => {
      // Remove what is left if the test failed halfway. Steps that are not applicable fail, which is ignored
      for (const step of [
        () => pep.removeUserFromGroup(uid, userGroup),
        () => pep.removeUserFromGroup(otherUid, userGroup),
        () => pep.removeUser(uid),
        () => pep.removeUser(otherUid),
        () => pep.removeUserGroup(userGroup),
      ]) {
        await pep.runHandleWasmException(step).catch(() => {});
      }
      await authenticateAs('Research Assessor');
    });

    it('should create, modify and remove users and user groups', () => pep.runHandleWasmException(async () => {
      await pep.createUserGroup({name: userGroup, maxAuthValiditySeconds: 3600});
      await pep.createUser(uid);
      await pep.addUserToGroup(uid, userGroup, expiration);
      expect(await pep.listUsers({userFilter: suffix})).to.deep.equal({
        users: [{displayId: uid, primaryId: undefined, otherUids: [], groups: [{userGroup, expiration}]}],
        userGroups: [{name: userGroup, maxAuthValiditySeconds: 3600}],
      });

      await pep.modifyUserGroup({name: userGroup});
      await pep.updateUserGroupExpiration(uid, userGroup, undefined);
      expect(await pep.listUsers({userFilter: suffix})).to.deep.equal({
        users: [{displayId: uid, primaryId: undefined, otherUids: [], groups: [{userGroup, expiration: undefined}]}],
        userGroups: [{name: userGroup, maxAuthValiditySeconds: undefined}],
      });

      await pep.addUserIdentifier(uid, otherUid, {isPrimaryId: true});
      expect(await listTestUserIds()).to.deep.equal([{displayId: uid, primaryId: otherUid, otherUids: []}]);

      await pep.unsetUserPrimaryId(otherUid);
      await pep.setUserDisplayId(otherUid);
      expect(await listTestUserIds()).to.deep.equal([{displayId: otherUid, primaryId: undefined, otherUids: [uid]}]);

      await pep.setUserPrimaryId(uid);
      expect(await listTestUserIds()).to.deep.equal([{displayId: otherUid, primaryId: uid, otherUids: []}]);

      await pep.removeUserIdentifier(uid);
      expect(await listTestUserIds()).to.deep.equal([{displayId: otherUid, primaryId: undefined, otherUids: []}]);

      await pep.removeUserFromGroup(otherUid, userGroup);
      await pep.removeUser(otherUid);
      await pep.removeUserGroup(userGroup);
      expect(await listTestUserIds()).to.be.empty;
      expect((await pep.listUsers({groupFilter: userGroup})).userGroups).to.be.empty;
    }));
  });
});
