/**
 * @file Sample client page that consumes PEP Weblib
 */

import Pep from 'pep-repo-client';
import {binaryToString, byteStreamToString, deleteObjects, deleteObjectsAsync} from 'pep-repo-client/utils';

/** @type {Pep|undefined} */
let pep;

/**
 * @param {*} ex
 * @param {Event} ev
 */
function handleMaybeWasmException(ex, ev) {
  // Note: Could normally check `instanceof WebAssembly.Exception` instead
  if (pep && Pep.mayBeWasmException(ex)) {
    const error = pep.handleWasmException(ex);
    alert(error);
    console.error('Uncaught', error);
    ev.preventDefault();
  }
}

addEventListener('error', ev => {
  // Ignore noise from background threads, see /cpp/weblib/prejs.js
  // See also https://github.com/emscripten-core/emscripten/issues/18016
  if (ev.error instanceof ErrorEvent || (!ev.error && ev.message.endsWith('[object WebAssembly.Exception]'))) {
    return;
  }
  handleMaybeWasmException(ev.error, ev);
  if (!ev.defaultPrevented) {
    alert(ev.error || ev.message);
  }
});
addEventListener('unhandledrejection', ev => {
  handleMaybeWasmException(ev.reason, ev);
  if (!ev.defaultPrevented) {
    alert(ev.reason);
  }
});

const loading = document.getElementById('loading');
const working = document.getElementById('working');
const loginBtn = /** @type {HTMLButtonElement} */ (document.getElementById('login-btn')),
    logoutBtn = /** @type {HTMLButtonElement} */ (document.getElementById('logout-btn'));
const loggedIn = document.getElementById('logged-in');
const userInfo = document.getElementById('user-info');
const
    subjectGroupsBtn = /** @type {HTMLButtonElement} */ (document.getElementById('subject-groups-btn')),
    columnsBtn = /** @type {HTMLButtonElement} */ (document.getElementById('columns-btn')),
    listBtn = /** @type {HTMLButtonElement} */ (document.getElementById('list-btn')),
    retrieveBtn = /** @type {HTMLButtonElement} */ (document.getElementById('retrieve-btn')),
    saveBtn = /** @type {HTMLButtonElement} */ (document.getElementById('save-btn'));
const output = /** @type {HTMLTextAreaElement} */ (document.getElementById('output'));

const
    participantFirstName = /** @type {HTMLInputElement} */ (document.getElementById('participant-first-name')),
    participantMiddleName = /** @type {HTMLInputElement} */ (document.getElementById('participant-middle-name')),
    participantLastName = /** @type {HTMLInputElement} */ (document.getElementById('participant-last-name')),
    participantDateOfBirth = /** @type {HTMLInputElement} */ (document.getElementById('participant-date-of-birth'));
const registerParticipantBtn = /** @type {HTMLButtonElement} */ (document.getElementById('register-participant-btn'));

output.value = '';

pep = await Pep.create({
  clientConfig: new URL("dist/ClientConfig.json", location.href),
  authLandingPage: new URL("pepWasmTest-auth-landing", location.href),
});

pep.onBusyChange(isBusy => working.hidden = !isBusy);
pep.onStatusChange(connected => loading.hidden = connected);
loginBtn.disabled = false;

subjectGroupsBtn.addEventListener('click', () => void (async () => {
  output.value = JSON.stringify(await pep.listSubjectGroups(), null, '  ');
})());

columnsBtn.addEventListener('click', () => void (async () => {
  output.value = JSON.stringify(await pep.listColumns(), null, '  ');
})());

/** @type {import("pep-repo-client").CellEntry[] | undefined} */
let entries;
listBtn.addEventListener('click', () => void (async () => {
  const res = pep.list({
    columnGroups: (await pep.listColumns()).map(cg => cg.name),
    subjectGroups: (await pep.listSubjectGroups()).map(sg => sg.name),
  });
  if (entries) {
    deleteObjects(entries);
  }
  entries = await Array.fromAsync(res);
  retrieveBtn.disabled = false;
  saveBtn.disabled = false;

  const jsonEntries = entries.map(entry => ({
    id: entry.id,
    subjectLocalPseudonym: entry.subjectLocalPseudonym,
    column: entry.column,
    fileSize: entry.fileSize,
    // Assume all metadata is text
    partialMetadata: Object.fromEntries(
        [...entry.partialMetadataView().entries()]
            .map(([key, value]) =>
                [key, value !== undefined ? binaryToString(value) : null])
    ),
  }));
  output.value = JSON.stringify(jsonEntries, null, '  ');
})());

retrieveBtn.addEventListener('click', () => void (async () => {
  const res = pep.retrieve(entries.filter(e => e.fileSize < 1024));
  const jsonEntries = [];
  try {
    for await (const data of res.values({preventCancel: true})) {
      try {
        jsonEntries.push({
          id: data.entry.id,
          subjectLocalPseudonym: data.entry.subjectLocalPseudonym,
          column: data.entry.column,
          content: await byteStreamToString(data.content),
        });

      } finally {
        // Note: In modern browsers one can use `using`:
        // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/using
        data.delete();
      }
    }
  } finally {
    await deleteObjectsAsync(res);
  }
  output.value = JSON.stringify(jsonEntries, null, '  ');
})());

saveBtn.addEventListener('click', () => void (async () => {
  let maxSize = 0;
  /** @type {import("pep-repo-client").CellEntry | undefined} */
  let entry;
  for (const aEntry of entries) {
    if (aEntry.fileSize > maxSize) {
      maxSize = aEntry.fileSize;
      entry = aEntry;
    }
  }
  if (entry.fileSize < 10e6) {
    throw new Error('No cells over 10MB, upload one via pepcli');
  }
  output.value = JSON.stringify({
    id: entry.id,
    subjectLocalPseudonym: entry.subjectLocalPseudonym,
    column: entry.column,
    fileSize: entry.fileSize,
  }, null, '  ');

  const fileExtensionBin = entry.partialMetadataView().get('fileExtension');
  const fileName = 'file' + (fileExtensionBin ? binaryToString(fileExtensionBin) : '');

  /** @type {import("pep-repo-client").CellData | undefined} */
  let data;
  async function getContent() {
    [data] = await Array.fromAsync(pep.retrieve([entry]));
    const startTime = performance.now();
    let downloaded = 0;
    let prevTime = startTime;
    return data.content.pipeThrough(new TransformStream({
      transform(chunk, controller) {
        downloaded += chunk.byteLength;
        const now = performance.now();
        if (now > prevTime + 500 || downloaded === entry.fileSize) {
          prevTime = now;
          output.value = `${downloaded / 1e6} / ${entry.fileSize / 1e6} MB downloaded (${Math.floor(downloaded / entry.fileSize * 100)}% complete)`;
          if (downloaded === entry.fileSize) {
            const endTime = performance.now();
            output.value += `\nDownloaded ${downloaded} B in ${endTime - startTime} ms`;
          }
        }
        controller.enqueue(chunk);
      }
    }));
  }

  try {
    if ('showSaveFilePicker' in window) {
      const file = await showSaveFilePicker({suggestedName: fileName});
      await (await getContent()).pipeTo(await file.createWritable());

    } else {
      console.warn('Browser does not support streaming file download, will write to Origin-Private File System first');
      const rootDir = await navigator.storage.getDirectory();
      const tmpDir = await rootDir.getDirectoryHandle('tmp-retrieve', {create: true});
      const tmpFile = await tmpDir.getFileHandle(`file-${crypto.randomUUID()}`, {create: true});
      await (await getContent()).pipeTo(await tmpFile.createWritable());
      const blob = await tmpFile.getFile();
      const blobUrl = URL.createObjectURL(blob);
      const a = document.createElement('a')
      a.href = blobUrl;
      a.download = fileName;
      a.click();
      // Unclear if a delay is required, but let's be sure the download started,
      //  see https://stackoverflow.com/a/71164969
      setTimeout(() => {
        URL.revokeObjectURL(blobUrl);
        void tmpDir.removeEntry(tmpFile.name);
      }, 1_000);
    }
  } finally {
    data?.delete();
  }
})());

registerParticipantBtn.addEventListener('click', () => void (async () => {
  output.value = await pep.registerParticipant({
    firstName: participantFirstName.value,
    middleName: participantMiddleName.value,
    lastName: participantLastName.value,
    dateOfBirth: participantDateOfBirth.valueAsDate,
  });
})());

async function updateEnrollStatus() {
  let enrolledUser = await pep.getEnrolledUser();
  userInfo.textContent = enrolledUser ? `Logged in as ${enrolledUser.userGroup} ${enrolledUser.user}` : '';
  loggedIn.hidden = !enrolledUser;
  logoutBtn.disabled = !enrolledUser;
}
void updateEnrollStatus();

loginBtn.addEventListener('click', () => {
  void (async () => {
    loginBtn.disabled = true;
    try {
      await pep.authenticate();
    } finally {
      loginBtn.disabled = false;
    }
    void updateEnrollStatus();
  })();
});
logoutBtn.addEventListener('click', () => {
  void (async () => {
    await pep.unenroll();
    void updateEnrollStatus();
  })();
});
