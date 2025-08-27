#!/usr/bin/env bash

# usage: LOCAL_TESTSERVER_PATH/set_testserver_settings.sh BUILDPATH SCRIPTPATH


##########################################################################################
################################# Set some constants #####################################
##########################################################################################
SETCONFIGSCRIPTNAME="set_testserver_settings.sh"
SETTINGS_FILENAME="testserver_settings.config"

# Link OAuthTokenSecret.json?
TOKENSECRET=false
# Use symlinks instead of hard copies of files to copy to the working dir
SYMLINKS=true
# Skip the linking/copying of scripts (do not ask)
NOSCRIPTS=false
VERBOSE=false
LOCALSERVER=true

##########################################################################################
############################### Define some functions ####################################
##########################################################################################

# Some colorful output macro's to separate status msg from the rest.
function console_update {     # Color overview at https://stackoverflow.com/questions/5947742/how-to-change-the-output-color-of-echo-in-linux
    LINEEND="${2:-\n}"
    MSG_COLOR="${3:-0;102m}"
    printf "\033[%s%s\033[0m%b" "$MSG_COLOR" "$1" "$LINEEND"
 }
function console_alert { console_update "$1" "$2" "0;41m"; }

# A script termination routine to be called upon a forced shutdown.
function terminate_routine {
  #console_alert "Shutting down routine after EXIT SIGHUP SIGINT or SIGTERM"
  verbose "Set Testserver Settings script terminated."
  exit
}

trap terminate_routine SIGHUP SIGINT SIGTERM # Not included trap for EXIT signal, to provide explicit manual escape.

function verbose {
  if [ "$VERBOSE" = true ] ; then
    echo "$@"
  fi
}

function symlinkOrCopy {
  if [ -z "$2" ]; then
    DEST="."
  else 
    DEST="$2"
  fi

  if [ "$SYMLINKS" = true ]; then
    ln -s "$1" "$DEST"
  else
    cp "$1" "$DEST"
  fi
}

function print_cmd_usage { #replace function print_cmd_usage { console_update "set_testserver_settings.sh usage: LOCAL_TESTSERVER_PATH/$SETCONFIGSCRIPTNAME BUILDPATH SCRIPTPATH"; }
  echo "Usage: set_testserver_settings.sh [--buildPath/-b <path>] [--scriptPath/-s <path>] [--configPath/-c <path>] [--clientConfigPath/-j <path>] [--copyScripts/-d] [--noScripts/-n] [--help/-h] [--verbose/-v]
  
  Switches: 
    --buildPath/-b <path>           The (absolute or relative) path to the PEP build dir with the binaries. 
                                    If not supplied, the script will try to locate a build based on the script path.
    --scriptPath/-s <path>          The (absolute or relative) path to the 'local_testserver' scripts dir (typically 
                                    something like %PEP_PROJECT_ROOT%/scripts/local_testserver ). This value can 
                                    usually be derived by the script itself and doesn't have to be supplied.
    --configPath/-c <path>          The (absolute or relative) path where rootCA.cert and ShadowAdministration.pub 
                                    are located. If not supplied, a local configuration/testserver is assumed/tried/used 
                                    based on the build path.
    --clientConfigPath/-j <path>    The (absolute or relative) path that contains ClientConfig.json. If not supplied, 
                                    '%configPath%/client/' is used.
    --copyScripts/-d                Instead of creating symlinks, copy files to the current path.
    --noScripts/-n                  Don't ask to link/copy scripts in the current working path (and don't do it)
    --help/-h                       This overview.
    --verbose/-v                    Display more detailed information in the output and also use this setting in the scripts.
                                    Edit '$SETTINGS_FILENAME' to later modify settings.
    .";
}


##########################################################################################
############################ Bash script argument parsing ################################
##########################################################################################


# Substitute long options to short ones (thnx https://stackoverflow.com/a/30026641/3906500 )
for arg in "$@"; do
  shift
  case "$arg" in
    '--buildPath')        set -- "$@" '-b'   ;;
    '--scriptPath')       set -- "$@" '-s'   ;;
    '--configPath')       set -- "$@" '-c'   ;;
    '--clientConfigPath') set -- "$@" '-j'   ;;
    '--copyScripts')      set -- "$@" '-d'   ;;
    '--noScripts')        set -- "$@" '-n'   ;;
    '--help')             set -- "$@" '-h'   ;;
    '--verbose')          set -- "$@" '-v'   ;;

    *)                    set -- "$@" "$arg" ;; # why is this?
  esac
done

# Parse short options
#OPTIND=1

while getopts 'vdc:b:s:j:hn' flag; do
  case "${flag}" in
    b)  # SET BUILD PATH, remove trailing slashes
            #shellcheck disable=SC2001
            BUILD_ARG=$(echo "$OPTARG" | sed 's:/*$::') 
            ;;
    c)  # CONFIG PATH, remove trailing slashes. Assume no local server is used.
            LOCALSERVER=false
            #shellcheck disable=SC2001
            CONFIG_ARG=$(echo "$OPTARG" | sed 's:/*$::')
            if [ -z "$CLIENTCONFIGFILE_ARG" ]; then # If not defined yet, assume ClientConfig.json path
              CLIENTCONFIGFILE_ARG="$CONFIG_ARG/client/ClientConfig.json"
            fi
            ;;
    d)  # Create hard copy of scripts instead of symlinks    
            SYMLINKS=false
            ;;
    j)  # ClientConfig.json PATH, Assume no local server is used.
            LOCALSERVER=false
            CLIENTCONFIGFILE_ARG=${OPTARG}
            ;;
    n)  # Don't create script (nor prompt for it)
            NOSCRIPTS=true
            ;;
    s)  # SCRIPT PATH, remove trailing slashes
            #shellcheck disable=SC2001
            SCRIPT_ARG=$(echo "$OPTARG" | sed 's:/*$::')
            ;;
    v)  # set verbose
            VERBOSE='true' 
            ;;
    h)      #console_alert "HELP!"
            print_cmd_usage 
            exit
            ;;
    ?)
            console_alert "Unknown parameter in $*."
            print_cmd_usage
            exit 2
            ;;
  esac
done # getopts

#shift $(expr $OPTIND - 1) # remove options from positional parameters


##########################################################################################
######################## Perform some path guesses and checks ############################
##########################################################################################

#################### SCRIPTPATH ###################
# Guess $SCRIPTPATH when not supplied
if [ -z "$SCRIPT_ARG" ]; then
  SCRIPTPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  verbose "First argument missing, assuming $SCRIPTPATH as the correct SCRIPTPATH value. [ OK ]"
else
  verbose "Configuration: BUILDPATH set to $SCRIPT_ARG (not verified)"
  SCRIPTPATH="$SCRIPT_ARG"
fi
# Check $SCRIPTPATH validity
if [ ! -f "$SCRIPTPATH/$SETCONFIGSCRIPTNAME" ]; then
  console_alert "SCRIPTPATH doesn't contain $SETCONFIGSCRIPTNAME. This is probably wrong. Exiting."
  print_cmd_usage
  exit
fi

#################### BUILDPATH  ###################
# Guess and validate $BUILDPATH
if [ -z "$BUILD_ARG" ]; then


  tryPaths=("$SCRIPTPATH/../../build/cpp" "$SCRIPTPATH/../../build/Debug/cpp" "$SCRIPTPATH/../../build/Release/cpp")
  for tryPath in "${tryPaths[@]}"; do # Probing options where rootCA might be found:
    if [[ -f "$tryPath/pep/cli/pepcli" ]]; then # check if pepcli exists at given path
      RELBUILDPATH="$tryPath" # set found loc
      BUILDPATH="$(cd "$(dirname "${RELBUILDPATH}")" && pwd)"
      verbose "--buildPath argument missing, assuming '$BUILDPATH' as the correct BUILDPATH value.  [ OK ]"
      break; #break for loop
    else
      verbose "./pep/cli/pepcli not in $tryPath."
    fi
  done

  # If no BUILDPATH was set, throw an error.
  if [ -z "$BUILDPATH" ]; then
      console_alert "--buildPath  argument (build dir) missing. Failed guessing one from (${tryPaths[*]}) as well (couldn't find './pep/cli/pepcli' in guesses)."
      print_cmd_usage
      exit
  fi

# If BUILDPATH was supplied via parameter:
else
  verbose "Configuration: BUILDPATH set to $BUILD_ARG (not verified)"
  BUILDPATH="$BUILD_ARG"
fi

CLIPATH="${BUILDPATH}/cpp/pep/cli"

##########################################################################################
#################### Check the availability of some configurations #######################
##########################################################################################

# Validating CONFIGPATH
if [ -n "$CONFIG_ARG" ]; then # # If CONFIG_ARG is provided argument given (manual setting of configuration path)
  CONFIG_ALLGOOD=true

  # Checking for availability of ShadowAdministration.pub
  if [[ -f "$CONFIG_ARG/ShadowAdministration.pub" ]]; then
    verbose "$CONFIG_ARG/ShadowAdministration.pub found. [OK]"
  else
    CONFIG_ALLGOOD=false
    console_alert "$CONFIG_ARG/ShadowAdministration.pub NOT FOUND. [WARNING]"
  fi

  # Checking for availability of rootCA.cert
  if [[ -f "$CONFIG_ARG/rootCA.cert" ]]; then
    verbose "$CONFIG_ARG/rootCA.cert found. [OK]"
  else
    CONFIG_ALLGOOD=false
    console_alert "$CONFIG_ARG/rootCA.cert NOT FOUND. [WARNING]"
  fi

  # Check for '/client/ClientConfig.json' (set directly, or via @CONFIG_ARG, at the argument parsing)
  if [[ -f "$CLIENTCONFIGFILE_ARG" ]]; then
    verbose "$CLIENTCONFIGFILE_ARG found. [OK]"
  else
    CONFIG_ALLGOOD=false
    console_alert "$CLIENTCONFIGFILE_ARG NOT FOUND. [WARNING]"
  fi
  # If all true: 
  if [ "$CONFIG_ALLGOOD" = true ] ; then
    console_update "Copying ShadowAdministration.pub, rootCA.cert and ClientConfig.json"
    symlinkOrCopy "$CONFIG_ARG/ShadowAdministration.pub" .
    verbose "rootCA at $CONFIG_ARG/rootCA.cert"
    symlinkOrCopy "$CONFIG_ARG/rootCA.cert" . 
    # ClientConfig.json will initiate a relative path to rootCA, which won't work upon symlinking, therefore a copy is made by default.
    cp "$CLIENTCONFIGFILE_ARG" .
  else
    console_alert "NOT linking ShadowAdministration.pub, rootCA.cert and ClientConfig.json. Fix warning and retry."
    exit
  fi # CONFIG_ALLGOOD=true
else # If NO CONFIG_ARG is provided
  console_update "No configuration provided. Assuming local server."

  console_update "Copying ShadowAdministration.pub and ClientConfig.json from cli path"
  symlinkOrCopy "$CLIPATH/ShadowAdministration.pub" .
  # ClientConfig.json will initiate a relative path to rootCA, which won't work upon symlinking, therefore a copy is made by default.
  cp "$CLIPATH/ClientConfig.json" .

  # Check possible paths for rootCA:
  possibleRootCALocs=("$CLIPATH/rootCA.cert" "$BUILDPATH/pki/rootCA.cert" "$BUILDPATH/cpp/pep/servers/keyserver/")
  for rootCALoc in "${possibleRootCALocs[@]}"; do # Probing options where rootCA might be found:
    if [[ -f "$rootCALoc" ]]; then # check if file exists at given path
      foundRootCALoc="$rootCALoc" # set found loc
      break; #break for loop
    fi
  done

  if [ -n "$foundRootCALoc" ]; then
    console_update "Linking rootCA from $rootCALoc"
    symlinkOrCopy "$foundRootCALoc" .
  else
    console_alert "Warning, no valid path for rootCA was found. Please copy or link manually to working dir."
  fi

  # Check for OAuthTokenSecret
  ## For the granting of privileges on the local server:
  TOKENFILE=OAuthTokenSecret.json
  if [ ! -f "$TOKENFILE" ]; then
    verbose "File $TOKENFILE does not exist in pwd."

  tryTokenPaths=("$BUILDPATH/../../../ops/config/$TOKENFILE" "$BUILDPATH/../../ops/config/$TOKENFILE")
  for tryTokenPath in "${tryTokenPaths[@]}"; do # Probing options where rootCA might be found:
    if [[ -f "$tryTokenPath" ]]; then # check if pepcli exists at given path
      TOKENPATH="$tryTokenPath" # set found loc
      verbose "Found $TOKENFILE in $TOKENPATH. [ OK ]"
      break; #break for loop
    else
      verbose "No $TOKENFILE found in any of ${tryTokenPaths[*]}"
    fi
  done
    if [ -f "$TOKENPATH" ]; then
      while true; do
        read -r -p "Do you wish to link \"$BUILDPATH/../../ops/config/$TOKENFILE\"? [y/n]? " yn
        case $yn in
          [Yy]* ) symlinkOrCopy "$SCRIPTPATH/../../../ops/config/OAuthTokenSecret.json"; TOKENSECRET=true; break;;
          [Nn]* ) break;;
          * ) echo "Please answer yes or no.";;
        esac
      done # end while loop
    fi # end if no source candidate for linking
  fi # end if no tokenfile
fi 

# shellcheck disable=SC2028
echo "# Generated config for local test scripts
SCRIPTPATH=$SCRIPTPATH
BUILDPATH=$BUILDPATH
TOKENSECRET=$TOKENSECRET
SERVERPID_FILENAME=\"serverpid.config\"
VERBOSE=$VERBOSE

# While we're here, why not add some innocent useful functions...
function verbose {
  if [ \"\$VERBOSE\" = true ] ; then
    echo \"\$@\"
  fi
}
# Some colorful output macro's to separate status msg from the rest.
function console_update {     # Color overview at https://stackoverflow.com/questions/5947742/how-to-change-the-output-color-of-echo-in-linux
    LINEEND=\"\${2:-\\\\n}\"
    MSG_COLOR=\"\${3:-0;102m}\"
    printf \"\\033[\${MSG_COLOR}\${1}\\033[0m\${LINEEND}\"
 }
function console_alert { console_update \"\$1\" \"\$2\" \"0;41m\"; }
#EOF" > $SETTINGS_FILENAME

if [ "$NOSCRIPTS" = false ]; then
  while true; do
    read -r -p "Create symlinks for some local_testserver scripts? [y/n]? " yn
    case $yn in
      [Yy]* ) 
        symlinkOrCopy "$SCRIPTPATH/logon.sh"
        symlinkOrCopy "$SCRIPTPATH/pepdo.sh"
        if [ "$LOCALSERVER" = true ]; then # No clientconfigfile, assuming local server.
          symlinkOrCopy "$SCRIPTPATH/startserver.sh"
          symlinkOrCopy "$SCRIPTPATH/stopserver.sh"
          symlinkOrCopy "$SCRIPTPATH/populate_testserver.sh"
          symlinkOrCopy "$SCRIPTPATH/pepservers.sh"; 
        fi 
        if [ "$TOKENSECRET" = true ]; then # Link scripts that use tokens
          symlinkOrCopy "$SCRIPTPATH/pepaa.sh"
          symlinkOrCopy "$SCRIPTPATH/pepra.sh"
          symlinkOrCopy "$SCRIPTPATH/pepda.sh"
        fi
        break;;
      [Nn]* ) exit;;
      * ) echo "Please answer yes or no.";;
    esac
  done # end while loop
fi #end if NOSCRIPTS
