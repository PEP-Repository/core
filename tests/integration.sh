#!/usr/bin/env bash

# You can add BP=1 in front of any 'execute' or 'trace' line to add a breakpoint

set -o errexit
set -o nounset
set -o pipefail

SCRIPTSELF=$(command -v "$0")
SCRIPTPATH="$( cd "$(dirname "$SCRIPTSELF")" || exit ; pwd -P )"
git_root="$SCRIPTPATH/.."

# import functions
# shellcheck source=/dev/null
. "$SCRIPTPATH/functions.bash"

readonly default_skip=''

usage() {
  echo "Usage:"
  echo " $0 [OPTIONS]"
  echo
  echo "Options are:"
  echo " --image <IMAGE>              - The pep-services docker image to use. Default: $IMAGE_REPOSITORY:<your-commit-sha> , which currently evaluates to $(default_image "$git_root")"
  echo " --core-dir <PATH>            - The directory containing PEP core. This directory will only be used to read from. Default: One level higher than the path of this script."
  echo " --generated-data-dir <PATH>  - The directory where all generated files will be stored. Generated files include copied configuration files, pki material, up- and downloaded testfiles, etc."
  echo "                                This directory is removed and remade at the start of the script. Default: CORE_DIR/temp/integration"
  echo " --build-dir <PATH>           - Only relevant when building locally. The base directory leading to the pep executables."
  echo "                                this directory should have the /cpp/pep/ sub dirs. Default: CORE_DIR/build"
  echo " --build-mode <MODE>          - Only relevant when building with Visual Studio. Used to indicate sub directories of the build directory, e.g 'Debug' or 'Release'. Default: ."
  echo " --tests-to-run \"<TEST> [<TEST> [...]]\""
  echo "                              - Subset of tests to run, separated by spaces, see integration.sh & pepcli_tests.sh. Default all"
  echo " --tests-to-skip \"<TEST> [<TEST> [...]]\""
  echo "                              - Subset of tests to skip, separated by spaces, see --tests-to-run. Default \"$default_skip\" unless included in --tests-to-run"
  echo " --local                      - Run the tests using your local build. Expects the working directory to be your build directory."
  echo " --no-docker                  - Run tests without using Docker at all (i.e. without s3proxy). Only possible in local mode; some unit tests will be skipped."
  echo " --reuse-secrets-and-data     - Reuse the secrets and data already present in --generated-data-dir. Configuration is still copied over."
  echo " -h|--help|-?                 - Display this help"
  exit 2
}

check_option_has_value() {
  if [ ${#} -eq 1 ]; then
    printGreen "WARN: Missing value for option $1"
    usage
  fi
}

LOCAL=false
USE_DOCKER=true
REUSE_SECRETS_AND_DATA=false
while [ ${#} -gt 0 ];
do
  case $1 in
    --image)
        check_option_has_value "${@}"
        shift
        IMAGE="$1"
      ;;
    --core-dir)
        check_option_has_value "${@}"
        shift
        CORE_DIR="$1"
      ;;
    --generated-data-dir)
        check_option_has_value "${@}"
        shift
        GENERATED_DATA_DIR="$1"
      ;;
    --build-dir)
        check_option_has_value "${@}"
        shift
        BUILD_DIR="$1"
      ;;
    --build-mode)
        check_option_has_value "${@}"
        shift
        BUILD_MODE="$1"
      ;;
    --tests-to-run)
        check_option_has_value "${@}"
        shift
        TESTS_TO_RUN="$1"
      ;;
    --tests-to-skip)
        check_option_has_value "${@}"
        shift
        TESTS_TO_SKIP="$1"
      ;;
    -h|--help|-\?) usage ;;
    --local) LOCAL=true ;;
    --no-docker) USE_DOCKER=false ;;
    --inline-server-log) ;; # Legacy: this is the only option now
    --reuse-secrets-and-data) REUSE_SECRETS_AND_DATA=true ;;
    -?*)
      printGreen "WARN: Unknown option: $1"
      usage ;;
    *) break
  esac
  shift
done

if [ ${#} -gt 0 ]; then
  >&2 printGreen "No positional arguments expected: $*"
  usage
fi

readonly LOCAL
readonly USE_DOCKER
if [ "$USE_DOCKER" = false ] && [ "$LOCAL" = false ]; then
  >&2 printGreen "It is not possible to disable docker in a non-local build"
  usage
fi

readonly IMAGE="${IMAGE:-"$(default_image "$git_root")"}"
echo "IMAGE: $IMAGE"

# Set the base dirs

TESTS_DIR="$(make_absolute "$(dirname "$0")")"
readonly TESTS_DIR
readonly CORE_DIR="${CORE_DIR:-"$(make_absolute "$TESTS_DIR/..")"}"
echo "CORE_DIR: $CORE_DIR"

GENERATED_DATA_DIR="${GENERATED_DATA_DIR:-"$CORE_DIR"/temp/integration}"

if [ "$REUSE_SECRETS_AND_DATA" = false ]; then
  rm -rf "$GENERATED_DATA_DIR" || (printYellow "Attempting to delete files that were created by Docker, using elevated credentials. Please remove $GENERATED_DATA_DIR manually using sudo." && exit 1)
fi
mkdir -p "$GENERATED_DATA_DIR"
GENERATED_DATA_DIR="$(make_absolute "$GENERATED_DATA_DIR")"
readonly GENERATED_DATA_DIR
echo "GENERATED_DATA_DIR: $GENERATED_DATA_DIR"

readonly DATA_DIR="$GENERATED_DATA_DIR/data"
mkdir -p "$DATA_DIR"
readonly PKI_DIR_ON_HOST="$GENERATED_DATA_DIR/pki"
mkdir -p "$PKI_DIR_ON_HOST"
readonly S3PROXY_RUNTIME_DIR="$GENERATED_DATA_DIR/s3proxy-runtime"
mkdir -p "$S3PROXY_RUNTIME_DIR"

if [ "$LOCAL" = true ]; then
  BUILD_DIR="$(make_absolute "${BUILD_DIR:-$CORE_DIR/build}")"
  readonly BUILD_DIR
  echo "BUILD_DIR: $BUILD_DIR"
  readonly PKI_DIR="$PKI_DIR_ON_HOST"
else
  if [ -n "${BUILD_DIR-}" ]; then
    >&2 printGreen "It is not possible to specify a build directory in a non-local build"
    usage
  fi
  if [ -n "${BUILD_MODE-}" ]; then
    >&2 printGreen "It is not possible to specify a build mode in a non-local build"
    usage
  fi
  readonly BUILD_DIR="builddir" # Must be set to something, actual value is not relevant for non-local use
  readonly PKI_DIR="/pki"
fi

readonly BUILD_MODE="${BUILD_MODE:-.}"
echo "BUILD_MODE: $BUILD_MODE"

readonly TESTS_TO_RUN="${TESTS_TO_RUN:-}"
TESTS_TO_SKIP="${TESTS_TO_SKIP:-}"
for test in $default_skip; do
  if ! contains " $TESTS_TO_RUN " " $test "; then
    TESTS_TO_SKIP+=" $test"
  fi
done
readonly TESTS_TO_SKIP="$TESTS_TO_SKIP"
echo "TESTS_TO_RUN: ${TESTS_TO_RUN:-<all>}; except TESTS_TO_SKIP: ${TESTS_TO_SKIP:-<none>}"

# Settings for pepStorageFacilityUnitTests
export PEP_ROOT_CA=../pki/rootCA.cert
export PEP_S3_ACCESS_KEY="MyAccessKey"
export PEP_S3_SECRET_KEY="MySecret"
export PEP_S3_EXPECT_COMMON_NAME="S3"
export PEP_S3_TEST_BUCKET=TestBucket1
export PEP_S3_TEST_BUCKET2=TestBucket2
export PEP_USE_CURRENT_PATH="1"

cleanup() {
  if [ "$LOCAL" = false ]; then
    docker stop pepservertest || true
    docker rm pepservertest || true
  fi
  if [ "$USE_DOCKER" = true ]; then
    "$S3PROXY_RUNTIME_DIR/s3proxy.sh" stop || true
    docker network rm pep-network || true
  fi
}

# Clean leftovers
cleanup &>/dev/null

finish() {
  sig=$?

  # If this function is invoked due to (e.g.) SIGINT having been trapped,
  # prevent the function from being invoked again when EXIT is trapped.
  trap - EXIT

  printGreen "########################################################## Cleanup stage #########################################################"

  jobs="$(jobs -rp)"
  if [ -n "$jobs" ]; then
    # shellcheck disable=SC2086 # Split PIDs
    kill $jobs || true
    # shellcheck disable=SC2086 # Split PIDs
    wait -f $jobs || true
  fi

  printGreen "Please ignore any errors below."
  cleanup
  printGreen " If this script failed, please ignore any errors directly above;"
  printGreen " these are just errors in the \"Cleanup stage\" that do not cause a non-zero exit."

  echo
  if [ "$sig" != 0 ]; then
    printYellow "Error: Exited with code $sig!" >&2
    case "$sig" in
      124) info=timeout   ;;

      129) info=SIGHUP    ;;
      130) info=SIGINT    ;;
      131) info=SIGQUIT   ;;
      132) info=SIGILL    ;;
      133) info=SIGTRAP   ;;
      134) info=SIGABRT   ;;
      135) info=SIGBUS    ;;
      136) info=SIGFPE    ;;
      137) info='SIGKILL (out of memory?)' ;;
      138) info=SIGUSR1   ;;
      139) info=SIGSEGV   ;;
      140) info=SIGUSR2   ;;
      141) info=SIGPIPE   ;;
      142) info=SIGALRM   ;;
      143) info=SIGTERM   ;;
      144) info=SIGSTKFLT ;;
      145) info=SIGCHLD   ;;
      146) info=SIGCONT   ;;
      147) info=SIGSTOP   ;;
      148) info=SIGTSTP   ;;
      149) info=SIGTTIN   ;;
      150) info=SIGTTOU   ;;
      151) info=SIGURG    ;;
      152) info=SIGXCPU   ;;
      153) info=SIGXFSZ   ;;
      154) info=SIGVTALRM ;;
      155) info=SIGPROF   ;;
      156) info=SIGWINCH  ;;
      157) info=SIGIO     ;;
      158) info=SIGPWR    ;;
      159) info=SIGSYS    ;;
      *) info= ;;
    esac
    if [ -n "$info" ]; then
      printYellow "$sig = $info" >&2
    fi
  else
    printGreen "Test script terminated without errors. All tests passed."
  fi
}

trap finish EXIT
trap finish SIGINT # Let cleanup function know that the script is being killed: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2623

printGreen "########################################################## Configuration stage #########################################################"
trace cp -r "$CORE_DIR"/config/integration/config/* "$DATA_DIR"
trace cp "$CORE_DIR/config/projects/gum/accessmanager/GlobalConfiguration.json" "$DATA_DIR/accessmanager/"
mkdir -p "$DATA_DIR/storagefacility/localpagestoredata/myBucket/"

if [ "$REUSE_SECRETS_AND_DATA" = false ]; then
  trace cp -r "$CORE_DIR"/config/integration/secrets/* "$DATA_DIR"
  trace cp -r "$CORE_DIR"/config/OAuthTokenSecret.json "$DATA_DIR/keyserver/"
  trace cp "$DATA_DIR/keyserver/OAuthTokenSecret.json" "$DATA_DIR/authserver/OAuthTokenSecret.json"

  if [ "$LOCAL" = true ]; then
    trace cd "$PKI_DIR_ON_HOST"
    trace "$CORE_DIR/pki/pki.sh"
  else
    trace docker run --pull=always --rm -v "$PKI_DIR_ON_HOST:$PKI_DIR" -w=$PKI_DIR "$IMAGE" bash /app/pki.sh all /app/ca_ext.cnf
  fi
fi

if [ "$USE_DOCKER" = true ]; then
  trace docker network create pep-network

  trace "$CORE_DIR/s3proxy/s3proxy.sh" stage "$S3PROXY_RUNTIME_DIR" "$PKI_DIR_ON_HOST/s3certs" "$REUSE_SECRETS_AND_DATA"
  # Create test buckets for pepStorageFacilityUnitTests, preventing 'NoSuchBucket' error code from s3proxy.
  # Specifying "--parents" to prevent failure when directories already exist, e.g. if
  # this script is run multiple times.
  mkdir -p "$S3PROXY_RUNTIME_DIR/data/$PEP_S3_TEST_BUCKET"
  mkdir -p "$S3PROXY_RUNTIME_DIR/data/$PEP_S3_TEST_BUCKET2"
  trace "$S3PROXY_RUNTIME_DIR/s3proxy.sh" pull

  trace "$S3PROXY_RUNTIME_DIR/s3proxy.sh" start pep-network
  trace sleep 10
fi

if [ "$LOCAL" = true ]; then
  # We need a slightly modified StorageFacility.json:
  trace cd "$DATA_DIR/storagefacility"
  trace rm "StorageFacility.json"
  if [ "$USE_DOCKER" = true ]; then
    trace mv "StorageFacility.local.json" "StorageFacility.json"
  else
    trace mv "StorageFacility.local.no.docker.json" "StorageFacility.json"
  fi

  trace cd "$DATA_DIR"
  trace "$CORE_DIR/docker/init_keys.sh" "$REUSE_SECRETS_AND_DATA" "$DATA_DIR" "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE"
  if [ "$REUSE_SECRETS_AND_DATA" = false ]; then
    trace "$CORE_DIR/docker/config_servers.sh" \
      "$DATA_DIR" \
      "$PKI_DIR_ON_HOST" \
      "$BUILD_DIR/cpp/pep/storagefacility/$BUILD_MODE/pepStorageFacility" \
      "$BUILD_DIR/cpp/pep/keyserver/$BUILD_MODE/pepKeyServer" \
      "$BUILD_DIR/cpp/pep/accessmanager/$BUILD_MODE/pepAccessManager" \
      "$BUILD_DIR/cpp/pep/transcryptor/$BUILD_MODE/pepTranscryptor" \
      "$BUILD_DIR/cpp/pep/registrationserver/$BUILD_MODE/pepRegistrationServer" \
      "$BUILD_DIR/cpp/pep/authserver/$BUILD_MODE/pepAuthserver" \
      "$BUILD_DIR/cpp/pep/cli/$BUILD_MODE/pepcli" \
      "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepEnrollment"
  fi
  printGreen "\$ $BUILD_DIR/cpp/pep/servers/$BUILD_MODE/pepServers &"
  trace start_servers_locally
else
  trace docker run --rm --net pep-network -v "$DATA_DIR:/data" "$IMAGE" bash /app/init_keys.sh "$REUSE_SECRETS_AND_DATA"
  if [ "$REUSE_SECRETS_AND_DATA" = false ]; then
    trace docker run --rm --net pep-network -v "$DATA_DIR:/data" -v "$PKI_DIR_ON_HOST:$PKI_DIR" "$IMAGE" bash /app/config_servers.sh
  fi
  trace docker run --net pep-network -v "$DATA_DIR:/data" -v "$PKI_DIR_ON_HOST:$PKI_DIR" -v "$TESTS_DIR/test_input:/test_input" --name pepservertest -d "$IMAGE"
  docker logs --follow pepservertest 2> >(sed -u "s/^/[pep-services]: /" >&2) > >(sed -u "s/^/[pep-services]: /") &
fi


execute client "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepEnrollment" ClientConfig.json 1 "ewogICAgInN1YiI6ICJhc3Nlc3NvciIsCiAgICAiZ3JvdXAiOiAiUmVzZWFyY2ggQXNzZXNzb3IiLAogICAgImlhdCI6ICIxNTQyODk1ODg3IiwKICAgICJleHAiOiAiMjA3MzY1NDEyMiIKfQo.cNoT3VMtEZkrHGLOayqj3gwaM7R2BYv24FJpshecK4s" ClientKeys.json
execute client cat ClientKeys.json

printGreen "########################################################## Test stage #########################################################"


####################

if should_run_test storage-facility-unit; then
  if [ "$USE_DOCKER" = true ]; then
    TEST_FILTERS=""
  else
    TEST_FILTERS="--gtest_filter=-S3Client.putObject:PageStore.basic"
  fi
  # Note that line below invokes pepStorageFacilityUnitTests (and sets the DOCKER_EXEC_ARGS variable only for that invocation).
  DOCKER_EXEC_ARGS="-e PEP_ROOT_CA -e PEP_S3_ACCESS_KEY -e PEP_S3_SECRET_KEY -e PEP_USE_CURRENT_PATH -e PEP_S3_HOST=s3proxyproxy -e PEP_S3_EXPECT_COMMON_NAME -e PEP_S3_TEST_BUCKET -e PEP_S3_TEST_BUCKET2" \
    execute . "$BUILD_DIR/cpp/pep/storagefacility/$BUILD_MODE/pepStorageFacilityUnitTests" --gtest_color=yes "$TEST_FILTERS"
fi

####################

if should_run_test watchdog; then
  execute watchdog "$BUILD_DIR/go/src/pep.cs.ru.nl/pep-watchdog/pep-watchdog" -oneshot -instant-stressor
fi

####################

if should_run_test client; then
  execute client "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepClientTest" ClientConfig.json 1 POM-1234
  execute client "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepClientTest" ClientConfig.json 2 POM-1234
fi

####################

if [ "$LOCAL" = true ]; then
  PEPCLI_COMMAND="$BUILD_DIR/cpp/pep/cli/$BUILD_MODE/pepcli"
  TEST_INPUT_DIR="$TESTS_DIR/test_input"
  CONFIG_DIR="$GENERATED_DATA_DIR/data"
else
  PEPCLI_COMMAND="/app/pepcli"
  TEST_INPUT_DIR="/test_input"
  CONFIG_DIR="/data"
fi

. "$TESTS_DIR/pepcli_tests.sh"

####################

if should_run_test dump-shadow; then
  execute . "$BUILD_DIR/cpp/pep/apps/$BUILD_MODE/pepDumpShadowAdministration" dump ShadowAdministration.key registrationserver/ShadowShortPseudonyms.sqlite
fi

####################

if should_run_test watchdog; then
  execute watchdog "$BUILD_DIR/go/src/pep.cs.ru.nl/pep-watchdog/pep-watchdog" -oneshot -instant-stressor
fi

####################
