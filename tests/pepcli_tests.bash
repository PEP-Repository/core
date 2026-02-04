# shellcheck disable=SC2034

# You can add BP=1 in front of any 'pepcli', 'execute' or 'trace' line to add a breakpoint

# This script is meant to only be run from within integration.sh.

readonly DEST_DIR="$CONFIG_DIR/test_output"
execute . mkdir -p "$DEST_DIR"

readonly ACCESS_ADMINISTRATOR_TOKEN="ewogICAgInN1YiI6ICJBY2Nlc3MgQWRtaW5pc3RyYXRvciIsCiAgICAiZ3JvdXAiOiAiQWNjZXNzIEFkbWluaXN0cmF0b3IiLAogICAgImlhdCI6ICIxNTcyMzU0MjI4IiwKICAgICJleHAiOiAiMjA3MzY1NDEyMiIKfQo.DYnQyvtpvj2OozTnC6MUMJKW7G-ckzber0q0kRnjwHQ"

# MacOS doesn't support the date command with nanosecond precision, so we need to use GNU coreutils gdate instead.
if [[ "$(uname)" == "Darwin" ]]; then
    # Ensure GNU date is installed
    if ! command -v gdate > /dev/null; then
        fail "gdate could not be found, please install coreutils using Homebrew."
    fi
    DATE_CMD="gdate"
else
    DATE_CMD="date"
fi

# Output pepcli's version info: version-logging-on-startup is suppressed
pepcli --version

#TESTS

TEST_PARTICIPANT="$(openssl rand -base64 12)"

####################

if should_run_test basic; then
  # Store a PEP ID...
  id=$(pepcli --oauth-token-group "Research Assessor" register id | grep "identifier:" | cut -d':' -f2 | xargs)
  # ... then verify that we can read it back (see #2750)
  pepcli validate data
  # Clean up: delete the ID that we stored
  write_registration_server_cell ParticipantIdentifier delete -p "$id"

  pepcli --oauth-token-group "Research Assessor" pull -P '*' -C ShortPseudonyms -c ParticipantIdentifier -c DeviceHistory -c ParticipantInfo --output-directory "$DEST_DIR/pulled-data"
  pepcli list -l -P '*' -c ParticipantIdentifier -c DeviceHistory -c ParticipantInfo
  execute . rm -rf "$DEST_DIR/pulled-data"

  # Store data in a not yet existing row
  id="$(pepcli store -p "$TEST_PARTICIPANT" -c DeviceHistory -d random-data \
    --ticket-out "$DEST_DIR/ticket" \
    --metadataxentry "$(pepcli xentry --name mymetakey --payload mymetadata)")"
  id="$(printf %s "$id" | jq --raw-output .id)"

  value="$(pepcli get --ticket "$DEST_DIR/ticket" --id "$id" --output-file -)"
  [ "$value" = random-data ] || fail "Expected [random-data], got [$value]"

  value="$(pepcli get --ticket "$DEST_DIR/ticket" --id "$id" --metadata -)"
  value="$(printf %s "$value" | jq --compact-output .extra)"
  desired_value="$(printf %s mymetadata | jq --raw-input --compact-output '{"mymetakey": {"payload": . | @base64}}')"
  [ "$value" = "$desired_value" ] || fail "Expected [$desired_value], got [$value]"

  desired_value=$'random data with\nLF newline'
  id="$(pepcli store -p "$TEST_PARTICIPANT" -c DeviceHistory -d "$desired_value")"
  id="$(printf %s "$id" | jq --raw-output .id)"

  value="$(pepcli get --ticket "$DEST_DIR/ticket" --id "$id" --output-file -)"
  [ "$value" = "$desired_value" ] || fail "Expected [$desired_value], got [$value]"

  execute . rm "$DEST_DIR/ticket"

  # Ensure that MRI column exists and is read+writable for "Research Assessor" (who will upload and download)
  pepcli --oauth-token-group "Data Administrator" ama column create Visit1.MRI.Func
  pepcli --oauth-token-group "Data Administrator" ama columnGroup create MRI
  pepcli --oauth-token-group "Data Administrator" ama column addTo Visit1.MRI.Func MRI
  pepcli --oauth-token-group "Access Administrator" ama cgar create MRI "Research Assessor" read
  pepcli --oauth-token-group "Access Administrator" ama cgar create MRI "Research Assessor" write

  # Ensure we have an SP value in the database that can be pseudonymized in the upload
  write_registration_server_cell ShortPseudonym.Visit1.FMRI store -p "$TEST_PARTICIPANT" -d GUM123456751
  
  pepcli --oauth-token-group "Access Administrator" user group create Read.ShortPseudonym.Visit1
  pepcli --oauth-token-group "Data Administrator" ama columnGroup create Just.ShortPseudonym.Visit1
  pepcli --oauth-token-group "Data Administrator" ama column addTo ShortPseudonym.Visit1.FMRI Just.ShortPseudonym.Visit1
  pepcli --oauth-token-group "Access Administrator" ama cgar create Just.ShortPseudonym.Visit1 Read.ShortPseudonym.Visit1 read
  pepcli --oauth-token-group "Access Administrator" ama pgar create '*' Read.ShortPseudonym.Visit1 access
  pepcli --oauth-token-group "Access Administrator" ama pgar create '*' Read.ShortPseudonym.Visit1 enumerate
  pepcli --oauth-token-group Read.ShortPseudonym.Visit1 pull --all-accessible --output-directory "$DEST_DIR"/pulled_all_accessible

  FOUND_FILES_COUNT=$(execute . find "$DEST_DIR"/pulled_all_accessible -type f | wc -l)
  EXPECTED_COUNT=3
  if [ "$FOUND_FILES_COUNT" -ne "$EXPECTED_COUNT" ]; then
      >&2 printYellow "An unexpected amount of files was found in 'pulled_all_accessible': Expected $EXPECTED_COUNT, found $FOUND_FILES_COUNT"
      >&2 execute . find "$DEST_DIR"/pulled_all_accessible -type f
      exit 1
  fi

  # Clean up everything we created to do the pull and file count
  execute . rm -rf "$DEST_DIR"/pulled_all_accessible
  pepcli --oauth-token-group "Access Administrator" ama pgar remove "*" Read.ShortPseudonym.Visit1 enumerate
  pepcli --oauth-token-group "Access Administrator" ama pgar remove "*" Read.ShortPseudonym.Visit1 access
  pepcli --oauth-token-group "Access Administrator" ama cgar remove Just.ShortPseudonym.Visit1 Read.ShortPseudonym.Visit1 read
  pepcli --oauth-token-group "Data Administrator" ama column removeFrom ShortPseudonym.Visit1.FMRI Just.ShortPseudonym.Visit1
  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove Just.ShortPseudonym.Visit1
  pepcli --oauth-token-group "Access Administrator" user group remove Read.ShortPseudonym.Visit1

  SYMLINK_TEST_DATA="$TEST_INPUT_DIR/pepcli/store/test_symlink_folder_structure"
  # Store a directory structure with symlinks with the -resolve-symlinks flag
  pepcli --oauth-token-group "Research Assessor" store -p "$TEST_PARTICIPANT" -c Visit1.MRI.Func -i "$SYMLINK_TEST_DATA" --resolve-symlinks

  printYellow "The next command produces an error. This is expected and correct behaviour."
  # Attempt to store a directory structure with symlinks withOUT the -resolve-symlinks flag. This should fail and PROCESS_FAILED_AS_INTENDED should be set to true.
  pepcli --oauth-token-group "Research Assessor" store -p "$TEST_PARTICIPANT" -c Visit1.MRI.Func -i "$SYMLINK_TEST_DATA" &&
      fail "Storing a directory structure with symlinks withOUT the -resolve-symlinks flag unexpectedly succeeded."

  # Store something so large that it will be sent to the page store, i.e. larger than INLINE_PAGE_THRESHOLD ( = 4*1024 bytes ).
  readonly RANDOM_DATA_FILE="$DEST_DIR/random-data.bin"
  execute . dd if=/dev/urandom of="$RANDOM_DATA_FILE" bs=1024 count=6
  pepcli store -p "$TEST_PARTICIPANT" -c DeviceHistory -i "$RANDOM_DATA_FILE"
  # Download the data we just stored and compare it to the original data.
  pepcli pull --output-directory "$DEST_DIR/pulled" -p "$TEST_PARTICIPANT" -c DeviceHistory
  # We use "find" to locate the downloaded file because it's in a subdirectory named after the participant's local pseudonym, which we don't know.
  # find always exits with 0 exit code, but it only prints the found path(s) if the -exec part succeeds. Therefore we can use grep to check
  # whether it has found the DeviceHistory directory, and the diff succeeded.
  execute . find "$DEST_DIR/pulled" -name DeviceHistory.bin -exec diff "$RANDOM_DATA_FILE" {} -q \; -print | grep DeviceHistory
  execute . rm -rf "$DEST_DIR/pulled"

  # Deleting a nonexistent (empty) cell should fail: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2367
  pepcli --oauth-token-group "Research Assessor" delete -p "$TEST_PARTICIPANT" -c StudyContexts &&
      fail "Deleting a nonexistent (empty) cell should fail."

  # Store empty file and download it again: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2337
  pepcli --oauth-token-group "Research Assessor" store -p "$TEST_PARTICIPANT" -c StudyContexts -i "$TEST_INPUT_DIR"/pepcli/store/EmptyStudyContexts.csv
  pepcli --oauth-token-group "Research Assessor" pull  -p "$TEST_PARTICIPANT" -c StudyContexts -o "$DEST_DIR"/pulled-empty-file
  execute . rm -rf "$DEST_DIR"/pulled-empty-file

  # Delete the (empty) file that we previously stored
  pepcli --oauth-token-group "Research Assessor" delete -p "$TEST_PARTICIPANT" -c StudyContexts
  # Trying to delete it again should fail: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2367
  pepcli --oauth-token-group "Research Assessor" delete -p "$TEST_PARTICIPANT" -c StudyContexts &&
      fail "Deleting a cell a second time should fail."

  # Querying column-access when no access is granted should not fail.
  pepcli --oauth-token-group "Access Administrator" user group create groupWithoutAccess
  pepcli --oauth-token-group "groupWithoutAccess" query column-access

  write_registration_server_cell ShortPseudonym.Visit1.FMRI delete -p "$TEST_PARTICIPANT"

  pepcli --oauth-token-group "Research Assessor" delete -p "$TEST_PARTICIPANT" -c DeviceHistory
  pepcli --oauth-token-group "Research Assessor" delete -p "$TEST_PARTICIPANT" -c Visit1.MRI.Func

  pepcli --oauth-token-group "Access Administrator" user group remove groupWithoutAccess

  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove MRI --force
  pepcli --oauth-token-group "Data Administrator" ama column remove Visit1.MRI.Func

fi

####################

if should_run_test file-extension; then

  # Create column and column group for file extension tests
  pepcli --oauth-token-group "Data Administrator" ama column create FileExtensionTest
  pepcli --oauth-token-group "Data Administrator" ama columnGroup create FileExtensionTestGroup
  pepcli --oauth-token-group "Data Administrator" ama column addTo FileExtensionTest FileExtensionTestGroup
  pepcli --oauth-token-group "Access Administrator" ama cgar create FileExtensionTestGroup "Research Assessor" read
  pepcli --oauth-token-group "Access Administrator" ama cgar create FileExtensionTestGroup "Research Assessor" write

  # create a file with an extension and store it in the column
  pepcli --oauth-token-group "Research Assessor" store -p "$TEST_PARTICIPANT" -c FileExtensionTest -i "$TEST_INPUT_DIR"/fileExtensionData1.txt
  # data is pulled and then it is checked whether the data that is stored is the same as the data that we put in
  pepcli --oauth-token-group "Research Assessor" pull -p "$TEST_PARTICIPANT" -c FileExtensionTest --output-directory "$DEST_DIR/pulled-data" --force
  execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt -exec diff "$TEST_INPUT_DIR"/fileExtensionData1.txt {} -q \; -print

  # an update is done, this should be a no opt
  pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update

  # new data is stored, updated and then checked whether this update has worked i.e. that the new data is being retrieved
  pepcli --oauth-token-group "Research Assessor" store -p "$TEST_PARTICIPANT" -c FileExtensionTest -i "$TEST_INPUT_DIR"/fileExtensionData2.txt
  pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update
  execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt -exec diff "$TEST_INPUT_DIR"/fileExtensionData2.txt {} -q \; -print

  # the pulled data is changed (made dirty). A new update now should fail.
  DIRTY_FILE="$(execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt)"
  echo "$DIRTY_FILE"
  execute . bash -c "echo 'A full commitment is what I am thinking of' > '$DIRTY_FILE'"
  printYellow "The next command produces an error. This is expected and correct behaviour."
  pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update &&
      fail "Updating dirty pulled data should fail."

  # Dirty pulled-data updated with --force switch.
  pepcli --oauth-token-group "Research Assessor" store -p "$TEST_PARTICIPANT" -c FileExtensionTest -i "$TEST_INPUT_DIR"/fileExtensionData3.txt
  pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update --force
  execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt -exec diff "$TEST_INPUT_DIR"/fileExtensionData3.txt {} -q \; -print

  # Data stored with --file-extension parameter
  pepcli --oauth-token-group "Research Assessor" store -p "$TEST_PARTICIPANT" -c FileExtensionTest -i "$TEST_INPUT_DIR"/fileExtensionData3.txt --file-extension ".pdf"
  pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update
  execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.pdf -exec diff "$TEST_INPUT_DIR"/fileExtensionData3.txt {} -q \; -print
  if [ -n "$(execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt)" ]; then
    fail "Old .txt file should be deleted."
  fi

  execute . rm -rf "$DEST_DIR/pulled-data"

  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove FileExtensionTestGroup --force
  pepcli --oauth-token-group "Data Administrator" ama column remove FileExtensionTest
fi

####################

if should_run_test structure-history; then

  # Create a user group, remove it later and then verify that we can query the group that was removed through the --at option
  pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" user group create onceUponATimeGroup
  UTC_TIMESTAMP=$($DATE_CMD +%s%N | cut -b1-13)
  pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" user group remove onceUponATimeGroup
  pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" user query --at "$UTC_TIMESTAMP" | grep 'onceUponATimeGroup'

  # Create a column, remove it later and then verify that we can still query the column that was removed through the --at option
  pepcli --oauth-token-group "Data Administrator" ama column create onceUponATimeColumn
  UTC_TIMESTAMP=$($DATE_CMD +%s%N | cut -b1-13)
  pepcli --oauth-token-group "Data Administrator" ama column remove onceUponATimeColumn
  pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" ama query --at "$UTC_TIMESTAMP" | grep 'onceUponATimeColumn'

fi

####################

if should_run_test ama; then

  # Create a column group, add columns to it and try to remove it.
  pepcli --oauth-token-group "Data Administrator" ama column create blockingColumn
  pepcli --oauth-token-group "Data Administrator" ama columnGroup create columnGroupWithColumn
  pepcli --oauth-token-group "Data Administrator" ama column addTo blockingColumn columnGroupWithColumn

  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove columnGroupWithColumn &&
      fail "removing a columngroup with associated columns should fail."
  # Adding the --force flag should make it succeed
  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove columnGroupWithColumn --force

  # Create a column group, add a access rule and try to remove it.
  pepcli --oauth-token-group "Data Administrator" ama columnGroup create columnGroupWithAccessRule
  pepcli --oauth-token-group "Access Administrator" ama cgar create columnGroupWithAccessRule "SomeUserGroup" "read"
  pepcli --oauth-token-group "Access Administrator" ama cgar create columnGroupWithAccessRule "SomeUserGroup" "write"


  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove columnGroupWithAccessRule &&
      fail "removing a columngroup with associated access rules should fail."
  # Adding the --force flag should make it succeed
  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove columnGroupWithAccessRule --force


  # Create participant
  SEARCH="Generated participant with identifier: "
  RESULT=$(pepcli --oauth-token-group "Research Assessor" register id 2>&1 | grep "$SEARCH")
  ID="${RESULT:${#SEARCH}}"

  # Create a participantgroup
  pepcli --oauth-token-group "Data Administrator" ama group create participantGroupWithParticipant
  pepcli --oauth-token-group "Data Administrator" ama group addTo participantGroupWithParticipant "$ID"

  # Attempt to remove the participant group
  pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithParticipant &&
      fail "removing a participantgroup with associated participants should fail."
  # Adding the --force flag should make it succeed
  pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithParticipant --force


  # Create a participantgroup and add a access rule
  pepcli --oauth-token-group "Data Administrator" ama group create participantGroupWithAccessRule
  pepcli --oauth-token-group "Access Administrator" ama pgar create participantGroupWithAccessRule "SomeUserGroup" "enumerate"
  pepcli --oauth-token-group "Access Administrator" ama pgar create participantGroupWithAccessRule "SomeUserGroup" "access"

  # Attempt to remove the participant group
  pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithAccessRule &&
      fail "removing a participantgroup with associated participants should fail."
  # Adding the --force flag should make it succeed
  pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithAccessRule --force

  write_registration_server_cell ParticipantIdentifier delete -p "$ID"

  pepcli --oauth-token-group "Data Administrator" ama column remove blockingColumn
fi

####################

if should_run_test token-block; then

  pepcli --oauth-token-group "Access Administrator"  user group create integrationGroup

  # Attempt to print the blocklist as an access administrator
  pepcli --oauth-token-group "Access Administrator" token block list

  # Attempt to print the blocklist as a user that is not an access administrator
  pepcli --oauth-token-group "Data Administrator" token block list &&
      fail "Users that are not access administrators should not have access to the blocklist"

  # Add a new user to integrationGroup and generate token for that user
  pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" user create userWithFreshToken
  pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" user addTo userWithFreshToken integrationGroup
  TOKEN_TEST_USER_TOKEN=$(pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" token request userWithFreshToken integrationGroup "$($DATE_CMD -d '2 days' +%s)")

  # Ensure that integrationGroup has read access to at least one column
  pepcli --oauth-token-group "Data Administrator" ama column create tokenBlockingColumn
  pepcli --oauth-token-group "Data Administrator" ama columnGroup create tokenBlockingColumnGroup
  pepcli --oauth-token-group "Data Administrator" ama column addTo tokenBlockingColumn tokenBlockingColumnGroup
  pepcli --oauth-token-group "Access Administrator" ama cgar create tokenBlockingColumnGroup integrationGroup read

  # Attempt to do a query with the generated token
  pepcli --oauth-token "$TOKEN_TEST_USER_TOKEN" query column-access

  # Block the token just generated
  pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" token block create userWithFreshToken integrationGroup -m "Blocked as part of test"

  # Attempt to redo the query with the token that is now blocked
  pepcli --oauth-token "$TOKEN_TEST_USER_TOKEN" query column-access &&
      fail "query with a blocked token should fail."

  # Unblock the token
  BLOCKLIST_ENTRY_ID=$(pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" token block list | grep userWithFreshToken | tail -n1 | cut -d ';' -f1 | tr -d '"')
  pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" token block remove "$BLOCKLIST_ENTRY_ID"

  # Attempt to redo the query with the token that is no longer blocked
  pepcli --oauth-token "$TOKEN_TEST_USER_TOKEN" query column-access

  # Removing a user from a user group should block tokens
  pepcli --oauth-token-group "Access Administrator" user removeFrom userWithFreshToken integrationGroup

   # Attempt to redo the query with the token that is now blocked
  pepcli --oauth-token "$TOKEN_TEST_USER_TOKEN" query column-access &&
      fail "Removing a user from a user group should block tokens for that user/usergroup"

  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove tokenBlockingColumnGroup --force
  pepcli --oauth-token-group "Data Administrator" ama column remove tokenBlockingColumn
  pepcli --oauth-token-group "Access Administrator" user remove userWithFreshToken
  pepcli --oauth-token-group "Access Administrator" user group remove integrationGroup
fi

####################

if should_run_test authserver-apache; then
  if [ "$LOCAL" = true ]; then
    curlCmd() { curl "$@"; }
    AUTHSERVER=localhost
  else
    curlCmd() { docker run --rm --net pep-network curlimages/curl "$@"; }
    AUTHSERVER=pepservertest
  fi

  test_authserver_request() {
    primaryUid="$1";
    humanReadableUid="$2";
    alternativeUids="$3";
    expectError="${4:-false}"
    spoofKey="${5:-"$(cat "$DATA_DIR"/authserver/spoofKey)"}"
    alternativeUidsHeader="PEP-Alternative-Uids;"
    if [ -n "$alternativeUids" ]; then
      alternativeUidsHeader="PEP-Alternative-Uids: $alternativeUids"
    fi
    trace curlCmd -i -H "PEP-Primary-Uid: $primaryUid" -H "PEP-Human-Readable-Uid: $humanReadableUid" -H "$alternativeUidsHeader" -H "PEP-Spoof-Check: $spoofKey" \
      "http://$AUTHSERVER:8080/auth?client_id=123&code_challenge=NCXJvk7daJeLDY8xw3KxsX8oRaLcXR-p7Tvzt9yjE80&code_challenge_method=S256&redirect_uri=http://localhost:16515/&response_type=code&primary_uid=$(url_encode "$primaryUid")&human_readable_uid=$(url_encode "$humanReadableUid")&alternative_uids=$(url_encode "$alternativeUids")" > "$DATA_DIR/authserverResponse.txt"
    if [ "$expectError" = false ]; then
      trace grep "Location: http://localhost:16515.*[\?&]code=" "$DATA_DIR/authserverResponse.txt"
    else
      trace grep "Location: http://localhost:16515.*[\?&]error=$expectError" "$DATA_DIR/authserverResponse.txt"
    fi
  }

  pepcli --oauth-token-group "Access Administrator" user create integrationUser
  pepcli --oauth-token-group "Access Administrator" user group create integrationGroup
  pepcli --oauth-token-group "Access Administrator" user addTo integrationUser integrationGroup

  pepcli --oauth-token-group "Access Administrator" user create difficultUser
  pepcli --oauth-token-group "Access Administrator" user addIdentifier difficultUser "\"something with comma's, and spaces\"@example.com" # comma's and spaces are allowed if the part before the @ is between quotes.
  pepcli --oauth-token-group "Access Administrator" user addIdentifier difficultUser "difficultuser+pep@example.com"
  pepcli --oauth-token-group "Access Administrator" user addTo difficultUser integrationGroup

  INTEGRATION_USER_PRIMARY_UID="TRnQNJSLx5RFD8VxfzD2HfTsEZ9cT4UsilWw8aiB1ZY"
  DIFFICULT_USER_PRIMARY_UID="MWE8U4BPnAJr27HjAqWD8DucHkFUTgDxLa4zSw7R9Bg"
  printYellow "We expect an error when user 'integrationUser' logs in with his primary UID."
  printYellow "The user is currently only known as 'integrationUser'. So not by e.g. his e-mailaddress, nor by his non-human-readable primary UID."
  test_authserver_request "$INTEGRATION_USER_PRIMARY_UID" integrationUser@example.com "" access_denied
  printYellow "When he logs in with his known UID 'integrationUser' (in this case specified as an alternative UID), this succeeds. Furthermore, his primary UID is now added to the database."
  test_authserver_request "$INTEGRATION_USER_PRIMARY_UID" integrationUser@example.com "integrationUser,integration_user"
  printYellow "so this time login should succeed, since the primary UID is now known to the system."
  test_authserver_request "$INTEGRATION_USER_PRIMARY_UID" integrationUser@example.com ""
  printYellow "but if he now logs in with 'integrationUser' in alternative-UIDs, but with a different primary UID, it should fail"
  test_authserver_request "wrong-primary-uid" integrationUser@example.com "integrationUser,integration_user" server_error

  test_authserver_request eve eve "" access_denied

  # Test if alternative UIDs with comma's are correctly split and decoded
  test_authserver_request "$DIFFICULT_USER_PRIMARY_UID" difficultuser@example.com "%22something%20with%20comma's%2C%20and%20spaces%22%40example.com,second_alternative"

  # Test if alternative UIDs with a plus are correctly URL-decoded
  test_authserver_request "$DIFFICULT_USER_PRIMARY_UID" difficultuser@example.com difficultuser%2Bpep%40example.com

  pepcli --oauth-token-group "Access Administrator" user removeFrom difficultUser integrationGroup
  pepcli --oauth-token-group "Access Administrator" user removeIdentifier "difficultuser+pep@example.com"
  pepcli --oauth-token-group "Access Administrator" user removeIdentifier "\"something with comma's, and spaces\"@example.com"
  pepcli --oauth-token-group "Access Administrator" user remove difficultUser

  pepcli --oauth-token-group "Access Administrator" user removeFrom integrationUser integrationGroup
  pepcli --oauth-token-group "Access Administrator" user remove integrationUser
  pepcli --oauth-token-group "Access Administrator" user group remove integrationGroup
fi

####################

if should_run_test user-query; then
  userDisplayId="user-query-test-user"
  userPrimaryId="user-query-test-primary-id"
  userAlternativeIds=("user-query-test-alternative1" "user-query-test-alternative2")
  [ "$(pepcli --oauth-token-group "Access Administrator" user query --format json | jq '."All Interactive Users" | any(."display id" == "'$userDisplayId'")')" == "false" ]
  pepcli --oauth-token-group "Access Administrator" user create "$userDisplayId"
  pepcli --oauth-token-group "Access Administrator" user addIdentifier --primary-id "$userDisplayId" "$userPrimaryId"
  for id in "${userAlternativeIds[@]}"; do
    pepcli --oauth-token-group "Access Administrator" user addIdentifier "$userPrimaryId" "$id"
  done
  queryResult="$(pepcli --oauth-token-group "Access Administrator" user query --format json | jq '."All Interactive Users"[] | select(."primary id" == "'$userPrimaryId'")')"
  [ -n "$queryResult" ]
  returnedDisplayId=$(echo "$queryResult" | jq --raw-output '."display id"')
  [ "$returnedDisplayId" == "$userDisplayId" ]
  returnedPrimaryId=$(echo "$queryResult" | jq --raw-output '."primary id"')
  [ "$returnedPrimaryId" == "$userPrimaryId" ]
  returnedAlternativeIds=$(echo "$queryResult" | jq --raw-output '."other user identifiers"[]')
  echo "'$returnedAlternativeIds'"
  for id in "${userAlternativeIds[@]}"; do
    echo "$returnedAlternativeIds" | grep "$id"
  done

  pepcli --oauth-token-group "Access Administrator" user remove "$userPrimaryId"
fi

####################

if should_run_test structure-metadata; then

  # Add some entries
  pepcli --oauth-token-group "Data Administrator" ama column create columnWithMetadata
  meta_value=$'Super cool value!\nEven with multiple lines! :D'
  pepcli --oauth-token-group "Data Administrator" structure-metadata column set columnWithMetadata --key mygroup:from_param --value "$meta_value"
  printf %s "$meta_value" | pepcli --oauth-token-group "Data Administrator" structure-metadata column set columnWithMetadata --key mygroup:from_stdin

  # Check presence of entries
  meta_value_retrieved="$(pepcli --oauth-token-group "Research Assessor" structure-metadata column get columnWithMetadata --key mygroup:from_param)"
  [ "$meta_value_retrieved" = "$meta_value" ] || fail "Expected [$meta_value], got [$meta_value_retrieved]"

  meta_value_retrieved="$(pepcli --oauth-token-group "Research Assessor" structure-metadata column get columnWithMetadata --key mygroup:from_stdin)"
  [ "$meta_value_retrieved" = "$meta_value" ] || fail "Expected [$meta_value], got [$meta_value_retrieved]"

  # Check write permission
  pepcli --oauth-token-group "Research Assessor" structure-metadata column set columnWithMetadata --key mygroup:should_fail --value myval &&
    fail "Only Data Administrator should be able to set structure metadata"

  # Does list return the entries?
  list_meta_value=$'Super cool value with some "quotes" but no newlines since list command may transform to OS-specific line endings'
  # overwrite
  pepcli --oauth-token-group "Data Administrator" structure-metadata column set columnWithMetadata --key mygroup:from_param --value "$list_meta_value"
  printf %s "$list_meta_value" | pepcli --oauth-token-group "Data Administrator" structure-metadata column set columnWithMetadata --key mygroup:from_stdin
  meta_value_retrieved="$(pepcli --oauth-token-group "Research Assessor" structure-metadata column list)"
  contains "$meta_value_retrieved" columnWithMetadata || fail "Expected string containing [columnWithMetadata], got [$meta_value_retrieved]"
  contains "$meta_value_retrieved" "$list_meta_value" || fail "Expected string containing [$list_meta_value], got [$meta_value_retrieved]"
  contains "$meta_value_retrieved" from_param || fail "Expected string containing [from_param], got [$meta_value_retrieved]"
  contains "$meta_value_retrieved" from_stdin || fail "Expected string containing [from_stdin], got [$meta_value_retrieved]"

  meta_value_retrieved="$(pepcli --oauth-token-group "Research Assessor" structure-metadata column list columnWithMetadata --json | jq --sort-keys)"
  meta_expected="$(jq --sort-keys --null-input \
  '{
    "columnWithMetadata": {
      "mygroup:from_param": $val,
      "mygroup:from_stdin": $val
    }
  }' \
  --arg val "$list_meta_value"
  )"
  diff <(printf %s "$meta_value_retrieved") <(printf %s "$meta_expected")

  # Check list filtering
  pepcli --oauth-token-group "Data Administrator" structure-metadata column set columnWithMetadata --key another_group:mykey --value myval

  meta_value_retrieved="$(pepcli --oauth-token-group "Research Assessor" structure-metadata column list columnWithMetadata --key 'mygroup:*')"
  contains "$meta_value_retrieved" "$list_meta_value" || fail "Expected string containing [$list_meta_value], got [$meta_value_retrieved]"
  contains "$meta_value_retrieved" from_param || fail "Expected string containing [from_param], got [$meta_value_retrieved]"
  contains "$meta_value_retrieved" from_stdin || fail "Expected string containing [from_stdin], got [$meta_value_retrieved]"
  contains "$meta_value_retrieved" another_group && fail "Expected string excluding [another_group], got [$meta_value_retrieved]"

  # Check remove
  pepcli --oauth-token-group "Data Administrator" structure-metadata column remove columnWithMetadata --key another_group:mykey
  pepcli --oauth-token-group "Data Administrator" structure-metadata column get columnWithMetadata --key another_group:mykey &&
    fail "Metadata entry should have been removed"

  # Add some entries for other metadata types
  pepcli --oauth-token-group "Data Administrator" ama columnGroup create columnGroupWithMetadata
  pepcli --oauth-token-group "Data Administrator" ama group create participantGroupWithMetadata
  pepcli --oauth-token-group "Access Administrator" user group create userGroupWithMetadata
  pepcli --oauth-token-group "Data Administrator" structure-metadata column-group set columnGroupWithMetadata --key mygroup:mykey --value "$meta_value"
  pepcli --oauth-token-group "Data Administrator" structure-metadata participant-group set participantGroupWithMetadata --key mygroup:mykey --value "$meta_value"
  pepcli --oauth-token-group "Access Administrator" structure-metadata user-group set userGroupWithMetadata --key mygroup:mykey --value "$meta_value"

  # Check presence of entries
  meta_value_retrieved="$(pepcli --oauth-token-group "Research Assessor" structure-metadata column-group get columnGroupWithMetadata --key mygroup:mykey)"
  [ "$meta_value_retrieved" = "$meta_value" ] || fail "Expected [$meta_value], got [$meta_value_retrieved]"
  meta_value_retrieved="$(pepcli --oauth-token-group "Research Assessor" structure-metadata participant-group get participantGroupWithMetadata --key mygroup:mykey)"
  [ "$meta_value_retrieved" = "$meta_value" ] || fail "Expected [$meta_value], got [$meta_value_retrieved]"
  meta_value_retrieved="$(pepcli --oauth-token-group "Access Administrator" structure-metadata user-group get userGroupWithMetadata --key mygroup:mykey)"
  [ "$meta_value_retrieved" = "$meta_value" ] || fail "Expected [$meta_value], got [$meta_value_retrieved]"

  # Test metadata using multiple userIDs belonging to the same user
  pepcli --oauth-token-group "Access Administrator" user create userWithMetadata
  pepcli --oauth-token-group "Access Administrator" user addIdentifier userWithMetadata userWithMetadataAlternative
  pepcli --oauth-token-group "Access Administrator" structure-metadata user set userWithMetadata --key mygroup:mykey --value "$meta_value"
  meta_value_retrieved="$(pepcli --oauth-token-group "Access Administrator" structure-metadata user get userWithMetadataAlternative --key mygroup:mykey)"
  [ "$meta_value_retrieved" = "$meta_value" ] || fail "Expected [$meta_value], got [$meta_value_retrieved]"

  # Clean up what we created in the DB
  pepcli --oauth-token-group "Data Administrator" ama column remove columnWithMetadata
  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove columnGroupWithMetadata
  pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithMetadata
  pepcli --oauth-token-group "Access Administrator" user group remove userGroupWithMetadata
  pepcli --oauth-token-group "Access Administrator" user remove userWithMetadata

fi

####################

if should_run_test structured-output; then
  # Prepare Structured-Output Test Data (SOAG)
  pepcli --oauth-token-group "Access Administrator" user group create SOAG

  # Prepapre Structured-Output Test Data (SOTD)
  pepcli --oauth-token-group "Data Administrator" ama column create SOTD.shortText
  pepcli --oauth-token-group "Data Administrator" ama column create SOTD.longText

  pepcli --oauth-token-group "Data Administrator" ama columnGroup create SOTD
  pepcli --oauth-token-group "Data Administrator" ama column addTo SOTD.shortText SOTD
  pepcli --oauth-token-group "Data Administrator" ama column addTo SOTD.longText SOTD

  pepcli --oauth-token-group "Access Administrator" ama cgar create SOTD SOAG read
  pepcli --oauth-token-group "Access Administrator" ama cgar create SOTD SOAG write

  # Prerare Structured-Output Test Participants (SOTP)
  SOTP1=$(pepcli --oauth-token-group SOAG register id | grep "identifier:" | cut -d':' -f2 | xargs)
  SOTP2=$(pepcli --oauth-token-group SOAG register id | grep "identifier:" | cut -d':' -f2 | xargs)
  SOTP3=$(pepcli --oauth-token-group SOAG register id | grep "identifier:" | cut -d':' -f2 | xargs)
  SOTP4=$(pepcli --oauth-token-group SOAG register id | grep "identifier:" | cut -d':' -f2 | xargs)

  pepcli --oauth-token-group "Data Administrator" ama group create SOTPGroup
  pepcli --oauth-token-group "Data Administrator" ama group addTo SOTPGroup "${SOTP1}"
  pepcli --oauth-token-group "Data Administrator" ama group addTo SOTPGroup "${SOTP2}"
  pepcli --oauth-token-group "Data Administrator" ama group addTo SOTPGroup "${SOTP3}"
  pepcli --oauth-token-group "Data Administrator" ama group addTo SOTPGroup "${SOTP4}"
  pepcli --oauth-token-group "Access Administrator" ama pgar create SOTPGroup SOAG "enumerate"
  pepcli --oauth-token-group "Access Administrator" ama pgar create SOTPGroup SOAG "access"

  pepcli --oauth-token-group SOAG store -p "${SOTP1}" -c SOTD.shortText -d "yellow" # exactly 7 bytes
  pepcli --oauth-token-group SOAG store -p "${SOTP1}" -c SOTD.longText -d "Canary Yellow"
  pepcli --oauth-token-group SOAG store -p "${SOTP2}" -c SOTD.shortText -d "blue"
  pepcli --oauth-token-group SOAG store -p "${SOTP2}" -c SOTD.longText -d "Celestial Blue"
  pepcli --oauth-token-group SOAG store -p "${SOTP3}" -c SOTD.shortText -d "green"
  # skipping SOTP3 longText
  pepcli --oauth-token-group SOAG store -p "${SOTP4}" -c SOTD.longText -d "Cadmium Red"
  # skipping SOTP4 shortText

  # Pull and export to csv
  CSV_PATH="$DEST_DIR/pulled-data/export.csv"
  pepcli --oauth-token-group SOAG pull\
    -P SOTPGroup -C SOTD --output-directory "$DEST_DIR/pulled-data" --force
  pepcli --oauth-token-group SOAG export\
    --from "$DEST_DIR/pulled-data" --max-inline-size 7 --output-file "$CSV_PATH" --force csv

  # Checking csv contents. The order of the rows and fields is not checked: we assume it to be consistent
  CSV_CONTENT=$(execute . cat "${CSV_PATH}")
  echo "$CSV_CONTENT" | grep -E '^"id"'               | grep '"SOTD.shortText"' | grep '"SOTD.longText (file ref)"'
  echo "$CSV_CONTENT" | grep -E '^"GUMU[[:alnum:]]+"' | grep '"yellow"'         | grep 'SOTD.longText"'
  echo "$CSV_CONTENT" | grep -E '^"GUMU[[:alnum:]]+"' | grep '"blue"'           | grep 'SOTD.longText"'
  echo "$CSV_CONTENT" | grep -E '^"GUMU[[:alnum:]]+"' | grep '"green"'          | grep '""'
  echo "$CSV_CONTENT" | grep -E '^"GUMU[[:alnum:]]+"' | grep '""'               | grep 'SOTD.longText"'

  # Check if the correct delimiter was used via a count
  EXPECTED_CSV_DELIMITER_COUNT=10
  ACTUAL_CSV_DELIMITER_COUNT=$(echo "$CSV_CONTENT" | grep -o ',' | wc -l | xargs)
  if [ "$ACTUAL_CSV_DELIMITER_COUNT" -ne "$EXPECTED_CSV_DELIMITER_COUNT" ]; then
    fail "Expected ${EXPECTED_CSV_DELIMITER_COUNT} commas but counted ${ACTUAL_CSV_DELIMITER_COUNT}"
  fi

  # Run another export command with a different delimiter and do another count
  pepcli --oauth-token-group SOAG export\
    --from "$DEST_DIR/pulled-data" --output-file "$CSV_PATH" --force csv --delimiter semicolon
  CSV_CONTENT=$(execute . cat "${CSV_PATH}")
  ACTUAL_CSV_DELIMITER_COUNT=$(echo "$CSV_CONTENT" | grep -o ';' | wc -l | xargs)
  if [ "$ACTUAL_CSV_DELIMITER_COUNT" -ne "$EXPECTED_CSV_DELIMITER_COUNT" ]; then
    fail "Expected ${EXPECTED_CSV_DELIMITER_COUNT} semicolons but counted ${ACTUAL_CSV_DELIMITER_COUNT}"
  fi

  # Clean up
  execute . rm -rf "$DEST_DIR/pulled-data"

  pepcli --oauth-token-group "Data Administrator" ama column removeFrom SOTD.shortText SOTD
  pepcli --oauth-token-group "Data Administrator" ama column remove SOTD.shortText
  pepcli --oauth-token-group "Data Administrator" ama column removeFrom SOTD.longText SOTD
  pepcli --oauth-token-group "Data Administrator" ama column remove SOTD.longText
  pepcli --oauth-token-group "Access Administrator" ama cgar remove SOTD SOAG read
  pepcli --oauth-token-group "Access Administrator" ama cgar remove SOTD SOAG write
  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove SOTD

  pepcli --oauth-token-group "Data Administrator" ama group removeFrom SOTPGroup "${SOTP1}"
  pepcli --oauth-token-group "Data Administrator" ama group removeFrom SOTPGroup "${SOTP2}"
  pepcli --oauth-token-group "Data Administrator" ama group removeFrom SOTPGroup "${SOTP3}"
  pepcli --oauth-token-group "Data Administrator" ama group removeFrom SOTPGroup "${SOTP4}"
  pepcli --oauth-token-group "Access Administrator" ama pgar remove SOTPGroup SOAG "enumerate"
  pepcli --oauth-token-group "Access Administrator" ama pgar remove SOTPGroup SOAG "access"
  pepcli --oauth-token-group "Data Administrator" ama group remove SOTPGroup

  pepcli --oauth-token-group "Access Administrator" user group remove SOAG
fi

####################

if should_run_test s3-roundtrip; then
  # Set up a column for storage of large(-ish) data by "Research Assessor"
  pepcli --oauth-token-group "Data Administrator" ama column create LargeColumn
  pepcli --oauth-token-group "Data Administrator" ama columnGroup create LargeColumns
  pepcli --oauth-token-group "Data Administrator" ama column addTo LargeColumn LargeColumns
  pepcli --oauth-token-group "Access Administrator" ama cgar create LargeColumns "Research Assessor" read
  pepcli --oauth-token-group "Access Administrator" ama cgar create LargeColumns "Research Assessor" write

  # Store a large (i.e. stored in S3) file with some participants
  readonly LARGE_RANDOM_DATA_FILE="$DEST_DIR/large-random-data.bin"
  # 10 blocks @ 1048576 bytes each = 10MiB
  execute . dd if=/dev/urandom of="$LARGE_RANDOM_DATA_FILE" bs=1048576 count=10
  for i in {1..50}; do
    pepcli --oauth-token-group "Research Assessor" store -p "participant$i" -c LargeColumn -i "$LARGE_RANDOM_DATA_FILE"
  done

  # Download the (large) files that we stored
  pepcli --oauth-token-group "Research Assessor" pull -P \* -c LargeColumn -o "$DEST_DIR/s3-backed-files"
  # We'd like to diff/compare the downloaded files to the original LARGE_RANDOM_DATA_FILE, but
  # that's not easily done because "find" doesn't propagate exit codes and we can't pipe within the
  # container. So we just count the downloaded files instead of (also) inspecting their contents.
  count=$(execute . find "$DEST_DIR/s3-backed-files" -type f -name LargeColumn.bin | wc -l)
  if [ "$count" -ne 50 ]; then
    fail "Expected to download 50 files from S3 but got $count."
  fi

  # Clean up
  execute . rm "$LARGE_RANDOM_DATA_FILE"
  execute . rm -rf "$DEST_DIR/s3-backed-files"
  for i in {1..50}; do
    pepcli --oauth-token-group "Research Assessor" delete -p "participant$i" -c LargeColumn
  done
  pepcli --oauth-token-group "Access Administrator" ama cgar remove LargeColumns "Research Assessor" write
  pepcli --oauth-token-group "Access Administrator" ama cgar remove LargeColumns "Research Assessor" read
  pepcli --oauth-token-group "Data Administrator" ama column removeFrom LargeColumn LargeColumns
  pepcli --oauth-token-group "Data Administrator" ama columnGroup remove LargeColumns
  pepcli --oauth-token-group "Data Administrator" ama column remove LargeColumn
fi

####################

if should_run_test certificate-renewal; then
  signing_servers=(authserver transcryptor storagefacility registrationserver accessmanager)
  # For execute calls, we need paths relative to $DATA_DIR
  # But since pepcli, for local runs, always has $DEST_DIR as working directory, these relative paths don't resolve correctly when passing them to pepcli.
  # so we also make an absolute path that we can pass to pepcli.
  certificate_renewal_data_dir="certificate-renewal"
  execute . mkdir -p "$certificate_renewal_data_dir/old-chains"

  certificate_renewal_data_dir_absolute="$(execute $certificate_renewal_data_dir pwd)"
  ca_config_file_name="$certificate_renewal_data_dir_absolute/ca_ext.cnf"
  trace cat "${CORE_DIR}/pki/ca_ext.cnf" | execute "$certificate_renewal_data_dir" tee "$ca_config_file_name"

  execute "$certificate_renewal_data_dir" openssl rand -base64 -out "fakeCA.password" 32
  MSYS_NO_PATHCONV=1 # Prevent the "/" at the start of the subject to be replaced by a Windows-path in Git Bash
  execute "$certificate_renewal_data_dir" openssl req -x509 -newkey rsa:4096 -keyout fakeCA.key -out fakeCA.cert -passin file:fakeCA.password \
    -sha256 -days 3650 -nodes -subj "/C=NL/ST=Gelderland/L=Nijmegen/O=Radboud Universiteit/OU=PEP Intermediate PEP Server CA/CN=PEP Intermediate PEP Server CA"
   MSYS_NO_PATHCONV=0

  sign_csr() {
    csr_file_name="$1"
    file_name_base="${csr_file_name%.csr}"
    cert_file_name="$file_name_base.cert"
    chain_file_name="$file_name_base.chain"
    common_name=${file_name_base#PEP}
    validity="${2:-}"

    ca_key_file_name="${PKI_DIR}/pepServerCA.key"
    ca_key_cert="${PKI_DIR}/pepServerCA.chain"
    password_file="${PKI_DIR}/pepServerCA.password"
    extensions="pep_cert"

    if [ "$validity" = "invalid" ]; then
      ca_key_file_name="fakeCA.key"
      ca_key_cert="fakeCA.cert"
      password_file="fakeCA.password"
    fi
    if [ "$validity" = "tls" ]; then
      extensions="server_cert"
    fi

    execute "$certificate_renewal_data_dir" openssl x509 -req -sha256 -in "$csr_file_name" -CAkey "$ca_key_file_name" -CA "$ca_key_cert" \
     -out "$cert_file_name" -days 365 -extfile "$ca_config_file_name" -extensions "$extensions"  \
     -CAcreateserial -CAserial ca_serial.srl -passin file:"$password_file"

    execute "$certificate_renewal_data_dir" cat "$cert_file_name" "$ca_key_cert" | execute "$certificate_renewal_data_dir" tee "$chain_file_name" > /dev/null
    execute "$certificate_renewal_data_dir" rm "$cert_file_name"
  }

  sign_csrs() {
    expected_csr_count="$1"
    subdir="${2:-"."}"
    validity="${3:-}"

    csrs="$(execute "$certificate_renewal_data_dir" find "$subdir" -maxdepth 1 -name "*.csr")"
    [ "$(echo "$csrs" | wc -l)" -eq "$expected_csr_count" ] || fail "Expected exactly $expected_csr_count *.csr file(s) in $certificate_renewal_data_dir/$subdir. Found *.csr files: $csrs"
    for csr in $csrs; do
      sign_csr "$csr" "$validity"
    done
  }

  compare_chains() {
    server="$1"
    phase="$2"
    copy_new_to_old="${3:-}"

    chain_file_on_disk=$(execute "$certificate_renewal_data_dir" find "$PKI_DIR" -iname "PEP$server.chain")
    chain_basename=$(basename "$chain_file_on_disk")
    new_chain="./$chain_basename"
    old_chain="./old-chains/$chain_basename"

    pepcli ping --print-certificate-chain --server "$server" | execute "$certificate_renewal_data_dir" tee reported.chain > /dev/null
    if [ "$phase" = prepare ]; then
      execute "$certificate_renewal_data_dir" diff -q reported.chain "$chain_file_on_disk"
    else
      if execute "$certificate_renewal_data_dir" diff -q reported.chain "$new_chain"; then
        trace [ "$phase" = reverted ] && fail "Certificate chain should no longer equal the new chain, after servers have been restarted without committing, for $server"
      else
        trace [ "$phase" = replaced ] || trace [ "$phase" = committed ] && fail "Certificate chain should have been replaced with the new chain for $server"
      fi
      if execute "$certificate_renewal_data_dir" diff -q "$new_chain" "$chain_file_on_disk"; then
        trace [ "$phase" = replaced ] && fail "Certificate chain should have been replaced, but not yet committed to file for $server"
        trace [ "$phase" = reverted ] && fail "Certificate chain should not have been committed to file, when it was restarted before committing, for $server"
      else
        trace [ "$phase" = committed  ] && fail "Certificate chain should have been replaced, as well as committed to file for $server"
      fi
      if execute "$certificate_renewal_data_dir" diff -q "$chain_file_on_disk" "$old_chain"; then
        trace [ "$phase" = committed  ] && fail "Certificate chain on disk should have been changed, after being committed, for $server"
      else
        trace [ "$phase" = replaced  ] && fail "Certificate chain on disk should not have been changed, before being committed, for $server"
        trace [ "$phase" = reverted ] && fail "Certificate chain on disk should not have changed, when it was restarted before committing, for $server"
      fi
    fi

    # During preparation we want to make a copy of the current certificate chain, so we can check later whether the current chain on disk has changed.
    # After committing, we usually want to copy it again, so we can do further testing, and in those tests we again want to check whether the current chain on disk has changed
    # If we don't want that to happen, we can add "no_copy"  to the compare_chains call
    if trace [ "$phase" = "prepare" ] || trace [ "$phase" = "committed" ] && trace [ "$copy_new_to_old" != "no_copy" ]; then
      execute "$certificate_renewal_data_dir" cp "$chain_file_on_disk" "$old_chain"
    fi
  }

  printGreen "Check that all signing servers currently sign their messages with the certificate chain on disk"
  for server in "${signing_servers[@]}"; do
    trace compare_chains "$server" prepare
  done

  printGreen "First, check requesting a CSR, and replacing and committing the new certificate chain for a single server."
  server="${signing_servers[0]}"
  execute . mkdir -p "$certificate_renewal_data_dir/first-run"

  pepcli --oauth-token-group "Data Administrator" server certificate request-csr --server "$server" --output-directory "$certificate_renewal_data_dir_absolute/first-run" \
    && fail "Only System Administrator should be able to request CSRs"
  pepcli --oauth-token-group "System Administrator" server certificate request-csr --server "$server" --output-directory "$certificate_renewal_data_dir_absolute/first-run"
  pepcli --oauth-token-group "System Administrator" server certificate request-csr --server "$server" --output-directory "$certificate_renewal_data_dir_absolute"

  trace sign_csrs 1 first-run
  trace sign_csrs 1 . invalid

  pepcli --oauth-token-group "System Administrator" server certificate replace --server "$server"  --input-directory "$certificate_renewal_data_dir_absolute/first-run" \
    && fail "After running 'request-csr' a second time, it should no longer be possible to use the CSRs from the first run"
  pepcli --oauth-token-group "System Administrator" server certificate replace --server "$server"  --input-directory "$certificate_renewal_data_dir_absolute" \
    && fail "Replacing certificates with an invalid certificate should not be possible"
  trace sign_csrs 1 . tls
  pepcli --oauth-token-group "System Administrator" server certificate replace --server "$server"  --input-directory "$certificate_renewal_data_dir_absolute" \
    && fail "Replacing certificates with a TLS certificate should not be possible"
  trace sign_csrs 1
  pepcli --oauth-token-group "Data Administrator" server certificate replace --server "$server"  --input-directory "$certificate_renewal_data_dir_absolute" \
    && fail "Only System Administrator should be able to replace certificates"
  pepcli --oauth-token-group "System Administrator" server certificate replace --server "$server"  --input-directory "$certificate_renewal_data_dir_absolute"
  trace compare_chains "$server" replaced

  pepcli --oauth-token-group "Data Administrator" server certificate commit --server "$server"  --input-directory "$certificate_renewal_data_dir_absolute" \
    && fail "Only System Administrator should be able to commit certificates"
  pepcli --oauth-token-group "System Administrator" server certificate commit --server "$server"  --input-directory "$certificate_renewal_data_dir_absolute"
  trace compare_chains "$server" committed

  execute "$certificate_renewal_data_dir" find . -maxdepth 1 -name "*.csr" -delete
  execute "$certificate_renewal_data_dir" find . -maxdepth 1 -name "*.chain" -delete

  printGreen "Then check requesting a CSR, and replacing and committing the new certificate chain for all servers at the same time"
  pepcli --oauth-token-group "System Administrator" server certificate request-csr --output-directory "$certificate_renewal_data_dir_absolute"
  sign_csrs "${#signing_servers[@]}"
  pepcli --oauth-token-group "System Administrator" server certificate replace --input-directory "$certificate_renewal_data_dir_absolute"
  for server in "${signing_servers[@]}"; do
    trace compare_chains "$server" replaced
  done
  pepcli --oauth-token-group "System Administrator" server certificate commit  --input-directory "$certificate_renewal_data_dir_absolute"
  for server in "${signing_servers[@]}"; do
    trace compare_chains "$server" committed no_copy # Don't copy the new chain just yet to $old_chain. We first want to check what happens after a restart
  done

  printGreen "Check that the committed certificates are still used after restart"
  restart_servers
  for server in "${signing_servers[@]}"; do
    trace compare_chains "$server" committed # Now the new chain can be copied, so we can do further testing
  done

  printGreen "Check that uncommitted certificates are not still used after restart"
  pepcli --oauth-token-group "System Administrator" server certificate request-csr --output-directory "$certificate_renewal_data_dir_absolute"
  sign_csrs "${#signing_servers[@]}"
  pepcli --oauth-token-group "System Administrator" server certificate replace --input-directory "$certificate_renewal_data_dir_absolute"
  for server in "${signing_servers[@]}"; do
    trace compare_chains "$server" replaced
  done
  restart_servers
  for server in "${signing_servers[@]}"; do
    trace compare_chains "$server" reverted
  done

  execute "$certificate_renewal_data_dir" find . -maxdepth 1 -name "*.csr" -delete
  execute "$certificate_renewal_data_dir" find . -maxdepth 1 -name "*.chain" -delete
fi

####################
