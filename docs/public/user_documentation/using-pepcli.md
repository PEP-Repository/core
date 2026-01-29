# Using pepcli

The `pepcli` application is the primary command line interface (CLI) application to interact with the PEP system. It is available for multiple platforms, and is included in PEP's `client` Docker images and in the Windows client software installer. Among `pepcli`'s functionalities are the ability to [upload and download data](uploading-and-downloading-data.md), and to administer the PEP system.

The use of command line utilities such as `pepcli` is subject to details of the platform on which it is used. For example, a literal `*` (asterisk) parameter value must be escaped to `\*` on Linux to prevent [shell expansion](https://www.gnu.org/software/bash/manual/html_node/Shell-Expansions.html) ("globbing"). Such details are not (extensively) covered in this documentation. Users are expected to be knowledgeable enough about their platforms to perform basic tasks and avoid common pitfalls.

## General usage

The `pepcli` utility must be invoked from a command line, with parameters telling it what to do. The general form of invocation is

```plaintext
pepcli [general flags] <COMMAND> [flags] [parameters...]
```

The various [commands are documented](#commands) in some detail on this page. The general flags are [documented separately](#general-flags). Some commands have subcommands; some subcommands have sub-sub-commands, and so on:

```plaintext
pepcli [general flags] <COMMAND> <SUBCOMMAND> [flags] [parameters...]
pepcli [general flags] <COMMAND> <SUBCOMMAND> <SUBSUBCOMMAND> [flags] [parameters...]
```

The abilities and options of underlying commands are documented with the parent commands to which they apply.

## Command line help

The `pepcli` application provides command line help if it is invoked without parameters, or with the `--help` switch:

```plaintext
pepcli
pepcli --help
```

The `--help` switch is also supported by most (or all?) of `pepcli`'s commands and subcommands. This can be used to "drill down" through command line help to construct an appropriate command line, e.g. by sequentially invoking:

```shell
pepcli --help
```

Output includes the _ama_ command, so we then issue:

```shell
pepcli ama --help
```

Output includes the _query_ subcommand, so we then issue:

```shell
pepcli ama query --help
```

Output mentions the _\--column-group_ switch, so we then issue the completed command line:

```shell
pepcli ama query --column-group ShortPseudonyms
```

## Authentication

Most of `pepcli`'s commands will connect to one or more of the PEP servers, and most server requests will require the user to be [authenticated](access-control.md#enrollment). There are multiple ways to do this, which are explained in the following sections.

### pepLogon

The preferred method to authenticate to PEP, is by using `pepLogon`. The simplest way to do this is by simply running this command:

```shell
pepLogon
```

This will open a browser window, where you can log in, using the account of your own organization (e.g. your RadboudUMC or RU account).
If you have succesfully logged in, it writes a file, `ClientKeys.json`, to your current working directory.
`pepcli` will read this file and use it to contact the PEP servers. The file is valid for 12 hours, after which you will have to run `pepLogon` again.

In some situations, it is not possible to open a browser window directly from pepLogon. For example, when you are working on a server via SSH.
We call this a _limited environment_.
In this case you can run the following command:

```shell
pepLogon --limited-environment
```

This will output something like this:

```plaintext
Please open https://ppp-auth.pep.cs.ru.nl/auth?client_id=123&code_challenge=_Vq068t4yK9EGwrrhR37eY4CZdkhPOHBibq7tEduNa8&code_challenge_method=S256&redirect_uri=%2Fcode&response_type=code in your browser.
Paste your code here:
```

You should open the shown link in a browser. This can be on the same, or on a different computer then where you are running pepLogon from.
After logging in in the browser, you will get a code which you can paste into `pepLogon`.

When you use Docker or Apptainer/Singularity, pepLogon will automatically use this limited environment mode.

#### Long lived OAuth tokens

When you use `pepLogon` as described above, you will be authenticated for 12 hours. In some cases, this is not enough.
For example when you have a process that runs automatically on a certain schedule, or when you do a download that takes longer dan 12 hours.

In this case it is possible to request a so called [OAuth token](access-control.md#oauth-token), which is valid for a longer period.
Only users that have specifically been granted permission to use such tokens can request them. Contact your PEP support contact to request this permission.

You can use `pepLogon` to request such a long lived token:

```shell
pepLogon --long-lived
```

After logging in in your browser, a file, `OAuthToken.json` will be written to your working directory. `pepcli` will read it from the working directory.
You can use the [`--oauth-token` flag](#general-flags) flag to pass a different file to pepcli, if it has a different name or is not in your current working directory.

### Users from external organizations

At the time of writing, it is only possible for users from RadboudUMC and Radboud University to log in to PEP with their RadboudUMC- or RU-account.
We are working on making this possible for users from other organizations as well. But for now you will need to get an OAuthToken that is generated for you by the PEP team.
Please contact your PEP support contact to request a token.

`pepcli` will read the file `OAuthToken.json` from the working directory.
You can use the [`--oauth-token` flag](#general-flags) to pass a different file to pepcli, if it has a different name or is not in your current working directory

## General flags

The `pepcli` utility's general flags can be used to indicate how to connect to and [enroll with](access-control.md#authentication) the PEP system:

- `--client-working-directory` was used to specify a directory containing configuration files specifying how to connect to PEP's servers. Preferably, the client working directory is now set by using the environment variable PEP_CONFIG_DIR. If no value is specified, it will default to the directory containing the `pepcli` executable file, or on MacOS, the Resources directory one level up.
- `--client-config-name` specifies the name of the main (client) configuration file. If not specified, the file is assumed to be named `ClientConfig.json`.
- `--oauth-token` specifies an OAuth token to be used for [enrollment](access-control.md#enrollment), or the path to a file containing such an OAuth token. If not specified, the user is assumed to [have been enrolled](access-control.md#enrollment) prior to `pepcli`'s invocation, e.g. by means of the `pepLogon` utility.

For brevity, these general flags will not be mentioned in the documentation of individual commands, or in the examples below. But they may be included with any command issued to `pepcli`, e.g.:

```plaintext
pepcli --oauth-token /PATH/TO/OAuthToken.json --client-working-directory /PATH/TO/config-directory list -C \* -P \*
```

Other general flags exist, but are intended for use by developers of the PEP system. While mentioned in `pepcli`'s command line help, they are not further documented here.

## Commands

The `pepcli` utility supports commands for various tasks, aimed at different types of users:

General purpose:

- [`query`](#query) provides information on your access to the PEP environment.
  - [`query column-access`](#query-column-access) lists the columns and column groups accessible to the enrolled user.
  - [`query participant-group-access`](#query-participant-group-access) lists the participant groups accessible to the enrolled user.
  - [`query enrollment`](#query-enrollment) tells users how they're enrolled.

Data storage and retrieval:

- [`get`](#get) retrieves data from a specific cell.
- [`list`](#list) lists data available in PEP.
- [`pull`](#pull) stores a data set in files on your local machine.
- [`store`](#store) stores data in a specific cell.

Administrative tasks:

**For users enrolled as a `Data Administrator`:**

- [`ama`](#ama) provides subcommands to perform administrative tasks related to the Access Manager service.
  - [`ama query`](#ama-query) summarizes the current data structure and access rules.
  - [`ama column`](#ama-column) can be used to create and remove columns, and to group and un-group them.
  - [`ama columnGroup`](#ama-columngroup) can be used to create and remove column groups.
  - [`ama group`](#ama-group) can be used to create and remove participant groups, and add participants to them
    - [`ama group auto-assign`](#ama-group-auto-assign) can be used to automatically fill participant groups based on study contexts.

**For users enrolled as an `Access Administrator`:**

- [`ama`](#ama) provides subcommands to perform administrative tasks related to the Access Manager service.
  - [`ama query`](#ama-query) summarizes the current data structure and access rules.
  - [`ama cgar`](#ama-cgar) can be used to manage the type(s) of access that access groups have to column groups.
  - [`ama pgar`](#ama-pgar) can be used to manage the type(s) of access that access groups have to participant groups.
- [`castor`](#castor) provides subcommands to perform administrative tasks related to PEP's [Castor integration](castor-integration.md) functionality.
  - [`castor export`](#castor-export) exports castor data as csv.
  - [`castor list-sp-columns`](#castor-list-sp-columns) lists short pseudonym columns that are bound to Castor studies.
  - [`castor list-import-columns`](#castor-list-import-columns) lists columns required by PEP's [Castor import](castor-integration.md#import).
  - [`castor create-import-columns`](#castor-create-import-columns) creates columns required by PEP's [Castor import](castor-integration.md#import).
  - [`castor column-name-mapping`](#castor-column-name-mapping) allows a Data Administrator to configure [column name mappings](castor-integration.md#column-name-mappings) to have PEP [import Castor data](castor-integration.md#import) into appropriately named columns.
- [`user`](#user) provides subcommands to perform administrative tasks related to users. Only for users enrolled as an `Access Administrator`:
  - [`user query`](#user-query) List users and user groups
  - [`user create/remove`](#user-createremove) Create and remove users
  - [`user addIdentifier/removeIdentifier`](#user-addidentifierremoveidentifier) Add and remove identifiers for users
  - [`user setDisplayId`](#user-setdisplayid) Set the display ID for users.
  - [`user setPrimaryId/unsetPrimaryId`](#user-setprimaryidunsetprimaryid) Set or unset the primary ID for users.
  - [`user addTo/removeFrom`](#user-addtoremovefrom) Add users to or remove users from user groups
  - [`user group create/remove/modify`](#user-group-createremovemodify) Create, remove or modify user groups
- [`token`](#token) provides subcommands for requesting and blocking tokens. Only for users enrolled as an `Access Administrator`:
  - [`token request`](#token-request) Generate a new authentication token for a user
  - [`token block create/remove/list`](#token-block-create-remove-list) Blocking and unblocking tokens by managing a blocklist on the server.

## `ama`

The `ama` command's various sub-commands can be used to perform administrative tasks. While `ama` is short for "Access Manager Administration", it should be noted that `ama` provides subcommands for both the `Access Administrator` and `Data Administrator` roles. Users must be enrolled for the role appropriate for the subcommand they're invoking.

### `ama cgar`

The `cgar` subcommand is short for "column group access rule". It allows `Access Administrator` to determine the types of access that access groups have to column groups:

```plaintext
pepcli ama cgar create <column group name> <access group name> <mode>
pepcli ama cgar remove <column group name> <access group name> <mode>
```

The column group must have been previously created by a `Data Administrator` using the [`pepcli ama columnGroup`](#ama-columngroup) subcommand. The `mode` parameter must be either `read` or `write`, indicating the type of access to grant or revoke.

After using the `cgar` subcommand, the rule immediately takes effect. Users enrolled for the specified access group will immediately be granted (or denied) access to the specified column group.

### `ama pgar`

The `pgar` subcommand is short for "participant group access rule". It allows `Access Administrator` to determine the types of access that access groups have to participant groups:

```plaintext
pepcli ama pgar create <participant group name> <access group name> <mode>
pepcli ama pgar remove <participant group name> <access group name> <mode>
```

The participant group must have been previously created by a `Data Administrator` using the [`pepcli ama group`](#ama-group) subcommand. The `mode` parameter must be either `enumerate` or `access`, indicating the type of access to grant or revoke.

After using the `pgar` subcommand, the rule immediately takes effect. Users enrolled for the specified access group will immediately be granted (or denied) access to the specified column group.

### `ama column`

The `column` subcommand allows `Data Administrator` to create and remove columns:

```plaintext
pepcli ama column create <column name>
pepcli ama column remove <column name>
```

Because of technical limitations, PEP column names may contain only [printable ASCII characters](https://en.wikipedia.org/wiki/ASCII#Printable_characters). Additional restrictions apply to the names of columns into which Castor data are imported.
<!--- Link to and/or describe these resitrictions -->

Note that column removal will not discard data present in those columns; it will merely make the column's contents inaccessible. Therefore:

- when users retrieve data from an earlier moment in time, those data may include columns that have since been removed.
- when a column is removed and later re-added, the newly added column will contain any data that had previously been stored in the same column name.

The `column` subcommand can also be used to group and un-group columns into column groups. To add a column to a columngroup:

```plaintext
pepcli ama column addTo <column name> <column group name>
```

and to remove a column from a column group:

```plaintext
pepcli ama column removeFrom <column name> <column group name>
```

When columns are added to a column group, those columns immediately become available to users who can access the column group.

Column groups can be created and removed using [the `pepcli ama columnGroup`](#ama-columngroup) subcommand. `Access Administrator` can [grant access to column groups](access-control.md#access-rules) using the `pepcli ama cgar` subcommand.

### `ama columnGroup`

The `columnGroup` subcommand allows `Data Administrator` to create and remove column groups:

```plaintext
pepcli ama columnGroup create <column group name>
pepcli ama columnGroup remove <column group name>
```

Because of technical limitations, column group names may contain only [printable ASCII characters](https://en.wikipedia.org/wiki/ASCII#Printable_characters). Note that [some column groups](data-structure.md#grouping) are predefined and/or automatically managed by PEP software.

Once a column group has been created, use the [`pepcli ama column`](#ama-column) subcommand to determine which columns are included in the group. `Access Administrator` can [grant access to column groups](access-control.md#access-rules) using the `pepcli ama cgar` subcommand.

### `ama group`

The `group` subcommand allows `Data Administrator` to create and remove participant groups:

```plaintext
pepcli ama group create <participant group name>
pepcli ama group remove <participant group name>
```

The `group` subcommand also allows to add and remove participants to and from participant groups:

```plaintext
pepcli ama group addTo      <participant group name> <participant>
pepcli ama group removeFrom <participant group name> <participant>
```

There are multiple ways to specify `<participant>`:

- A PEP-id
- A _Local Pseudonym_. This is the pseudonym that is used as directory name, when doing `pepcli pull`.
- A _Participant Alias_. This is a shorter version, derived from (cropping the) _Local Pseudonym_ (formerly called _user pseudonym_).

 `Access Administrator` can [grant access to participant groups](access-control.md#access-rules) using the `pepcli ama pgar` subcommand.

### `ama group auto-assign`

Automatically update participant groups based on participant assignment to study _contexts_, and participants being marked as test participants. The command:

```shell
pepcli ama group auto-assign
```

- assigns every non-test participant to the `all` participant group, and,
- for every study context that a non-test participant belongs to, assigns that participant to a participant group named `all-[STUDY_CONTEXT]`, and,
- for every participant group whose name is `all` or starts with `all-`, removes participants from that group if they should not (or no longer) be assigned to that group, and,
- removes empty participant groups whose name is `all` or starts with `all-`.

By default the command only outputs the (re-)assignments that would be performed, but doesn't update the configuration. To actually perform the (re-)assignments, invoke the command with the `--wet` switch (indicating that it's not a "dry" run).

Use the command's `--mapname` switch to create `all-[MAPPED_REPLACEMENT]` groups instead of groups whose names correspond with a raw context name. Multiple `--mapname`s may be specified with a single invocation.

### `ama query`

The `query` subcommand summarizes the current state of PEP's [data structure](data-structure.md) and [access rules](access-control.md#access-rules). Both the `Access Administrator` and `Data Administrator` roles can invoke:

```plaintext
pepcli ama query
```

The output lists:

- The [`Columns`](#ama-column) that have been defined by data administrator.
- The [`ColumnGroups`](#ama-columngroup) that have been defined by data administrator, and the columns included in each column group.
- The [`ColumnGroupAccessRules`](#ama-cgar) that have been defined by access administrator, i.e. which access groups have what type(s) of access to which column groups.
- The [(participant) `Groups`](#ama-group) that have been defined by data administrator.
- The (participant) `GroupAccessRules` that have been defined by access administrator, i.e. which access groups have what type(s) of access to which participant groups.

## user

The `user` command's various sub-commands can be used to perform administrative tasks related to authentication. Users have
to be enrolled as `Access Administrator` in order to perform `user` commands.

### `user query`

The `user query` subcommand lists all user-groups and users that are known to the authentication server.
In addition to that, it also shows which user-groups each user belongs to.
The authentication server only knows about users that can log-in interactively.
Users that (only) received an OAuth-token don't have to be in the list.

This is an example of the invocation and output of `user query`:

```plaintext
> pepcli user query
- All User Groups: # size=4
  - Access Administrator
  - Data Administrator: {max auth valid time: 1d}
  - Monitor
  - Research Assessor

- All Interactive Users: # size=5
  - user identifiers: # size=3
      - YXNzZXNzb3JAbWFzdGVyLnBlcC5jcy5ydS5ubA
      - assessor@main.pep.cs.ru.nl
      - U1234567@university-of-peppers.com
    user groups: # size=1
      - Research Assessor

  - user identifiers: # size=1
      - monitor@main.pep.cs.ru.nl
    user groups: # size=1
      - Monitor

  - user identifiers: # size=1
      - dataadmin@main.pep.cs.ru.nl
    user groups: # size=1
      - Data Administrator

  - user identifiers: # size=2
      - YWNjZXNzYWRtaW5AbWFzdGVyLnBlcC5jcy5ydS5ubA
      - accessadmin@main.pep.cs.ru.nl
    user groups: # size=1
      - Access Administrator

  - user identifiers: # size=2
      - bXVsdGloYXRAbWFzdGVyLnBlcC5jcy5ydS5ubA
      - multihat@main.pep.cs.ru.nl
    user groups: # size=4
      - Access Administrator
      - Research Assessor
      - Data Administrator
      - Monitor

  - user identifiers: # size=1
      - no_group@main.pep.cs.ru.nl
    user groups: # size=0
```

Some things to note:

- For the user group `Data Administrator` it says: `{max auth valid time: 1d}`. This means that users in that user group
  can request tokens for themselves. These tokens will have a validity of 1 day, unless the user asks for a shorter validity.
  Longer validity is not allowed.
- Users have one or more user identifiers. See [`user addIdentifier/removeIdentifier`](#user-addidentifierremoveidentifier)
  for more information about using multiple identifiers.

The result of `user query` can be filtered with one of the following flags:

- `--group <value>` only lists user groups that contain the substring `<value>` in their name, and the users in those groups
- `--user <value>` only lists users that contain the substring `<value>` in one of their user identifiers, and the user groups they belong to

You can use `--format json` to format the output of `user query` as json.

### `user create/remove`

New users can be created with the following command:

```shell
pepcli user create <uid>
```

This will create a user, identified by user identifier `<uid>`. This is typically an e-mail address.

You can remove a user with:

```shell
pepcli user remove <uid>
```

This will remove the user, together with all their identifiers. It is not possible to remove users which are still in user
groups. You have to remove them from those user groups first.

### `user addIdentifier/removeIdentifier`

Users have one or multiple user identifiers. Typically they have:

- A human readable identifier, such as an e-mail address
- A complex random-looking string of characters. This is a system ID. Things like e-mail addresses can change,
  so login services usually also send a system ID to PEP, which will never change. These system IDs are inconvenient to
  work with, so you can use both in PEP. When new users are created, you can add them based on a human readable identifier.
  The system ID will be added automatically when they first login.
- Sometimes there are additional identifiers. Depending on the login service that is used by the user, there can be multiple
  attributes that PEP receives. For example, from SURFconext we can receive not only the e-mail address, but also the
  [_eduPersonPrincipalName_](https://wiki.surfnet.nl/display/surfconextdev/Attributes+in+SURFconext#AttributesinSURFconext-eduPersonPrincipalNamePrincipalname).
  It is possible to add both, although it is usually not required.

It is also possible to add more possible values for a single attribute. E.g., if you know that an e-mail address will change,
and the SystemID is not yet stored for that user, you can already add the new e-mail address to PEP. The user will then
be able to log in as soon as their e-mail address changes.

These different kinds of user identifiers are not handled differently by the `user` commands. It is not stored in PEP if
a certain user identifier is a system ID or a human readable ID. The authentication server itself does treat them differently,
e.g. only a system ID is added automatically on first login. And a human-readable identifier is stored in the oauth token
the user receives from the authentication server. But this all happens automatically, so access administrators do not have
to take this into account.

Additional identifiers for a user can be added with

```shell
pepcli user addIdentifier <existingUid> <newUid>
```

You have to specify `<existingUid>`, which is already known to PEP, so that PEP knows which user `<newUid>` should be added to.

Identifiers can be removed with

```shell
pepcli user removeIdentifier <uid>
```

It is not possible to remove an identifier for a user if that is the only remaining identifier for that user. You can remove
the user entirely with the `pepcli user remove` command, if that is desired.

### `user setDisplayId`
The displayID for a user can be set by running
```shell
pepcli user setDisplayId <uid>
```

`<uid>` needs to be an exisitng userId for that user.

The displayId is used when PEP needs to choose one identifier of a user to display to e.g. an access administrator,
when they are administering users.

When a new user is created with `pepcli user create <uid>`, `<uid>` will be set as the current display ID.
It can be changed at a later time with `pepcli user setDisplayId <someOtherExistingUid>`.

It is not possible to unset the displayId. Once a displayId has been set for a user, that user will always have a displayId.

### `user setPrimaryId/unsetPrimaryId`
The primaryID for a user can be set by running
```shell
pepcli user setPrimaryId <uid>
```

`<uid>` needs to be an exisitng userId for that user.

Authentication sources often use a non-human readable, fixed identifier as a permanent identifier for a user. Human readable
IDs are often not guaranteed to remain the same. We call this fixed identifier the _primary ID_ of the user.

Primary IDs are not very suitable to be used by human users. So instead, we can register the user based on e.g. their e-mail address.
When a user first logs in, their primary ID is then automatically stored, and marked as the primary ID. If the e-mail address changes
at a later time, but the primary ID remains the same, the user will still be recognized by PEP.

If the primary ID for a user changes, but their human readable ID is still the same, PEP will not automatically use the new
primary ID. It is possible that, at different points in time, different users have the same e-mail address. PEP should not treat those
as being the same user. So that's why it doesn't automatically accept a changed primary ID. The user will get an error message,
and should contact their PEP support. The access administrator can then unset the primary ID, if that is indeed the desired solution.
The primary ID will then be auto-assigned again at the next login of the user.

Primary IDs can be unset with:
```shell
pepcli user unsetPrimaryId <uid>
```

### `user addTo/removeFrom`

Users can be added to, and removed from user groups with the following commands:

```shell
pepcli user addTo <uid> <group>
pepcli user removeFrom <uid> <group>
```

### `user group create/remove/modify`

User groups can be created and removed with the following commands:

```shell
pepcli user group create <name>
pepcli user group remove <name>
```

If users in a user group should be able to request tokens for themselves, you can create the group as follows:

```shell
pepcli user group create --max-auth-validity <validity_period> <name>
```

`<validity_period>` can be specified by a number, followed by one of the following:

- `d`, `day` or `days`, to specify the number of days
- `h`, `hour` or `hours`, to specify the number of hours
- `m`, `minute` or `minutes`, to specify the number of minutes
- `s`, `second` or `seconds`, to specify the number of seconds

To change the maximum token validity of an existing group, you can use:

```shell
pepcli user group modify --max-auth-validity <validity_period> <name>
```

If you want to remove the ability for users of a user group to request tokens for themselves, you can use the `user group modify`
subcommand, with the `--max-auth-validity` left out:

```shell
pepcli user group modify <name>
```

## token
The `token` command's various sub-commands can be used to perform administrative tasks related to tokens, such as requesting
new tokens and blocking tokens. Users have to be enrolled as `Access Administrator` in order to perform `token` commands.

### token request

```shell
Usage: pepcli token request [--expiration-yyyymmdd <value>] [--json] <subject> <user-group> [expiration-unixtime]
```

This command is used to generate a new authentication token for a user.
It requires the following positional parameters:

1. `subject`: identifier of the user for which the token is generated. This could be an email address or an employee-id
   for example.
1. `user-group`: the user group for which the token is generated

In addition to that, you are _required_ to pass an expiration date/time, which can be done in two ways:

- **setting a date with the `--expiration-yyyymmdd` switch (recommended):**
  This switch expects the year, month and day as an 8 digit string without any separators.
  For example, February 8, 2026, would be `20260208` and October 21, 2027 would be `20271021`.
  Note that the token will expire at midnight before the given date: if you set the expiration date to `20270101`,
  then the token will be valid on the last day of 2026, but not on the first day of 2027.
- **passing a timestamp with a 3rd positional argument, `expiration-unixtime` (advanced):**
  This parameter expect a unix timestamp, pinpointing the exact second at which the token will expire.
  Use this variant if you need more precision than just a plain date.

The command prints the token to the screen, which is a long string of random-looking characters.

Adding the `--json` switch will output the token in JSON format that can be used in a JSON authentication key file.

!!! note "Note"
    The command usage notation we use here and in the `--help` text does not consider relations between parameters, so
    both parameters are listed as optional, even though you are required to pass one of them.

#### Token request examples

```shell
# Generate a token that authenticates user "U1234567@university-of-peppers.com" as a "Research Assessor",
# that is valid until the 22nd of March 2027
> pepcli token request "U1234567@university-of-peppers.com" "Research Assessor" --expiration-yyyymmdd 20270322
iMTc1NjQ2[...]62buW1Sf

# Generate a token that authenticates user "dataadmin@main.pep.cs.ru.nl" as a "Access Administrator",
# that is valid until 10:00:00 AM at August 29, 2025 (GMT).
# Print the output as JSON this time.
> pepcli token request --json "dataadmin@main.pep.cs.ru.nl" "Access Administrator" 1756461600
{
  "OAuthToken": "iMTc1NjQ2[...]CdI-IxNz"
}
```

### token block create, remove, list

This family of commands can be used to block existing tokens, that are already out in the open,
so that they can no longer be used. This should only be necessary in exceptional cases, such as:

- **A user left a project:** A user was granted access to a project using a token, but is now no longer part of
  that project. In this you want to make sure that this token is no longer valid.
- **A token was sent to the wrong person**: Generating tokens and then handing out tokens requires some manual work and
  mistakes can happen in this process. Maybe the wrong e-mail address was entered when sending out the token or maybe
  multiple tokens were generated in a batch and they somehow got mixed up. Whatever happened, there is now someone who
  has access to a token that was meant for someone else and this needs to be resolved.
- **A token was lost**: The token was received by the right person, but something happened and now they no longer have
  access to it. Maybe they put it on a USB stick that fell out of their pocket or maybe they accidentally deleted it
  from their computer without knowing. Often it is just unclear what happened, but the fact remains that the rightful
  owner can no longer use the token.
- **In case of a data breach**: There was a data leak, either by accident or by a malicious act. Tokens that were leaked
  can no longer be considered to be secret.

The PEP servers maintain a blocklist to specify which tokens should be rejected, regardless of their validity period.
All `pepcli token block` commands operate on this blocklist.

A rejection rule has three fields, `subject`, `user-group` and `issuedBefore`. A single rule will block all tokens that
match the following criteria:

- The subject of the token matches subject of the rule
- The user-group of the token matches the user-group of the rule
- The issue date, which tells when the token was created, is before the `issuedBefore` date

#### token block create

```shell
Usage: pepcli token block create [--issuedBefore-unixtime <value>] [--issuedBefore-yyyymmdd <value>] --message <value> <subject> <user-group>
```

The main command is `pepcli token block create`, which can block tokens by adding new rejection rules to the list.
A block rule blocks up to a specific issue date (`issuedBefore`): all matching tokens that were issued/generated before
that date are blocked, tokens issued after

It requires the following positional parameters:

1. `subject` The user for which the the target token authenticates.
2. `user-group` The user-group for which the target token authenticates.

In addition to that, it requires a `--message` (or just `-m`), which is some free-form text to explain why the blocklist
entry was created.

By default, the block applies on all matching tokens that were generated before the block was requested. This behavior
can be altered with an `issuedBefore` switch, which comes in two flavours:

- `--issuedBefore-yyyymmdd` (recommended), which accepts a human readable 'yyyymmdd' date.
- `--issuedBefore-unixtime` (advanced), which accepts a unix timestamp.

Using both of these is not allowed and will be refused by the application.

A reason to explicitly pass an `issuedBefore` date/time could be that you already generated a new timestamp, before
creating the block rule. In that case, you could set `issuedBefore` to the date you created the new token or earlier,
so that this new token is not affected.

On success, the application will print the entry that was created, with the exact values that were stored in the
blocklist.

##### Token blocking examples

```shell
# For these examples we are logged in as "accessadmin@university-of-peppers.com", which is reflected in the output

# Block all existing tokens that authenticate user "user894@some_domain.com" as a "Monitor"
> pepcli token block create "user894@some_domain.com" "Monitor" -m "Token was mailed to wrong address"
id, targetSubject, targetUserGroup, targetIssueDateTime, note, issuer, creationDateTime
21, user894@some_domain.com, Monitor, 2024-09-26T13:06:37Z, Token was mailed to wrong address, accessadmin@university-of-peppers.com, 2024-09-26T13:06:37Z

# Similar command that only blocks tokens created before "2024"
> pepcli token block create "U1234567@university-of-peppers.com" "Research Assessor" -m "User no longer active" --issuedBefore-yyyymmdd "20240101"
id, targetSubject, targetUserGroup, targetIssueDateTime, note, issuer, creationDateTime
22, U1234567@university-of-peppers.com, Research Assessor, 2024-01-01T01:00:00Z, User no longer active, accessadmin@university-of-peppers.com, 2024-09-26T13:08:10Z
```

#### token block list

```shell
Usage: pepcli token block list
```

Run `pepcli token block list` to get a list of all rejection rules that are currently in effect.

The output includes the following information for each rejection rule:

- **id**: A unique numeric identifier for the entry
- **targetSubject**: The username that the entry is targeting
- **targetUserGroup**: The user-group that the entry is targeting
- **targetIssueDateTime**: Only tokens issued before this point in time are affected.
  This matches the `issuedBefore` value used in the `create` command.
- **note**: Note from the creator, usually to explain why the entry was created.
- **issuer**: The user that created the entry
- **creationDateTime**: When the entry was created

##### Examples

```shell
> pepcli token block list
id, targetSubject, targetUserGroup, targetIssueDateTime, note, issuer, creationDateTime
21, user894@some_domain.com, Monitor, 2024-09-26T13:06:37Z, Token was mailed to wrong address, accessadmin@university-of-peppers.com, 2024-09-26T13:06:37Z
22, U1234567@university-of-peppers.com, Research Assessor, 2027-01-01T00:00:00Z, User no longer active, accessadmin@university-of-peppers.com, 2024-09-02T13:01:03Z
```

#### token block remove

```shell
Usage: pepcli token block remove <id>
```

The `pepcli token block remove` can remove any blocklist entry that is currently in effect.
This essentially rolls back a previously entered `pepcli token block create`.

The command expects one parameter, namely the numeric token blocklist id of the entry that you wish to remove,
which can be found with a call to `pepcli token block list`

##### Token block remove example

```shell
# Remove the blocklist entry with id = 11
pepcli token block remove 11
```

## castor

The `castor` command's various sub-commands are used to interact with PEP's [Castor integration](castor-integration.md) functionality. Most commands are intended for (and restricted to) use by `Data Administrator`s to configure the system for the [import of Castor data](castor-integration.md#import).

### castor column-name-mapping

To prevent the Castor import from using overly long and/or difficult to interpret column names, Data Administrator can use the `pepcli castor column-name-mapping` command to define [column name mappings](castor-integration.md#column-name-mappings). PEP's import routine will then import Castor data into columns named after the mappings rather than after the raw Castor names. The command's subcommands provide basic CRUD operations. The "read" operations can be performed by any enrolled user:

- `pepcli castor column-name-mapping list` lists all configured column name mappings.
- `pepcli castor column-name-mapping read <castor>` reports the mapping that is applied to Castor entities whose name matches `<castor>`. If no such mapping exists, the output will be empty.

Manipulation of column name mappings requires enrollment as a `Data Administrator`:

- `pepcli castor column-name-mapping create <castor> <pep>` creates a new column name mapping, causing PEP to use the specified `<pep>` replacement for Castor entities whose name matches `<castor>`.
- `pepcli castor column-name-mapping update <castor> <pep>` specifies a new `<pep>` replacement for an existing column name mapping for the specified `<castor>` name.
- `pepcli castor column-name-mapping delete <castor>` removes an existing column name mapping for the specified `<castor>` name. Subsequent import runs will revert to using the Castor name to determine the name of the column where data will be stored.

### castor create-import-columns

This command ensures that all columns exist that will be needed when the specified Castor data are [imported into PEP](castor-integration.md#import). Since the import process cannot create columns itself, Data Administrator can use this command to create them beforehand. When invoked without further parameters, the command creates missing columns for all Castor data that will be imported. Each created column is also automatically added to the `Castor` column group, ensuring that the column is writable to the import process:

```plaintext
pepcli castor create-import-columns
```

The above switch-less invocation may lack information required to create import columns for some short pseudonym columns. These will be reported, and the command can then be invoked with switches to create columns for data import from affected Castor studies:

- the `--sp-column` switch specifies the column containing short pseudonym values that correspond with the Castor study's record IDs.
- when data are to be imported from surveys that are answered multiple times by participants, the `--answer-set-count` must be used to indicate how many answer sets are to be expected. For example, if participants will answer a survey a maximum of 10 times, specify `--answer-set-count 10` to have the command create sufficient column names for all survey answers to be imported.

E.g. when a Castor study is bound to short pseudonym column `ShortPseudonym.Covid.Castor.CovidQuestionnaires`, and that study contains survey packages that are answered up to 30 times:

```plaintext
pepcli castor create-import-columns --sp-column ShortPseudonym.Covid.Castor.CovidQuestionnaires --answer-set-count 30
```

### castor export

Exports castor data as csv.

### castor list-import-columns

This command lists the columns that are needed when the specified Castor data are [imported into PEP](castor-integration.md#import). Since the import process cannot create columns itself, Data Administrator must create required columns beforehand. (S)he then uses this command to find out which columns are needed. The command must be invoked with the `--sp-column` switch, specifying the column containing short pseudonym values that correspond with the Castor study's record IDs. E.g.:

```plaintext
pepcli castor list-import-columns --sp-column ShortPseudonym.Visit1.Castor.HomeQuestionnaires`
```

While the output lists the columns that will be needed by the import process, missing columns are not created by this command. Use the [`castor create-import-columns` command](#castor-create-import-columns) instead to have the columns created instead of just listed. Alternatively, Data Administrator can `castor list` the columns, then invoke [`ama column`](#ama-column)'s `create` and `addTo` commands manually to create the columns and group them into the `Castor` column group.

The `castor list-import-columns` command accepts an optional switch:

- the `--answer-set-count` is used for Castor studies containing surveys that are answered multiple times by participants. PEP imports such survey data into [numbered columns](castor-integration.md#column-naming), and the switch indicates the number of answer sets to expect. For example, if participants will answer a survey a maximum of 10 times, specify `--answer-set-count 10` to have the command list sufficient column names for all survey answers to be imported.

### castor list-sp-columns

When invoked without further parameters, this command lists the names of all short pseudonym columns that refer to a Castor study. This means that the values in the listed columns are the Castor record IDs for the corresponding participant.

The command accepts an optional `--imported-only` switch. If specified, the command outputs only those columns that are processed when importing data from Castor into PEP. These are the column names that can be passed to the `--sp-column` switch of the [`castor list-import-columns`](#castor-list-import-columns) and [`castor create-import-columns`](#castor-create-import-columns) subcommands.

## get

When the [`pepcli list`](#list) command has produced IDs referring to data, the associated data can be retrieved using the `pepcli get` command:

```plaintext
pepcli get -t <ticket file> -i <identifier> -o <output file>
```

The flags are:

- `-t` The ticket you stored with the `-T` flag of the [`list` command](#list)
- `-i` The identifier you got from [`pepcli list`](#list)
- `-o` The file to write the output to. `-` indicates [stdout](https://en.wikipedia.org/wiki/Standard_streams#Standard_output_(stdout)). This is the default.

Note that the `pepcli get` command is not capable of data re-pseudonymisation. Data from columns requiring such processing should be retrieved using [`pepcli pull`](#pull) instead.

## list

Use the `pepcli list` command to determine which data is available in PEP:

```plaintext
pepcli list -C <column group> -P <participant group> -T <ticket out file>
```

The command outputs its information as a JSON array with one entry per subject. Every such entry contains the subject's polymorphic pseudonym, plus an array listing the columns in which data is stored for the subject. Depending on switches passed to the command, small data may be _inlined_, i.e. included in the command's output. For larger entries, the output will include an ID instead of the data itself. Such IDs can then be passed to the [`pepcli get` command](#get) to retrieve the associated data.

The available switches and flags for this command are:

| Switch/Flag                                  | Shorthand | Description                                                                 |
|---------------------------------------|-----------|-----------------------------------------------------------------------------|
| `--column-group <value>`       | `-C` | Column group to list data for. Can be repeated if you want data for more than one column group. There is a special column group `*` that contains all columns.                                                      |
| `--column <value>`             | `-c` | Specific column to list data for. Can be repeated, and combined with `-C` if you want multiple columns and column groups.                                                            |
| `--group-output`                      | `-g`       | Data MAY show up grouped, when it belongs to the same participant. By default this depends on the order in which data comes in, so this grouping is not guaranteed. Use `-g` to force grouping of data. This may impact performance.                                            |
| `--inline-data-size-limit <value>`    | `-s` | The size limit (in bytes) for data that should be inlined, i.e. be included the the `list` command's output. Currently defaults to 1000. Setting this to 0 means that data will ALWAYS be inlined. |
| `--local-pseudonyms`                  | `-l`       | Include the local pseudonyms in the output. By default pepcli will only show polymorphic pseudonyms (PP). These are not constant, and cannot be used to see whether data belongs to the same participant. You need the local pseudonyms (LP) for that.                                       |
| `--metadata`                          | `-m`       | Print metadata. It may contain encrypted entries when only an ID was returned for the file in question; apply `pepcli get` to the ID to get the decrypted entries. An attempt to inline data is always performed, if a usergroup only has read-meta access `--no-inline-data` may be needed to access only the metadata |
| `--no-inline-data`                    |           | Never retrieve data inline; only return IDs.                                 |
| `--participant-group <value>`  | `-P` | Participant group to list data for. Can be repeated if you want data for more than one participant group. There is a special participant group `*` that contains all participants.                                                 |
| `--participant <value>`        | `-p` | Specific participant to list data for. Can be repeated, and combined with `-P` if you want multiple participants and participant groups.                                                       |
| `--short-pseudonym <value>`    | `--sp` | Short pseudonyms of participants to query.                                   |
| `--ticket <value>`                    | `-t` | You can pass a ticket from an earlier request with the `-t` flag. The column(group)s and participant(group)s of this request must be a subset of the earlier request.                                              |
| `--ticket-out <value>`                | `-T` | The first thing PEP does when you interact with it, is checking whether you have access to the participant(group)s and column(group)s you request. If you do have access, it will hand out a ticket. You can store this ticket with the `-T` flag, to use it for later actions.                                  |

Note that the `pepcli list` command is not capable of data re-pseudonymisation. Data from columns requiring such processing should be retrieved using [`pepcli pull`](#pull) instead.

## pull

The `pull` command downloads a data set from PEP and stores the data in files. If you need more fine-grained control, use the [`list`](#list) and [`get`](#get) commands instead.

```plaintext
pepcli pull --all-accessible
pepcli pull -C <column group> -P <participant group>
```

This will by default store the data to the directory `pulled-data`.

The available switches and flags for this command are:

| Switch/Flag                                    | Shorthand  | Description                                                                 |
|--------------------------------------------|--------|-----------------------------------------------------------------------------|
| `--all-accessible`                         |        | Download all data to which the currently enrolled usergroup has access to. You can use this instead of the `-c`, `-C`, `-p` and `-P` flags, and will just give you everything that you have been granted access to.                 |
| `--column-group <value>`                  | `-C`   | Column group to download data for. Can be repeated if you want data for more than one column group. There is a special column group `*` that contains all columns.                                                       |
| `--column <value>`                        | `-c`   | Specific column to download data for. Can be repeated, and combined with `-C` if you want multiple columns and column groups.                                                            |
| `--export <value>`                  |      | Export the downloaded data to the format specified as `<value>`. Valid values are `csv` and `json`. Can be repeated if you want to export to multiple formats.                       |
| `--force`                                  | `-f`   | Overwrite or remove existing data in output directory.                       |
| `--output-directory <value>`               | `-o`   | Directory to write files to. The default directory is `pulled-data`.                  |
| `--participant-group <value>`             | `-P`   | Participant group to download data for. Can be repeated if you want data for more than one participant group. There is a special participant group `*` that contains all participants.                                                  |
| `--participant <value>`                   | `-p`   | Specific participant identifier or pseudonym to download data for. Can be repeated if you want data for more than one participant.                                                        |
| `--report-progress`                        |        | Produce progress status update messages.                                            |
| `--resume`                                 | `-r`   | Resume an interrupted download from the temporary folder.                                 |
| `--short-pseudonym <value>`               | `--sp` | Short pseudonyms of participants to query.                                   |
| `--update`                                 | `-u`   | Updates an existing output directory, e.g. when new data is available. This will use the same participant(group)s and column(group)s as the original download, so `-c`, `-C`, `-p` and `-P` are not allowed with this flag.                                      |
| `--suppress-file-extensions`               |        | Don't apply file extensions to downloaded files. This could be useful for automated parsing, as file names will be known in advance (namely, the column name), without needing to parse the metadata.                            |

## export

The export command takes data from a pulled data directory and converts it into a simple file format like CSV or JSON,
so that it can be viewed or used by other tools.

```plaintext
Usage: pepcli export [generic options...] <format> [format-specific options...]
```

The simplest way to use the command is `pepcli export <format>`. This reads data from the pulled-data folder in your
current location and saves the result as a file named export, placed next to that folder. The file’s extension depends
on the format you choose (like .csv or .json).

We currently support two formats: `csv` and `json`

### generic options

| Switch/Flag                                | Shorthand  | Description                                                                 |
|--------------------------------------------|--------|-----------------------------------------------------------------------------|
| `--file-reference-postfix <value>`         |        | Columns containing references to  external files get this postfix. Value defaults to `(file ref)`  |
| `--file-reference-style <value>`           |        | How paths to external files ar represented in the output. Value must be one of: `uri`, `absolute`, `relative-to-output`, `relative-to-input`. Value defaults to `relative-to-input` |
| `--force`                                  | -f     | Overwrite existing files |
| `--from <value>`                           |        | Directory with pull results to use as input (relative to current working directory). Value defaults to `pulled-data` |
| `--help`                                   | -h     | Produce command line help and exit |
| `--max-inline-size <value>`                |        | Files larger than this many bytes are not inlined. Value defaults to `100` |
| `--no-auto-extension`                      |        | Disables automatic addition of an output-file extension |
| `--output-file <value>`                    | -o     | File to write the export results to (relative to current working  directory). Value defaults to `export` |

### csv-specific options

| Switch/Flag                                | Shorthand  | Description                                                                 |
|--------------------------------------------|--------|-----------------------------------------------------------------------------|
| `--delimiter <value>`                      |        | Delimiter used to separate fields in the CSV file. Value must be one of: `comma`, `semicolon`, `tab`. Value defaults to `comma`.

### json-specific options

There are no format-specific switches or flags for JSON.

### examples

```plaintext
# First use `pepcli pull` to retrieve data from PEP. The export requires that
# the data is already stored somewhere on your machine.
> pepcli pull --all-accessible
...

# This creates a directory `pulled-data` in the current working directory.
# The default output location of `pepcli pull` matches the default input
# location of `pepcli export`, so to export this data, we only need to specify
# the desired format:
> pepcli export csv
export.csv

# The command exited succesfully and told us that it created a `export.csv` file
# in the current directory. Most spreadsheet editors will be able to open this
# without issues.
#
# What if we have a very picky editor that requires that the CSV values are
# separated by tabs instead of commas? # We can fix that by repeating just the
# export command, passing `--force` (generic) and `--delimiter` (CSV).
> pepcli export --force csv --delimiter "tab"
export.csv
```

### json output

The JSON output follows this example:

```json
{
  "data": [
    {
      "id":"GUMU0001",
      "visit1 (file ref)":"/path/to/GUMU0001/visit1.data",
      "visit2 (file ref)":"/path/to/GUMU0001/visit2.data",
      "favoriteColor":"red"
    },
    {
      "id":"GUMU002",
      "visit1 (file ref)":"/path/to/GUMU0002/visit1.data",
      "visit2 (file ref)":"",
      "favoriteColor":"blue"
    },
    {
      "id":"GUMU003",
      "visit1 (file ref)":"/path/to/GUMU0003/visit1.data",
      "visit2 (file ref)":"/path/to/GUMU0003/visit2.data",
      "favoriteColor":"blue"
    }
  ],
  "metadata": {
    "header": ["id", "visit1 (file ref)", "visit2 (file ref)", "favoriteColor", "age"]
  }
}
```

- `data`: data that comes directly from the pull directory, as an array of objects.
        Each object represents a data-subject.
- `metadata.header`: lists all columns as an array of strings
- missing data is represented as an empty string; there is currently no way to
  distinguish missing values from empty values in exported data.


## query

Use the `pepcli query` command to retrieve information about your access to the system. Note that such access depends on the access group for which you are enrolled. Enroll for a specific access group:

- either by means of a prior call to `pepLogon`,
- or by [passing an appropriate OAuth token](#general-flags) to `pepcli`.

### query column-access

The `column-access` subcommand lists the columns and column groups that are accessible to you.

```plaintext
pepcli query column-access
```

The output will include all accessible columns and column groups, as well as whether you have read and/or write access.

### query participant-group-access

The `participant-group-access` subcommand lists the columns and column groups that are accessible to you.

```plaintext
pepcli query participant-group-access
```

The output will include whether you have `access` and/or `enumerate` privileges for each participant group.

- the `access` privilege allows you to retrieve data from rows included in the group.
- the `enumerate` privilege allows you to list the rows included in the group.

Most actions require the user to hold both privileges. E.g. when specifying a `-P groupname` to [`pepcli pull`](#pull), you'll want to list the rows included in the group _and_ retrieve data from them.

Members of the `Data Administrator` user group automatically have full access to all participant groups.

### query enrollment

Use the `enrollment` subcommand to find out how you're [enrolled](access-control.md#enrollment) into the PEP system.

```plaintext
pepcli query enrollment
```

The output will include your user name (ID) and the user group to which you belong. The command will produce an error if you haven't enrolled yet, or if your enrollment has expired.

## store

You can store data with this command:

```plaintext
pepcli store -c <column name> -p <participant> -i /PATH/TO/DATA/FILE
```

This will output the identifier of the stored entry.

The flags are:

- `-c` The column to store the data in
- `-p` This is either the participant identifer, or the polymorphic pseudonym to store data for. PPs can be obtained with `pepcli list`.
- `-i` Path to the file to store. `-` means stdin, and is the default.
- `-d` Data to store. Use either this or `-i`
- `-T` By default, pepcli will request a write-only ticket. You can use `-T` and give a path to store the ticket in. If you use this flag, pepcli will also request read access to the entry that is stored. You can then use the ID in the output, together with this ticket for `pepcli get`. This way you can check whether the data was stored correctly. Note that pepcli also performs its own checks to see whether the data was stored correctly.

## Specific usage scenarios

### Uploading and downloading data

See: [Uploading and downloading data](uploading-and-downloading-data.md).

### Data administration

#### Creating participant groups, based on certain attributes

Lets say we want to create a participant group `males`, which contains all male participants. The sex of a participant can be found in a column `Castor.GeneralInfo`.

1. We start by creating the participant group:

  ```plaintext
  pepcli ama group create males
  ```

2. We then download the data for the column `Castor.GeneralInfo`, for all participants:

  ```plaintext
  pepcli pull -c "Castor.GeneralInfo" -P "*"
  ```

3. The data administrator now filters the downloaded data: They take note of the directory names of those participants that, according to the downloaded data, are male. How this is done exactly, is outside of the scope of PEP. The result is a list of directory names. These directory names are the local pseudonyms of the participants.
4. We can now use this list to add participants to the participant group:

  ```plaintext
  pepcli ama group addTo males <local pseudonym>
  ```

For each `<local pseudonym>` from the list of step 3.
