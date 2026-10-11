/**
 * @file This is run in integration.sh, which prepares the structure and data
 */

import {expect} from 'chai';
import Pep, {CellEntry, ClientConfig, getColumnModes, InitConfig} from "./pep.mjs";
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
    it('should return the name, modes and columns for every column group', async () => {
      const columnGroups = await pep.runHandleWasmException(() => pep.listColumns());
      expect(columnGroups).to.containSubset([
        {
          name: 'WasmTestColumnGroup',
          columns: ['WasmTestColumn'],
        },
        {
          name: 'ParticipantInfo',
          modes: {read: true, readMeta: true, write: false, writeMeta: false},
          columns: ['ParticipantInfo'],
        },
        {
          name: 'ParticipantIdentifier',
          modes: {read: true, readMeta: true, write: false, writeMeta: false},
          columns: ['ParticipantIdentifier'],
        },
      ]);
      expect(columnGroups.find(group => group.name === 'WasmTestColumnGroup')?.modes)
          .to.deep.equal({read: true, readMeta: true, write: true, writeMeta: false});
    });

    it('should allow getting the modes for every column', async () => {
      const columns = getColumnModes(await pep.runHandleWasmException(() => pep.listColumns()));
      expect(columns.find(column => column.name === 'WasmTestColumn')?.modes)
          .to.deep.equal({read: true, readMeta: true, write: true, writeMeta: false});
    });
  });

  describe('#listSubjectGroups()', () => {
    it('should list the name and modes of each subject group', async () => {
      const subjectGroups = await pep.runHandleWasmException(() => pep.listSubjectGroups());
      expect(subjectGroups).to.containSubset([
        {name: '*', modes: {access: true, enumerate: false}},
        {name: 'WasmTestSubjectGroup'},
      ]);
      expect(subjectGroups.find(group => group.name === 'WasmTestSubjectGroup')?.modes)
          .to.deep.equal({access: true, enumerate: true});
    });
  });

  describe('#list()', () => {
    context('given a subject group and column group', () => {
      it('should list cells with metadata', async () => {
        let entries: CellEntry[] | undefined;

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
        }).finally(() => entries && deleteObjects(entries));
      });
    });

    context('given mode read-meta', () => {
      it('should list cells', async () => {
        let entries: CellEntry[] | undefined;

        return pep.runHandleWasmException(async () => {
          entries = await Array.fromAsync(pep.list({
            subjectGroups: ['WasmTestSubjectGroup'],
            columnGroups: ['WasmTestColumnGroup'],
            mode: 'read-meta',
          }));
          expect(entries.map(entry => binaryToString(entry.partialMetadataView().get('fileExtension') ?? new Uint8Array())))
              .to.include.members(['.small', '.large']);
        }).finally(() => entries && deleteObjects(entries));
      });
    });

    context('given a subject Origin ID and column group', () => {
      it('should list cells with metadata', async () => {
        let entries: CellEntry[] | undefined;

        return pep.runHandleWasmException(async () => {
          entries = await Array.fromAsync(pep.list({
            subjects: ['WasmTestSubjectSmall'], // Pass origin ID
            columnGroups: ['WasmTestColumnGroup'],
          }));
          expect(entries).has.length(1);
          const [entry] = entries as [CellEntry];
          expect(binaryToString(entry.partialMetadataView().get('fileExtension') ?? new Uint8Array()))
              .equals('.small', 'Got back wrong fileExtension');
        }).finally(() => entries && deleteObjects(entries));
      });
    });
  });

  describe('#retrieve()', () => {
    context('given a subject group and column group', () => {
      it('should retrieve cells with data', () => {
        let entries: CellEntry[] | undefined;

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
        }).finally(() => entries && deleteObjects(entries));
      });
    });
  });

  describe('user management', () => {
    const suffix = crypto.randomUUID();
    const uid = `weblib-test-user-${suffix}`;
    const userGroup = `WeblibTestUserGroup-${suffix}`;
    // Whole seconds, as that is what the server stores
    const expiration = new Date(Math.floor(Date.now() / 1000 + 24 * 60 * 60) * 1000);

    before(() => authenticateAs('Access Administrator'));
    after(() => authenticateAs('Research Assessor'));

    it('should create, modify and remove users and user groups', () => pep.runHandleWasmException(async () => {
      await pep.createUserGroup({name: userGroup, maxAuthValiditySeconds: 3600});
      await pep.createUser(uid);
      await pep.addUserToGroup(uid, userGroup, expiration);

      let result = await pep.listUsers({userFilter: uid});
      expect(result.users).to.deep.equal([{
        displayId: uid,
        primaryId: undefined,
        otherUids: [],
        groups: [{userGroup, expiration}],
      }]);
      expect(result.userGroups).to.deep.equal([{name: userGroup, maxAuthValiditySeconds: 3600}]);

      await pep.updateUserGroupExpiration(uid, userGroup, undefined);
      result = await pep.listUsers({groupFilter: userGroup});
      expect(result.users[0]?.groups).to.deep.equal([{userGroup, expiration: undefined}]);

      await pep.removeUserFromGroup(uid, userGroup);
      await pep.removeUser(uid);
      await pep.removeUserGroup(userGroup);

      result = await pep.listUsers({userFilter: uid});
      expect(result.users).to.be.empty;
    }));
  });

  describe('data administration', () => {
    const suffix = crypto.randomUUID();
    const column = `WeblibTestColumn-${suffix}`;
    const columnGroup = `WeblibTestColumnGroup-${suffix}`;
    const subjectGroup = `WeblibTestSubjectGroup-${suffix}`;

    before(() => authenticateAs('Data Administrator'));
    after(() => authenticateAs('Research Assessor'));

    it('should create and remove columns and column groups', () => pep.runHandleWasmException(async () => {
      await pep.createColumn(column);
      await pep.createColumnGroup(columnGroup);
      await pep.addColumnToGroup(column, columnGroup);

      let state = await pep.queryAccessManager({columnGroup});
      expect(state.columnGroups).to.deep.equal([{name: columnGroup, columns: [column]}]);

      await pep.removeColumnFromGroup(column, columnGroup);
      state = await pep.queryAccessManager({columnGroup});
      expect(state.columnGroups).to.deep.equal([{name: columnGroup, columns: []}]);

      // force removes the column membership as well
      await pep.addColumnToGroup(column, columnGroup);
      await pep.removeColumnGroup(columnGroup, true);
      await pep.removeColumn(column);

      state = await pep.queryAccessManager();
      expect(state.columns).to.not.include(column);
      expect(state.columnGroups.map(group => group.name)).to.not.include(columnGroup);
    }));

    it('should create and remove subject groups, and add and remove subjects', () => pep.runHandleWasmException(async () => {
      await pep.createSubjectGroup(subjectGroup);
      expect((await pep.queryAccessManager({subjectGroup})).subjectGroups).to.deep.equal([subjectGroup]);

      await pep.addSubjectToGroup(subjectGroup, 'WasmTestSubjectSmall');
      await pep.removeSubjectFromGroup(subjectGroup, 'WasmTestSubjectSmall');
      // Succeeds without force, as the group is empty again
      await pep.removeSubjectGroup(subjectGroup);

      expect((await pep.queryAccessManager()).subjectGroups).to.not.include(subjectGroup);
    }));
  });

  describe('access rules', () => {
    // Use a new user group, so rules of the existing groups are left alone
    const userGroup = `WeblibTestAccessRuleGroup-${crypto.randomUUID()}`;

    before(async () => {
      await authenticateAs('Access Administrator');
      await pep.runHandleWasmException(() => pep.createUserGroup({name: userGroup}));
    });
    after(async () => {
      await pep.runHandleWasmException(() => pep.removeUserGroup(userGroup));
      await authenticateAs('Research Assessor');
    });

    it('should create and remove column group access rules', () => pep.runHandleWasmException(async () => {
      await pep.createColumnGroupAccessRule('WasmTestColumnGroup', userGroup, 'read');
      expect((await pep.queryAccessManager({userGroup})).columnGroupAccessRules)
          .to.deep.equal([{columnGroup: 'WasmTestColumnGroup', userGroup, mode: 'read'}]);

      await pep.removeColumnGroupAccessRule('WasmTestColumnGroup', userGroup, 'read');
      expect((await pep.queryAccessManager({userGroup})).columnGroupAccessRules).to.be.empty;
    }));

    it('should create and remove subject group access rules', () => pep.runHandleWasmException(async () => {
      await pep.createSubjectGroupAccessRule('WasmTestSubjectGroup', userGroup, 'enumerate');
      expect((await pep.queryAccessManager({userGroup})).subjectGroupAccessRules)
          .to.deep.equal([{subjectGroup: 'WasmTestSubjectGroup', userGroup, mode: 'enumerate'}]);

      await pep.removeSubjectGroupAccessRule('WasmTestSubjectGroup', userGroup, 'enumerate');
      expect((await pep.queryAccessManager({userGroup})).subjectGroupAccessRules).to.be.empty;
    }));
  });
});
