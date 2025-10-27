import Pep from 'pep-repo-client';
import {binaryToString, deleteObjectsAsync} from 'pep-repo-client/utils';

function printExtraErrorDetails(ex) {
  if (ex instanceof Error) {
    if (ex.cause) {
      // Browsers don't always display full cause
      console.error('Cause:', ex.cause);
    }
    if (ex.stack) {
      // Firefox messes up WASM stack when printing Error
      console.error('Raw stack:', ex.stack);
    }
  }
}

addEventListener('error', ev => {
  if (ev.error) {
    printExtraErrorDetails(ev.error);
  }
  alert(ev.error || ev.message);
});
addEventListener('unhandledrejection', ev => {
  printExtraErrorDetails(ev.reason);
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
    retrieveBtn = /** @type {HTMLButtonElement} */ (document.getElementById('retrieve-btn'));
const output = /** @type {HTMLTextAreaElement} */ (document.getElementById('output'));

const
    participantFirstName = /** @type {HTMLInputElement} */ (document.getElementById('participant-first-name')),
    participantMiddleName = /** @type {HTMLInputElement} */ (document.getElementById('participant-middle-name')),
    participantLastName = /** @type {HTMLInputElement} */ (document.getElementById('participant-last-name')),
    participantDateOfBirth = /** @type {HTMLInputElement} */ (document.getElementById('participant-date-of-birth'));
const registerParticipantBtn = /** @type {HTMLButtonElement} */ (document.getElementById('register-participant-btn'));

output.value = '';

const pep = await Pep.create({
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

  const jsonEntries = entries.map(entry => ({
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
  const res = pep.retrieve(entries);
  const jsonEntries = [];

  try {
    // @ts-ignore
    for await (/** @type {import("pep-repo-client").CellData} */ const data of res) {
      try {
        let content = '';
        {
          const decoder = new TextDecoder();
          const chunks = data.content;
          try {
            for await (/** @type {import("pep-repo-client").Buffer} */ const chunk of chunks) {
              try {
                content += decoder.decode(new Uint8Array(chunk.view()), {stream: true});
              } finally {
                chunk.delete();
              }
            }
          } finally {
            await deleteObjectsAsync(chunks);
          }
          content += decoder.decode();
        }

        jsonEntries.push({
          subjectLocalPseudonym: data.entry.subjectLocalPseudonym,
          column: data.entry.column,
          content: content,
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
