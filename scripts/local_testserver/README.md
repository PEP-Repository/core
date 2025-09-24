# Local test server

## Setting up a local test server with participants

The scripts in this directory (PEPCORE/scripts/local_testserver) help setting up a local server with test participants. It will grant read and write access to all columns for the Data Administrator to do so (which are *not* withdrawn afterwards!).

It currently produces up to only 10 test participants. The behaviour may be extended in the future for optimal usage for testing purposes.

## Usage

### Step 1: Create a configuration file

Call the `set_testserver_settings.sh` (see below) script from your working dir which will create a configuration file called `testserver_settings.config`.

### Step 2: Populate the test server

Call `populate_testserver.sh` from your working dir.

## Bash script descriptions

### set_testserver_settings

Before the `populate_testserver.sh` script can be called, the paths of the build-dir and the scripts-dir must be defined in a configuration file.

This is done by the `set_testserver_settings.sh` script:

```bash
PATH_TO_PEPCORE/scripts/local_testserver/set_testserver_settings.sh
```

When using external servers, the settting of a `build dir path` and `configuration files` is required. Use the `set_testserver_settings.sh -h` flag for help on that.

In most other cases the script will try to locate a local build dir and local server configuration settings based om some expected architectures. Set them manually to override these.

### populate_testserver.sh

Will run a local server, extend the rights for DA as mentioned above, create 10 test-participants, show the contents of all rows and tables, and shut down the local server afterwards (this behaviour may be extended). When this script is not ended normally, the PEP servers might remain running in the background.

### pepaa.sh, pepda.sh, pepra,sh

Perform a `pepcli` instruction as resp. `Access Administrator`, `Data Aministrator` or `Research Assessor` in the current working dir, using the required oauth parameters and a token. Only use these files with local servers/builds.

### pepdo.sh

Perform a `pepcli` instruction in the local working dir after loggin in dynamically using `logon.sh`. Not to be used when using tokens (use pepaa.sh, pepda.sh or pepra.sh instead)

### startserver.sh, stopserver

Respectively start and stop a set of local pep servers. The start script will store a linux PID in a config-file which will be used to kill the processes with the stop command.
