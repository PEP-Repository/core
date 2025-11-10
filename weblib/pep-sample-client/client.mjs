import Pep from 'pep-repo-client';
import {binaryToString, concatStringsAsync, deleteObjectsAsync} from 'pep-repo-client/utils';

/** @type {Pep|undefined} */
let pep;

/**
 * @param {*} ex
 * @param {Event} ev
 */
function handleMaybeWasmException(ex, ev) {
  //@ts-ignore
  if (pep && ex && ex instanceof WebAssembly.Exception) {
    const error = pep.handleWasmException(ex);
    alert(error);
    console.error('Uncaught', error);
    ev.preventDefault();
  }
}

addEventListener('error', ev => {
  // Ignore noise from background threads, see /cpp/weblib/prejs.js
  if (ev.error instanceof ErrorEvent || (!ev.error && ev.message === 'Uncaught [object WebAssembly.Exception]')) {
    return;
  }
  handleMaybeWasmException(ev.error, ev);
  alert(ev.error || ev.message);
});
addEventListener('unhandledrejection', ev => {
  handleMaybeWasmException(ev.reason, ev);
  alert(ev.reason);
});

const loading = document.getElementById('loading');
const working = document.getElementById('working');
const loginBtn = /** @type {HTMLButtonElement} */ (document.getElementById('login-btn'));
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
    for (const entry of entries) {
      entry.delete();
    }
  }
  // @ts-ignore
  entries = await Array.fromAsync(res);
  retrieveBtn.disabled = false;
  saveBtn.disabled = false;

  const jsonEntries = entries.map(entry => ({
    id: entry.id,
    subjectLocalPseudonym: entry.subjectLocalPseudonym,
    column: entry.column,
    fileSize: entry.fileSize.toString(),
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
    // @ts-ignore
    for await (/** @type {import("pep-repo-client").CellData} */ const data of res) {
      try {
        jsonEntries.push({
          id: data.entry.id,
          subjectLocalPseudonym: data.entry.subjectLocalPseudonym,
          column: data.entry.column,
          content: await concatStringsAsync(data.content.pipeThrough(new TextDecoderStream())),
        });

      } finally {
        data.delete();
      }
    }
  } finally {
    // @ts-ignore
    await deleteObjectsAsync(res);
  }
  output.value = JSON.stringify(jsonEntries, null, '  ');
})());
saveBtn.addEventListener('click', () => void (async () => {
  const entry = entries.find(e => e.fileSize > 10e6 /*10 MB*/);
  if (!entry) {
    throw new Error('No cells over 10MB, upload one via pepcli');
  }
  output.value = JSON.stringify({
    id: entry.id,
    subjectLocalPseudonym: entry.subjectLocalPseudonym,
    column: entry.column,
    fileSize: entry.fileSize.toString(),
  }, null, '  ');

  const fileExtensionBin = entry.partialMetadataView().get('fileExtension');
  const fileName = 'file' + (fileExtensionBin ? binaryToString(fileExtensionBin) : '');

  /** @type {import("pep-repo-client").CellData | undefined} */
  let data;
  async function getContent() {
    //@ts-ignore
    [data] = await Array.fromAsync(pep.retrieve([entry]));
    const startTime = performance.now();
    let downloaded = 0n;
    let prevTime = startTime;
    return data.content.pipeThrough(new TransformStream({
      transform(chunk, controller) {
        downloaded += BigInt(chunk.byteLength);
        const now = performance.now();
        if (now > prevTime + 500 || downloaded === entry.fileSize) {
          prevTime = now;
          output.value = `${Number(downloaded) / 1e6} / ${Number(entry.fileSize) / 1e6} MB downloaded (${Math.floor(Number(downloaded) / Number(entry.fileSize) * 100)}% complete)`;
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
      //@ts-ignore
      const file = await showSaveFilePicker({suggestedName: fileName});
      await (await getContent()).pipeTo(await file.createWritable());

    } else {
      console.warn('Browser does not support streaming file download, will buffer to memory');
      //@ts-ignore
      const blob = new Blob(await Array.fromAsync(await getContent()));
      const blobUrl = URL.createObjectURL(blob);

      const a = document.createElement('a')
      a.href = blobUrl;
      a.download = fileName;
      a.click();
      // Unclear if a delay is required, but let's be sure the download started,
      //  see https://stackoverflow.com/a/71164969/4454665
      setTimeout(() => URL.revokeObjectURL(blobUrl), 1_000);
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
