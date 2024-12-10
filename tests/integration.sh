#!/usr/bin/env sh
# shellcheck disable=SC2039

set -o errexit
set -o nounset

usage() {
  echo "Usage:"
  echo " $0 [OPTIONS] [IMAGE] [CORE_DIR] [GENERATED_DATA_DIR] [BUILD_DIR] [BUILD_MODE] "
  echo
  echo "[IMAGE]              - The pep-services docker image to use. Default: $IMAGE_REPOSITORY:<your-commit-sha> , which currently evaluates to $(default_image)"
  echo "[CORE_DIR]           - The directory containing PEP core. This directory will only be used to read from. Default: One level higher than the path of this script."
  echo "[GENERATED_DATA_DIR] - The directory where all generated files will be stored. Generated files include copied configuration files, pki material, up- and downloaded testfiles, etc." 
  echo "                         This directory is removed and remade at the start of the script. Default: CORE_DIR/temp/integration"
  echo "[BUILD_DIR]          - Only relevant when building locally. The base directory leading to the pep executables. 
                                 this directory should have the /cpp/pep/ sub dirs. Default: CORE_DIR/build"
  echo "[BUILD_MODE]         - Only relevant when building with Visual Studio. Used to indicate sub directories of the build directory, e.g 'Debug' or 'Release'. Default: ."
  echo
  echo "Options are:"
  echo " --local             - Run the tests using your local build. Expects the working directory to be your build directory."
  echo " --no-docker         - Run tests without use of docker. Only possible in local mode."
  echo " -h|--help|-?        - Display this help"
  exit 2
}

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
git_root="$SCRIPTPATH/.."

# import functions
# shellcheck source=/dev/null
. "$SCRIPTPATH/functions.sh"
trap finish EXIT

LOCAL=false
USE_DOCKER=true
while [ ${#} -gt 0 ];
do
  case $1 in
    -h|--help|\?) usage ;;
    --local) LOCAL=true ;;
    --no-docker) USE_DOCKER=false;; 
    -?*) 
      printGreen "WARN: Unknown option: $1"
      usage ;;
    *) break
  esac
  shift
done
readonly LOCAL
readonly USE_DOCKER
if [ "$USE_DOCKER" = false ] && [ "$LOCAL" = false ]; then
  printGreen "It is not possible to disable docker in a non-local build"
  usage
fi

readonly IMAGE="${1:-"$(default_image "$git_root")"}"
echo "IMAGE: $IMAGE"

# Set the base dirs

TESTS_DIR="$(make_absolute "$(dirname "$0")")"
readonly TESTS_DIR
readonly CORE_DIR="${2:-"$(make_absolute "$TESTS_DIR/..")"}"
echo "CORE_DIR: $CORE_DIR"

GENERATED_DATA_DIR="${3:-"$CORE_DIR"/temp/integration}"

rm -rf "$GENERATED_DATA_DIR" || (printYellow "Attempting to delete files that were created by Docker, using elevated credentials. Please remove $GENERATED_DATA_DIR manually using sudo." && exit 1)
mkdir -p "$GENERATED_DATA_DIR"
GENERATED_DATA_DIR="$(make_absolute "$GENERATED_DATA_DIR")"
readonly GENERATED_DATA_DIR
echo "GENERATED_DATA_DIR: $GENERATED_DATA_DIR"

readonly DATA_DIR="$GENERATED_DATA_DIR/data" 
mkdir -p "$DATA_DIR"
readonly PKI_DIR="$GENERATED_DATA_DIR/pki"
mkdir -p "$PKI_DIR"
readonly S3PROXY_RUNTIME_DIR="$GENERATED_DATA_DIR/s3proxy-runtime"
mkdir -p "$S3PROXY_RUNTIME_DIR"

if [ "$LOCAL" = true ]; then
  BUILD_DIR="$(make_absolute "${4:-$CORE_DIR/build}")"
  readonly BUILD_DIR
  echo "BUILD_DIR: $BUILD_DIR"
  curlCmd() { curl "$@"; }
  AUTHSERVER=localhost
else
  readonly BUILD_DIR="builddir" # Must be set to something, actual value is not relevant for non-local use
  curlCmd() { docker run --rm -v "$(pwd):/data" -w /data --net pep-network curlimages/curl "$@"; }
  AUTHSERVER=pepservertest
fi

readonly BUILD_MODE="${5:-.}"
echo "BUILD_MODE: $BUILD_MODE"

# Settings for pepStorageFacilityUnitTests
export PEP_ROOT_CA=../pki/rootCA.cert
export PEP_S3_ACCESS_KEY="MyAccessKey"
export PEP_S3_SECRET_KEY="MySecret"
export PEP_S3_EXPECT_COMMON_NAME="S3"
export PEP_S3_TEST_BUCKET=TestBucket1
export PEP_S3_TEST_BUCKET2=TestBucket2
export PEP_USE_CURRENT_PATH="1"

printGreen "########################################################## Configuration stage #########################################################"
trace cp -r "$CORE_DIR"/config/integration/* "$DATA_DIR"
trace cp -r "$CORE_DIR"/config/OAuthTokenSecret.json "$DATA_DIR/keyserver/"
trace cp "$CORE_DIR/config/projects/gum/accessmanager/GlobalConfiguration.json" "$DATA_DIR/accessmanager/"
trace cp "$DATA_DIR/keyserver/OAuthTokenSecret.json" "$DATA_DIR/authserver/OAuthTokenSecret.json"

if [ "$LOCAL" = true ]; then
   
  # We need a slightly modified StorageFacility.json:
  trace cd "$DATA_DIR/storagefacility"
  trace rm "StorageFacility.json"
  if [ "$USE_DOCKER" = true ]; then
    trace mv "StorageFacility.local.json" "StorageFacility.json"
  else
    trace mv "StorageFacility.local.no.docker.json" "StorageFacility.json"
  fi

  trace cd "$PKI_DIR"
  trace "$CORE_DIR/pki/pki.sh"
  trace cd "$DATA_DIR"
  trace "$CORE_DIR/docker/init_keys.sh" "$DATA_DIR" "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE"
  trace "$CORE_DIR/docker/config_servers.sh" \
    "$DATA_DIR" \
    "$PKI_DIR" \
    "$BUILD_DIR/cpp/pep/storagefacility/$BUILD_MODE/pepStorageFacility" \
    "$BUILD_DIR/cpp/pep/keyserver/$BUILD_MODE/pepKeyServer" \
    "$BUILD_DIR/cpp/pep/accessmanager/$BUILD_MODE/pepAccessManager" \
    "$BUILD_DIR/cpp/pep/transcryptor/$BUILD_MODE/pepTranscryptor" \
    "$BUILD_DIR/cpp/pep/registrationserver/$BUILD_MODE/pepRegistrationServer" \
    "$BUILD_DIR/cpp/pep/authserver/$BUILD_MODE/pepAuthserver" \
    "$BUILD_DIR/cpp/pep/cli/$BUILD_MODE/pepcli" \
    "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepEnrollment"
  printGreen "\$ $BUILD_DIR/cpp/pep/servers/pepServers"
  "$BUILD_DIR/cpp/pep/servers/$BUILD_MODE/pepServers" > "$DATA_DIR/pepServers.log" 2>&1 &
  export pepServersPid=$!
else
  trace docker network create pep-network
  trace docker run --rm --net pep-network -v "$PKI_DIR:/pki" -w=/pki "$IMAGE" bash /app/pki.sh all /app/ca_ext.cnf
  trace docker run --rm --net pep-network -v "$DATA_DIR:/data" "$IMAGE" bash /app/init_keys.sh
  trace docker run --rm --net pep-network -v "$DATA_DIR:/data" -v "$PKI_DIR:/pki:ro" "$IMAGE" bash /app/config_servers.sh
  trace docker run --net pep-network -v "$DATA_DIR:/data" -v "$PKI_DIR:/pki:ro" -v "$TESTS_DIR/test_input:/test_input"  --name pepservertest -d "$IMAGE"
fi


if [ "$USE_DOCKER" = true ]; then
 trace "$CORE_DIR/s3proxy/s3proxy.sh" stage "$S3PROXY_RUNTIME_DIR" "$PKI_DIR/s3certs"
 # Create test buckets for pepStorageFacilityUnitTests, preventing 'NoSuchBucket' error code from s3proxy.
 # Specifying "--parents" to prevent failure when directories already exist, e.g. if
 # this script is run multiple times.
 mkdir -p "$S3PROXY_RUNTIME_DIR/data/$PEP_S3_TEST_BUCKET"
 mkdir -p "$S3PROXY_RUNTIME_DIR/data/$PEP_S3_TEST_BUCKET2"
 printGreen "\$ $S3PROXY_RUNTIME_DIR/s3proxy.sh pull"
 "$S3PROXY_RUNTIME_DIR/s3proxy.sh" pull
 
 printGreen "\$ $S3PROXY_RUNTIME_DIR/s3proxy.sh start"
 "$S3PROXY_RUNTIME_DIR/s3proxy.sh" start
 trace sleep 10
fi
execute client "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepEnrollment" ClientConfig.json 1 "ewogICAgInN1YiI6ICJhc3Nlc3NvciIsCiAgICAiZ3JvdXAiOiAiUmVzZWFyY2ggQXNzZXNzb3IiLAogICAgImlhdCI6ICIxNTQyODk1ODg3IiwKICAgICJleHAiOiAiMjA3MzY1NDEyMiIKfQo.cNoT3VMtEZkrHGLOayqj3gwaM7R2BYv24FJpshecK4s" ClientKeys.json
execute client cat ClientKeys.json

printGreen "########################################################## Test stage #########################################################"


if [ "$USE_DOCKER" = true ]; then
  TEST_FILTERS=""
else
  TEST_FILTERS="--gtest_filter=-S3Client.putObject:PageStore.basic"
fi
# Note that line below invokes pepStorageFacilityUnitTests (and sets the DOCKER_EXEC_ARGS variable only for that invocation).
DOCKER_EXEC_ARGS="-e PEP_ROOT_CA -e PEP_S3_ACCESS_KEY -e PEP_S3_SECRET_KEY -e PEP_USE_CURRENT_PATH -e PEP_S3_HOST=s3proxyproxy -e PEP_S3_EXPECT_COMMON_NAME -e PEP_S3_TEST_BUCKET -e PEP_S3_TEST_BUCKET2"  execute . "$BUILD_DIR/cpp/pep/storagefacility/$BUILD_MODE/pepStorageFacilityUnitTests" "$TEST_FILTERS"


execute watchdog "$BUILD_DIR/go/src/pep.cs.ru.nl/pep-watchdog/pep-watchdog" -oneshot -instant-stressor
execute client "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepClientTest" ClientConfig.json 1 POM-1234
execute client "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepClientTest" ClientConfig.json 2 POM-1234

if [ "$LOCAL" = true ]; then
  PEPCLI_COMMAND="$BUILD_DIR/cpp/pep/cli/$BUILD_MODE/pepcli"
  TEST_INPUT_DIR="$TESTS_DIR/test_input"
  CONFIG_DIR="$GENERATED_DATA_DIR/data"
else
  PEPCLI_COMMAND="/app/pepcli"
  TEST_INPUT_DIR="/test_input"
  CONFIG_DIR="/data"
fi

bash "$TESTS_DIR/pepcli_tests.sh" "$PEPCLI_COMMAND" "$CONFIG_DIR" "$TEST_INPUT_DIR" "$LOCAL"

INTEGRATION_USER_PRIMARY_UID="TRnQNJSLx5RFD8VxfzD2HfTsEZ9cT4UsilWw8aiB1ZY"
DIFFICULT_USER_PRIMARY_UID="MWE8U4BPnAJr27HjAqWD8DucHkFUTgDxLa4zSw7R9Bg"
SPOOF_KEY="$(cat "$DATA_DIR"/authserver/spoofKey)"
trace curlCmd -i -H "PEP-Primary-Uid: $INTEGRATION_USER_PRIMARY_UID" -H "PEP-Human-Readable-Uid: integrationUser@example.com" -H "PEP-Alternative-Uids;" -H "PEP-Spoof-Check: $SPOOF_KEY" \
  "http://$AUTHSERVER:8080/auth?client_id=123&code_challenge=NCXJvk7daJeLDY8xw3KxsX8oRaLcXR-p7Tvzt9yjE80&code_challenge_method=S256&redirect_uri=http://localhost:16515/&response_type=code&primary_uid=$INTEGRATION_USER_PRIMARY_UID&human_readable_uid=integrationUser%40example.com&alternative_uids=" -o authserverResponse.txt
# We expect an error when user 'integrationUser' logs in with his primary UID.
# The user 'integrationUser' is currently only known by that UID. So not by e.g. his e-mailaddress, nor by his non-human-readable primary UID.
trace grep "Location: http://localhost:16515.*[\?&]error=access_denied" authserverResponse.txt
trace curlCmd -i -H "PEP-Primary-Uid: $INTEGRATION_USER_PRIMARY_UID" -H "PEP-Human-Readable-Uid: integrationUser@example.com" -H "PEP-Alternative-Uids:integrationUser,integration_user" -H "PEP-Spoof-Check: $SPOOF_KEY" \
  "http://$AUTHSERVER:8080/auth?client_id=123&code_challenge=NCXJvk7daJeLDY8xw3KxsX8oRaLcXR-p7Tvzt9yjE80&code_challenge_method=S256&redirect_uri=http://localhost:16515/&response_type=code&primary_uid=$INTEGRATION_USER_PRIMARY_UID&human_readable_uid=integrationUser%40example.com&alternative_uids=integrationUser%2Cintegration_user" -o authserverResponse.txt
# When he logs in with his known UID 'integrationUser' (in this case specified as an alternative UID), this succeeds. Furthermore, his primary UID is now added to the database.
trace grep "Location: http://localhost:16515.*[\?&]code=" authserverResponse.txt
trace curlCmd -i -H "PEP-Primary-Uid: $INTEGRATION_USER_PRIMARY_UID" -H "PEP-Human-Readable-Uid: integrationUser@example.com" -H "PEP-Alternative-Uids;" -H "PEP-Spoof-Check: $SPOOF_KEY" \
  "http://$AUTHSERVER:8080/auth?client_id=123&code_challenge=NCXJvk7daJeLDY8xw3KxsX8oRaLcXR-p7Tvzt9yjE80&code_challenge_method=S256&redirect_uri=http://localhost:16515/&response_type=code&primary_uid=$INTEGRATION_USER_PRIMARY_UID&human_readable_uid=integrationUser%40example.com&alternative_uids=" -o authserverResponse.txt
# so this time login should succeed, since the primary UID is now known to the system.
trace grep "Location: http://localhost:16515.*[\?&]code=" authserverResponse.txt
trace curlCmd -i -H "PEP-Primary-Uid: eve" -H "PEP-Human-Readable-Uid: eve" -H "PEP-Alternative-Uids;" -H "PEP-Spoof-Check: $SPOOF_KEY" \
  "http://$AUTHSERVER:8080/auth?client_id=123&code_challenge=NCXJvk7daJeLDY8xw3KxsX8oRaLcXR-p7Tvzt9yjE80&code_challenge_method=S256&redirect_uri=http://localhost:16515/&response_type=code&primary_uid=eve&human_readable_uid=eve&alternative_uids=" -o authserverResponse.txt
trace grep "Location: http://localhost:16515.*[\?&]error=access_denied" authserverResponse.txt

# Test if alternative UIDs with comma's are correctly split and decoded
trace curlCmd -i -H "PEP-Primary-Uid: $DIFFICULT_USER_PRIMARY_UID" -H "PEP-Human-Readable-Uid: difficultuser@example.com" -H "PEP-Alternative-Uids:%22something%20with%20comma's%2C%20and%20spaces%22%40example.com,second_alternative" -H "PEP-Spoof-Check: $SPOOF_KEY" \
  "http://$AUTHSERVER:8080/auth?client_id=123&code_challenge=NCXJvk7daJeLDY8xw3KxsX8oRaLcXR-p7Tvzt9yjE80&code_challenge_method=S256&redirect_uri=http://localhost:16515/&response_type=code&primary_uid=$DIFFICULT_USER_PRIMARY_UID&human_readable_uid=difficultuser%40example.com&alternative_uids=%2522something%2520with%2520comma's%252C%2520and%2520spaces%2522%2540example.com%2Csecond_alternative" -o authserverResponse.txt
trace grep "Location: http://localhost:16515.*[\?&]code=" authserverResponse.txt

# Test if alternative UIDs with a plus are correctly URL-decoded
trace curlCmd -i -H "PEP-Primary-Uid: $DIFFICULT_USER_PRIMARY_UID" -H "PEP-Human-Readable-Uid: difficultuser@example.com" -H "PEP-Alternative-Uids:difficultuser%2Bpep%40example.com" -H "PEP-Spoof-Check: $SPOOF_KEY" \
  "http://$AUTHSERVER:8080/auth?client_id=123&code_challenge=NCXJvk7daJeLDY8xw3KxsX8oRaLcXR-p7Tvzt9yjE80&code_challenge_method=S256&redirect_uri=http://localhost:16515/&response_type=code&primary_uid=$DIFFICULT_USER_PRIMARY_UID&human_readable_uid=difficultuser%40example.com&alternative_uids=difficultuser%252Bpep%2540example.com" -o authserverResponse.txt
trace grep "Location: http://localhost:16515.*[\?&]code=" authserverResponse.txt

execute . "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepDumpShadowAdministration" dump ShadowAdministration.key registrationserver/ShadowShortPseudonyms.sqlite
execute watchdog "$BUILD_DIR/go/src/pep.cs.ru.nl/pep-watchdog/pep-watchdog" -oneshot -instant-stressor
execute . "$BUILD_DIR/cpp/pep/benchmark/$BUILD_MODE/pepbenchmark"
