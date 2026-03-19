#!/usr/bin/env bash
set -e

# OAuth token secret file location
TOKEN_SECRET_FILE="/app/authserver/OAuthTokenSecret.json"
PEPCLI_EXECUTABLE="/app/pepcli"

# Helper function to run pepcli with common flags
pepcli_run(){
  local oauth_group="$1"
  shift
  "$PEPCLI_EXECUTABLE" --suppress-version-info --oauth-token-secret "$TOKEN_SECRET_FILE" --oauth-token-group "$oauth_group" "$@" &>/dev/null
}

cleanup_pep_configuration(){
  
  for cg in ParticipantInfo StudyContexts VisitAssessors WatchData Device CastorShortPseudonyms Castor IsTestParticipant Canary; do
    pepcli_run "Data Administrator" ama columnGroup remove --force "$cg" || true
  done

  for col in StudyContexts ParticipantInfo IsTestParticipant Canary; do
    pepcli_run "Data Administrator" ama column remove "$col" || true
  done

  for UserGroup in Monitor PullCastor Watchdog; do
    pepcli_run "Access Administrator" ama pgar remove "*" "$UserGroup" access || true
    pepcli_run "Access Administrator" ama pgar remove "*" "$UserGroup" enumerate || true
  done
  
  pepcli_run "Access Administrator" ama cgar remove ShortPseudonyms Monitor read || true
  pepcli_run "Access Administrator" ama cgar remove ParticipantIdentifier Monitor read || true
  pepcli_run "Access Administrator" ama cgar remove ParticipantIdentifier "Data Administrator" read || true
}

create_participant(){
  NAME="$1"
  ADDRESS="$2"
  CLASS="$3"
  FRENCH_LEVEL="$4"
  FRENCH_GRADE="$5"
  MATHS_LEVEL="$6"
  MATHS_GRADE="$7"
  SEARCH="Generated participant with identifier: "
  echo "Creating Participant: $NAME" >&2
  RESULT=$("$PEPCLI_EXECUTABLE" --suppress-version-info --oauth-token-secret "$TOKEN_SECRET_FILE" --oauth-token-group "Research Assessor" register id 2>/dev/null | grep "$SEARCH")
  ID="${RESULT:${#SEARCH}}"
  pepcli_run "Research Assessor" store -p "$ID" -c Name -d "$NAME" --file-extension .txt
  pepcli_run "Research Assessor" store -p "$ID" -c Address -d "$ADDRESS" --file-extension .txt
  pepcli_run "Research Assessor" store -p "$ID" -c Class -d "$CLASS" --file-extension .txt
  pepcli_run "Research Assessor" store -p "$ID" -c French_Level -d "$FRENCH_LEVEL" --file-extension .txt
  pepcli_run "Research Assessor" store -p "$ID" -c French_Grade -d "$FRENCH_GRADE" --file-extension .txt
  pepcli_run "Research Assessor" store -p "$ID" -c Maths_Level -d "$MATHS_LEVEL" --file-extension .txt
  pepcli_run "Research Assessor" store -p "$ID" -c Maths_Grade -d "$MATHS_GRADE" --file-extension .txt
  echo "Registered Participant $NAME with ID: $ID" >&2
}

echo "Starting PEP Servers in background..."
/app/pepServers /app &>/dev/null &
PEPSERVERS_PID=$!

echo "Waiting 10 seconds for servers to fully initialize..."
sleep 10

# Check if pepServers is still running
if ! kill -0 $PEPSERVERS_PID 2>/dev/null; then
    echo "Error: pepServers failed to start"
    exit 1
fi

echo "Servers ready. Running tutorial setup..."

echo "Clearing PEP Repository..."
cleanup_pep_configuration

echo "Creating User Groups..."
pepcli_run "Access Administrator" user group create "Access Administrator"
pepcli_run "Access Administrator" user group create "Data Administrator"
pepcli_run "Access Administrator" user group create "Research Assessor"

echo "Creating Columns..."
pepcli_run "Data Administrator" ama column create Name
pepcli_run "Data Administrator" ama column create Address
pepcli_run "Data Administrator" ama column create Class
pepcli_run "Data Administrator" ama column create French_Level
pepcli_run "Data Administrator" ama column create French_Grade
pepcli_run "Data Administrator" ama column create Maths_Level
pepcli_run "Data Administrator" ama column create Maths_Grade
pepcli_run "Data Administrator" ama column create Average_Grade
pepcli_run "Data Administrator" ama column create History_Grade

echo "Creating Column Groups..."
pepcli_run "Data Administrator" ama columnGroup create PersonalInfo
pepcli_run "Data Administrator" ama columnGroup create NonPersonalInfo
pepcli_run "Data Administrator" ama columnGroup create History_Grade
pepcli_run "Data Administrator" ama columnGroup create ShortPseudonym.History
pepcli_run "Data Administrator" ama column addTo Name PersonalInfo
pepcli_run "Data Administrator" ama column addTo Address PersonalInfo
pepcli_run "Data Administrator" ama column addTo Class NonPersonalInfo
pepcli_run "Data Administrator" ama column addTo French_Level NonPersonalInfo
pepcli_run "Data Administrator" ama column addTo French_Grade NonPersonalInfo
pepcli_run "Data Administrator" ama column addTo Maths_Level NonPersonalInfo
pepcli_run "Data Administrator" ama column addTo Maths_Grade NonPersonalInfo
pepcli_run "Data Administrator" ama column addTo Average_Grade NonPersonalInfo
pepcli_run "Data Administrator" ama column addTo History_Grade History_Grade
pepcli_run "Data Administrator" ama column addTo ShortPseudonym.History ShortPseudonym.History

echo "Creating Access Rules..."
pepcli_run "Access Administrator" ama cgar create PersonalInfo "Research Assessor" read
pepcli_run "Access Administrator" ama cgar create PersonalInfo "Research Assessor" write
pepcli_run "Access Administrator" ama cgar create NonPersonalInfo "Research Assessor" read
pepcli_run "Access Administrator" ama cgar create NonPersonalInfo "Research Assessor" write
pepcli_run "Access Administrator" ama cgar create NonPersonalInfo "Data Administrator" read

echo "Creating Participants..."
create_participant "Alice"    "Fruitstraat 12" "2A" "A" "8" "A" "3"
create_participant "Danielle" "Appelstraat 7"  "1C" "B" "5" "A" "8"
create_participant "Bob"      "Lange Laan 3"   "3B" "A" "4" "C" "7"
create_participant "Charles"  "Lange Laan 3"   "2A" "C" "7" "D" "6"

rm /data/ClientKeys.json || true
rm /data/OAuthToken.json || true

echo ""
echo "================================================================"
echo "PEP Tutorial Sandbox ready!"
echo "================================================================"
echo "The container is running with pepServers and sample data loaded."
echo ""
echo "To use the tutorial:"
echo "  1. Press Ctrl+C to move on from the setup phase."
echo "  2. Then, to enter the container which includes pepcli, run: docker exec -it pep-sandbox bash"
echo "  3. Inside the container, run: pepLogon"
echo "  4. Then use pepcli commands as shown in the tutorial documentation"
echo ""
echo "To stop the container, run: docker stop pep-sandbox"
echo "================================================================"
echo ""
echo "Container is ready. Press Ctrl+C to move on."

# Keep container running
wait $PEPSERVERS_PID