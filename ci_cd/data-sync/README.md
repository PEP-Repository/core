# Data synchronization

## How to set up data synchronization

1. Within a project repository (e.g. `pep/ppp-config`), the `config` directory
    contains two sets of configuration: one for the acceptance environment
    (usually in subdirectory `acc`) and one for the production environment
    (usually in subdirectory `prod`). Both subdirectories contain a
    `constellation.json` file specifying the environment's hosts and services,
    and how they can be accessed, updated, and controlled.
2. The `constellation.json` for the `acc` environment contains a *single* host
    with a node named `sync-pull`. This will be the "pull host" in the data
    sync procedure.
3. The `constellation.json` for the `prod` environment contains a `sync-push`
    node for *all* hosts that have data to be synchronized. These will be the
    "push hosts" in the data sync procedure.
4. Push hosts (i.e. servers in the `prod` environment) have been prepared with
    a script that can copy data from that host to the corresponding `acc` host.
    There's also an SSH trigger that allows the script to be executed. Alternatively,
    for push hosts that allow it, the script can be deployed to the push host during a
    CI pipeline's `deploy` phase. To configure this,
    add a `script` child to the host's `sync-push` node.
5. During the `deploy` phase of a CI pipeline for the `acc` branch, the
   `constellation.sh` script is invoked to set up data synchronization for the
   `acc` environment. It
   - removes any old data sync stuff on the pull host.
   - generates a `push.sh` shell script that invokes all SSH triggers on all
     push hosts, and an associated `push.known_hosts` file containing the host
     keys.
   - copies scripts to push hosts that support it.
   - copies all required data sync files to the pull host:
     - the `pull.py` script, and
     - the generated `push.sh` script, and
     - the generated `push.known_hosts` file, and
     - SSH ID files required to run the SSH triggers on the push hosts.
6. The CI pipeline for the `acc` branch has a `sync-data` phase containing
   jobs that can be run manually to either overwrite the `acc` environment's
   data, or to perform a dry run (performing some tests without actually
   overwriting the data).

## How synchronization runs

When a CI pipeline's data sync job is then executed, the CI job:

- invokes the `constellation.sh` script's `sync-data` command, which
- invokes the `pull.py` script on the pull host, which
- invokes the `push.sh` script, which
- invokes the SSH triggers on each of the push hosts, which
- invoke the synchronization script on that push host.

The `constellation.sh` script's `sync-data` command forwards its arguments to
the `pull.py` script: see its `load_args` method for supported parameters. The
`pull.py` script performs the synchronization in a three-step process:

### Step I

The production watchdog is suspended to prevent it from updating
production data while we're synchronizing. After this, the push hosts
make a list of the data to be copied from production to acceptance.

### Step II

After some time (a minute by default), the push hosts check if the
current production data is still equal to the list created in step I.
If something has changed, the list may represent an inconsistent state,
e.g. containing only part of a large upload. In this case, the
synchronization operation is aborted. Otherwise, the production watchdog
is (re-)started.

### Step III

The lists on the push hosts (apparently) represent a consistent state,
so actual synchronization may proceed. First the acceptance watchdog is
stopped to prevent it from interfering with data while we're updating
it. Then each of the production servers (push hosts) copies the listed
data to the corresponding acceptance server. The acceptance watchdog is
(re)started after this.

## What synchronization doesn't do

Synchronization only copies scripts to push hosts if those scripts are
specified in the host's `.sync-push.script` node. Other push hosts (e.g. those
under the control of a hosting party) are not updated automatically and must be
configured by hand.

Data are only synchronized from configured push hosts, and the scripts on
those push hosts determine which data are overwritten on the acceptance
environment. As a result, the acceptance watchdog may report (persistent)
checksum chain differences for services whose data have not been synchronized.
To get rid of these persistent issues (after having ensured that they have
indeed been caused by synchronization), follow the instructions on
<https://docs.pages.pep.cs.ru.nl/private/ops/latest/procedures_maintenance/monitoring/#clearing-persistent-issues>.
