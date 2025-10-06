import {describe} from 'mocha';
import {expect} from 'chai';
import Pep, {ClientConfig} from "./pep.mjs";
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import consumers from 'node:stream/consumers';

//TODO are these listeners needed?
process.on('uncaughtException', error => {
  process.exitCode = 1;
  console.error(error);
  // eslint-disable-next-line no-debugger
  debugger;
  console.warn('\nWe will try to continue anyway\n');
});

process.on('uncaughtExceptionMonitor', (error, origin) =>
    console.error('\n\n\x07❌️', origin));

let mainExited = false;
process.on('beforeExit', () => {
  if (!mainExited) {
    process.exitCode ??= 13;
    console.error('\n\n\x07❌️ Unexpected exit: Either Node.js was killed or ' +
        'it prevented a hang due to a Promise that can never be fulfilled');
  }
});

interface OAuthTokenSecretFile {
  OAuthTokenSecret: string;
}

describe('Pep', function () {
  let pep!: Pep;
  before(async function () {
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

    pep = await Pep.create({
      clientConfig,
      configFileContentOverrides: {
        CACertificateFile,
        ShadowPublicKeyFile,
      },
    });
    const token = pep.internalGenerateToken(tokenSecretFile.OAuthTokenSecret, 'Research Assessor');
    await pep.authenticateWithToken(token);
  });
  after(function () {
    if (pep) {
      pep.delete();
    }
  });

  describe('#listColumns()', function () {
    it('should list columns and column groups', async function () {
      expect(await pep.listColumns()).to.containSubset([
        {
          "columns": [
            "BUGS.Visit1.Assessor",
            "BUGS.Visit2.Assessor",
            "Visit1.Assessor",
            "Visit2.Assessor",
            "Visit3.Assessor",
          ],
          "name": "VisitAssessors",
        },
        {
          "columns": ["ParticipantInfo"],
          "name": "ParticipantInfo"
        },
        {
          "columns": ["ParticipantIdentifier"],
          "name": "ParticipantIdentifier",
        },
      ]);
    });
  });

  describe('#listSubjectGroups()', function () {
    it('should list subject groups', async function () {
      expect(await pep.listSubjectGroups()).to.containSubset([
        {name: '*'},
      ]);
    });
  });
});

mainExited = true;
