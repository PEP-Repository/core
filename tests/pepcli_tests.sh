#!/usr/bin/env bash
# shellcheck disable=SC2039,SC2034

# This script is meant to only be run from within integration.sh. 

set -o errexit
set -o nounset
set -o pipefail

# shellcheck source=/dev/null
. "$(dirname "$0")/functions.sh"

readonly PEPCLI_COMMAND="$1"
readonly DATA_DIR="$2"
readonly TEST_INPUT_DIR="$3"
readonly LOCAL="$4"
readonly DEST_DIR="$DATA_DIR/test_output"
mkdir -p "$DEST_DIR"

readonly ACCESS_ADMINISTRATOR_TOKEN="ewogICAgInN1YiI6ICJBY2Nlc3MgQWRtaW5pc3RyYXRvciIsCiAgICAiZ3JvdXAiOiAiQWNjZXNzIEFkbWluaXN0cmF0b3IiLAogICAgImlhdCI6ICIxNTcyMzU0MjI4IiwKICAgICJleHAiOiAiMjA3MzY1NDEyMiIKfQo.DYnQyvtpvj2OozTnC6MUMJKW7G-ckzber0q0kRnjwHQ"

# MacOS doesn't support the date command with nanosecond precision, so we need to use GNU coreutils gdate instead.
if [[ "$(uname)" == "Darwin" ]]; then
    # Ensure GNU date is installed
    if ! command -v gdate > /dev/null; then
        echo "gdate could not be found, please install coreutils using Homebrew."
        exit 1
    fi
    DATE_CMD="gdate"
else
    DATE_CMD="date"
fi

#TESTS
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa user create integrationUser
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa group create integrationGroup
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa user addTo integrationUser integrationGroup

pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa user create difficultUser
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa user addIdentifier difficultUser "\"something with comma's, and spaces\"@example.com" # comma's and spaces are allowed if the part before the @ is between quotes.
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa user addIdentifier difficultUser "difficultuser+pep@example.com"
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa user addTo difficultUser integrationGroup

pepcli --oauth-token-group "Research Assessor" pull -P '*' -C ShortPseudonyms -c ParticipantIdentifier -c DeviceHistory -c ParticipantInfo
pepcli list -l -P '*' -c ParticipantIdentifier -c DeviceHistory -c ParticipantInfo

# Store data in a not yet existing row
pepcli store -p test-participant -c DeviceHistory -d random-data

# Ensure that MRI column exists and is read+writable for "Research Assessor" (who will upload and download)
pepcli --oauth-token-group "Data Administrator" ama column create Visit1.MRI.Func
pepcli --oauth-token-group "Data Administrator" ama columnGroup create MRI
pepcli --oauth-token-group "Data Administrator" ama column addTo Visit1.MRI.Func MRI
pepcli --oauth-token-group "Access Administrator" ama cgar create MRI "Research Assessor" read
pepcli --oauth-token-group "Access Administrator" ama cgar create MRI "Research Assessor" write

# Ensure we have an SP value in the database that can be pseudonymized in the upload
pepcli --oauth-token-group "RegistrationServer" store -p test-participant -c ShortPseudonym.Visit1.FMRI -d GUM123456751

pepcli pull --all-accessible --output-directory "$DEST_DIR"/pulled_all_accessible

FOUND_FILES_COUNT=$(execute . find "$DEST_DIR"/pulled_all_accessible -type f | wc -l)
EXPECTED_COUNT=3
if [ "$FOUND_FILES_COUNT" -ne "$EXPECTED_COUNT" ]; then 
    >&2 echo "An unexpected amount of files was found in 'pulled_all_accessible': Expected $EXPECTED_COUNT, found $FOUND_FILES_COUNT" 
    >&2 execute . find "$DEST_DIR"/pulled_all_accessible -type f 
    exit 1
fi

SYMLINK_TEST_DATA="$TEST_INPUT_DIR/pepcli/store/test_symlink_folder_structure"
# Store a directory structure with symlinks with the -resolve-symlinks flag
pepcli --oauth-token-group "Research Assessor" store -p test-participant -c Visit1.MRI.Func -i "$SYMLINK_TEST_DATA" --resolve-symlinks

printYellow "The next command produces an error. This is expected and correct behaviour."
# Attempt to store a directory structure with symlinks withOUT the -resolve-symlinks flag. This should fail and PROCESS_FAILED_AS_INTENDED should be set to true.
pepcli --oauth-token-group "Research Assessor" store -p test-participant -c Visit1.MRI.Func -i "$SYMLINK_TEST_DATA" && FAILING_STORE_EXITCODE=$? || FAILING_STORE_EXITCODE=$?

# If failedAsIntented is not true, fail the test and exit.
if [ "$FAILING_STORE_EXITCODE" -eq 0 ]; then
    printYellow "Storing a directory structure with symlinks withOUT the -resolve-symlinks flag unexpectedly succeeded." 
    exit 1
fi    

# Store something so large that it will be sent to the page store, i.e. larger than INLINE_PAGE_THRESHOLD ( = 4*1024 bytes ).
readonly RANDOM_DATA_FILE="$DEST_DIR/random-data.bin"
execute . dd if=/dev/urandom of="$RANDOM_DATA_FILE" bs=1024 count=6
pepcli store -p test-participant -c DeviceHistory -i "$RANDOM_DATA_FILE"
# Download the data we just stored and compare it to the original data.
pepcli pull --output-directory "$DEST_DIR/pulled" -p test-participant -c DeviceHistory
# We use "find" to locate the downloaded file because it's in a subdirectory named after the participant's local pseudonym, which we don't know.
# find always exits with 0 exit code, but it only prints the found path(s) if the -exec part succeeds. Therefore we can use grep to check 
# whether it has found the DeviceHistory directory, and the diff succeeded.
execute . find "$DEST_DIR/pulled" -name DeviceHistory.bin -exec diff "$RANDOM_DATA_FILE" {} -q \; -print | grep DeviceHistory

# Create column and column group for file extension tests
pepcli --oauth-token-group "Data Administrator" ama column create FileExtensionTest
pepcli --oauth-token-group "Data Administrator" ama columnGroup create FileExtensionTestGroup
pepcli --oauth-token-group "Data Administrator" ama column addTo FileExtensionTest FileExtensionTestGroup
pepcli --oauth-token-group "Access Administrator" ama cgar create FileExtensionTestGroup "Research Assessor" read
pepcli --oauth-token-group "Access Administrator" ama cgar create FileExtensionTestGroup "Research Assessor" write

# create a file with an extension and store it in the column
pepcli --oauth-token-group "Research Assessor" store -p test-participant -c FileExtensionTest -i "$TEST_INPUT_DIR"/fileExtensionData1.txt
# data is pulled and then it is checked whether the data that is stored is the same as the data that we put in
pepcli --oauth-token-group "Research Assessor" pull -p test-participant -c FileExtensionTest --output-directory "$DEST_DIR/pulled-data" --force
execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt -exec diff "$TEST_INPUT_DIR"/fileExtensionData1.txt {} -q \; -print 

# an update is done, this should be a no opt
pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update

# new data is stored, updated and then checked whether this update has worked i.e. that the new data is being retrieved 
pepcli --oauth-token-group "Research Assessor" store -p test-participant -c FileExtensionTest -i "$TEST_INPUT_DIR"/fileExtensionData2.txt
pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update
execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt -exec diff "$TEST_INPUT_DIR"/fileExtensionData2.txt {} -q \; -print 

# the pulled data is changed (made dirty). A new update now should fail. 
DIRTY_FILE="$(execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt)"
echo "$DIRTY_FILE"
execute . bash -c "echo 'A full commitment is what I am thinking of' > '$DIRTY_FILE'"
printYellow "The next command produces an error. This is expected and correct behaviour."
pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update && FAILING_UPDATE_EXITCODE=$? || FAILING_UPDATE_EXITCODE=$?
if [ "$FAILING_UPDATE_EXITCODE" -eq 0 ]; then
    >&2 echo "Updating dirty pulled data should fail."
    exit 1
fi  

# Dirty pulled-data updated with --force switch.
pepcli --oauth-token-group "Research Assessor" store -p test-participant -c FileExtensionTest -i "$TEST_INPUT_DIR"/fileExtensionData3.txt
pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update --force
execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt -exec diff "$TEST_INPUT_DIR"/fileExtensionData3.txt {} -q \; -print 

# Data stored with --file-extension parameter
pepcli --oauth-token-group "Research Assessor" store -p test-participant -c FileExtensionTest -i "$TEST_INPUT_DIR"/fileExtensionData3.txt --file-extension ".pdf"
pepcli --oauth-token-group "Research Assessor" pull --output-directory "$DEST_DIR/pulled-data" --update 
execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.pdf -exec diff "$TEST_INPUT_DIR"/fileExtensionData3.txt {} -q \; -print
if [ "$(execute . find "$DEST_DIR/pulled-data" -name FileExtensionTest.txt)" ]; then
    >&2 echo "Old .txt file should be deleted." 
    exit 1
fi 

# Deleting a nonexistent (empty) cell should fail: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2367
pepcli --oauth-token-group "Research Assessor" delete -p test-participant -c StudyContexts && DELETE_EMPTY_EXITCODE=$? || DELETE_EMPTY_EXITCODE=$?
if [ "$DELETE_EMPTY_EXITCODE" -eq 0 ]; then
    >&2 echo "Deleting a nonexistent (empty) cell should fail."
    exit 1
fi  

# Store empty file and download it again: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2337
pepcli --oauth-token-group "Research Assessor" store -p test-participant -c StudyContexts -i "$TEST_INPUT_DIR"/pepcli/store/EmptyStudyContexts.csv
pepcli --oauth-token-group "Research Assessor" pull  -p test-participant -c StudyContexts -o "$DEST_DIR"/pulled-empty-file

# Delete the (empty) file that we previously stored
pepcli --oauth-token-group "Research Assessor" delete -p test-participant -c StudyContexts
# Trying to delete it again should fail: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2367
pepcli --oauth-token-group "Research Assessor" delete -p test-participant -c StudyContexts && SECOND_DELETE_EXITCODE=$? || SECOND_DELETE_EXITCODE=$?
if [ "$SECOND_DELETE_EXITCODE" -eq 0 ]; then
    >&2 echo "Deleting a cell a second time should fail."
    exit 1
fi  

# Create a user group, remove it later and then verify that we can query the group that was removed through the --at option
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa group create onceUponATimeGroup
UTC_TIMESTAMP=$($DATE_CMD +%s%N | cut -b1-13)
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa group remove onceUponATimeGroup
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa query --at "$UTC_TIMESTAMP" | grep 'onceUponATimeGroup'

# Create a column, remove it later and then verify that we can still query the column that was removed through the --at option
pepcli --oauth-token-group "Data Administrator" ama column create onceUponATimeColumn
UTC_TIMESTAMP=$($DATE_CMD +%s%N | cut -b1-13)
pepcli --oauth-token-group "Data Administrator" ama column remove onceUponATimeColumn
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" ama query --at "$UTC_TIMESTAMP" | grep 'onceUponATimeColumn'


# Create a column group, add columns to it and try to remove it.
pepcli --oauth-token-group "Data Administrator" ama column create blockingColumn
pepcli --oauth-token-group "Data Administrator" ama columnGroup create columnGroupWithColumn
pepcli --oauth-token-group "Data Administrator" ama column addTo blockingColumn columnGroupWithColumn

pepcli --oauth-token-group "Data Administrator" ama columnGroup remove columnGroupWithColumn && REMOVE_COLUMNGROUP_WITH_COLUMN_EXITCODE=$? || REMOVE_COLUMNGROUP_WITH_COLUMN_EXITCODE=$?
if [ "$REMOVE_COLUMNGROUP_WITH_COLUMN_EXITCODE" -eq 0 ]; then
    >&2 echo "removing a columngroup with associated columns should fail."
    exit 1
fi  
# Adding the --force flag should make it succeed
pepcli --oauth-token-group "Data Administrator" ama columnGroup remove columnGroupWithColumn --force

# Create a column group, add a access rule and try to remove it.
pepcli --oauth-token-group "Data Administrator" ama columnGroup create columnGroupWithAccessRule
pepcli --oauth-token-group "Access Administrator" ama cgar create columnGroupWithAccessRule "SomeUserGroup" "read"
pepcli --oauth-token-group "Access Administrator" ama cgar create columnGroupWithAccessRule "SomeUserGroup" "write"


pepcli --oauth-token-group "Data Administrator" ama columnGroup remove columnGroupWithAccessRule && REMOVE_COLUMNGROUP_WITH_ACCESSRULE_EXITCODE=$? || REMOVE_COLUMNGROUP_WITH_ACCESSRULE_EXITCODE=$?
if [ "$REMOVE_COLUMNGROUP_WITH_ACCESSRULE_EXITCODE" -eq 0 ]; then
    >&2 echo "removing a columngroup with associated access rules should fail."
    exit 1
fi  
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
pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithParticipant && REMOVE_PARTICIPANTGROUP_WITH_PARTICIPANT_EXITCODE=$? || REMOVE_PARTICIPANTGROUP_WITH_PARTICIPANT_EXITCODE=$?
if [ "$REMOVE_PARTICIPANTGROUP_WITH_PARTICIPANT_EXITCODE" -eq 0 ]; then
    >&2 echo "removing a participantgroup with associated participants should fail."
    exit 1
fi 
# Adding the --force flag should make it succeed
pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithParticipant --force


# Create a participantgroup and add a access rule
pepcli --oauth-token-group "Data Administrator" ama group create participantGroupWithAccessRule
pepcli --oauth-token-group "Access Administrator" ama pgar create participantGroupWithAccessRule "SomeUserGroup" "enumerate"
pepcli --oauth-token-group "Access Administrator" ama pgar create participantGroupWithAccessRule "SomeUserGroup" "access"

# Attempt to remove the participant group
pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithAccessRule && REMOVE_COLUMNGROUP_WITH_ACCESSRULE_EXITCODE=$? || REMOVE_COLUMNGROUP_WITH_ACCESSRULE_EXITCODE=$?
if [ "$REMOVE_COLUMNGROUP_WITH_ACCESSRULE_EXITCODE" -eq 0 ]; then
    >&2 echo "removing a participantgroup with associated participants should fail."
    exit 1
fi 
# Adding the --force flag should make it succeed
pepcli --oauth-token-group "Data Administrator" ama group remove participantGroupWithAccessRule --force


# Attempt to print the blocklist as an access administrator
pepcli --oauth-token-group "Access Administrator" asa token block list

# Attempt to print the blocklist as a user that is not an access administrator
pepcli --oauth-token-group "Data Administrator" asa token block list && EXIT_CODE=$? || EXIT_CODE=$?
if [ "$EXIT_CODE" -eq 0 ]; then
  >&2 echo "Users that are not access administrators should not have access to the blocklist"
  exit 1
fi

# Add a new user to integrationGroup and generate token for that user 
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa user create userWithFreshToken
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa user addTo userWithFreshToken integrationGroup
TOKEN_TEST_USER_TOKEN=$(pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa token request userWithFreshToken integrationGroup "$($DATE_CMD -d '2 days' +%s)")

# Ensure that integrationGroup has read access to at least one column
pepcli --oauth-token-group "Access Administrator" ama cgar create MRI integrationGroup read

# Attempt to do a query with the generated token
pepcli --oauth-token "$TOKEN_TEST_USER_TOKEN" query column-access

# Block the token just generated 
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa token block create userWithFreshToken integrationGroup -m "Blocked as part of test"

# Attempt to redo the query with the token that is now blocked
pepcli --oauth-token "$TOKEN_TEST_USER_TOKEN" query column-access && EXIT_CODE=$? || EXIT_CODE=$?
if [ "$EXIT_CODE" -eq 0 ]; then
  >&2 echo "query with a blocked token should fail."
  exit 1
fi

# Unblock the token
BLOCKLIST_ENTRY_ID=$(pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa token block list | grep userWithFreshToken | awk -F',' '{print $1}')
pepcli --oauth-token "$ACCESS_ADMINISTRATOR_TOKEN" asa token block remove "$BLOCKLIST_ENTRY_ID"

# Attempt to redo the query with the token that is no longer blocked
pepcli --oauth-token "$TOKEN_TEST_USER_TOKEN" query column-access

# Querying column-access when no access is granted should not fail.
pepcli --oauth-token-group "Access Administrator" asa group create groupWithoutAccess
pepcli --oauth-token-group "groupWithoutAccess" query column-access
