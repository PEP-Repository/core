# PEP Command Line interface

=========================

Before the commandline tool can be used, its environment has
to be configured and it has to be issued an authorization.

Configuration
-------------

`pepcli` requires various configuration files of which
`ClientConfig.json` is the most important.  Configuration includes
addresses of the various servers, TLS certificate authority public keys,
access tokens, private keys, pseudonym public key, data public key and
other cryptographic settings.

By default `pepcli` will look for configuration files in the executable's
own directory. Specify the `--client-working-directory` flag to use
configuration from another directory.

Authorization
-------------

Authorization proceeds in two steps.  First one needs an
access token, which is confusingly called an **oauth token**.
This token can either be retrieved via an oauth dance from the
authentication server, or a long-term oauth token can be provided.

In the second step `pepcli` uses this oauth token to start a short-term
session in a process called **enrollment**.  In the enrollment process
a signing keypair, pseudonym keypair and data keypair are generated
and stored in `ClientKeys.json`.  These keys are typically valid for
about a day.

By default `pepcli` will look for `ClientKeys.json` (which contains
the short-term session keys) in the configuration directory and checks
whether they expired.  If it can’t find them or if they expired,
`pepcli` will look for `OAuthToken.json` in the configuration directory
for an oauth token.  With this token it will automatically enroll to
generate short-term keys.  The oauth token can also be specified on
the commandline with `--oauth-token`.

OAuth Tokens are generated using a shared secret between the
authentication server and the key server, called an **oauth token
secret**.  If this secret is available (which is the case in the
local development environment where it is stored in the `OAuthTokenSecret.json`
file), then `pepcli` will generate an oauth token using the shared secret.

PEP data structure recap
------------------------

The data stored in PEP can be seen as one big table, where there is a row
for each participant. Examples of columns are

* `ParticipantInfo` contains the personalia of a participant.
* `DeviceHistory` contains the history of watches used by the participant.
* `WatchData.Week23` contains watch data of the 23rd week.

A cell has at most one **verified & valid** value.  A value (valid or not) is referred
to as a **file**. Besides the actual data, a **file** has the following metadata
associated to it

* Whether the file is **valid**.  A file is marked invalid if there turns out to
  be an error in the data due to for instance measuring errors.
* Whether the file is **verified**.  When researchers upload data, it's initially
  marked as unverified and verified by data administrator.
* A storage facility **identifier**
* A **timestamp**

### Column groups and (participant) groups

Columns are sorted in **column groups**.  An example of a **column group**
is `ShortPseudonyms`.  A column might appear in several different column groups.

Participants are ordered in **(participant) groups**.  A participant might
appear in several different groups (or none at all).  An example of a group
is `*`, which contains all participants that have consented.

Access is given to full column groups and participant groups --- not to separate
columns or participants.

### Polymorphic pseudonyms

The main "identifier" used to point to a participant is a
**polymorphic pseudonym**.  By design, there are many different polymorphic
pseudonyms that point to the same participant.  Given two polymorphic pseudonyms,
it's intentionally infeasable to check whether they point to the same participant.
In this way, two research groups cannot link pieces of data of the same
participant (if they don't share any column in common).

### Tickets

When querying or retrieving data, the client requests a **ticket** from the
access manager.  A ticket serves two roles:

- It is a signed statement by the access manager that the bearer has access
  to the given column (grous), participant (groups) with the given modes.
- It contains a timestamp and is a **snapshot** in the following sense.
  When retrieving data using a ticket, only data that was verified and valid at
  the time the ticket was issued, is returned.

There are two reasons to temporarily store tickets

- When retrieving multiple files, it's convenient to use the same ticket to
  guarantee is a consistent picture.  (This is the snapshot role of a ticket.)
- A ticket is relatively expensive to create.  The time to create a ticket is
  proportional to the number of matched participants.

`list` — listing files
----------------------

With `pepcli list` one can query the currently valid and verified files
for certain participant (`-p`), (participant) groups (`-P`), columns (`-c`)
and column groups (`-C`).  For each matched file, either the data
is included (if it's not too large) or a identifier is included
with which the data can be queried (with `pepcli get`).

(These identifiers are polymorphic: a single file has *many* different
identifiers and you will get a different one on every request.)

What follows is an example query.

```shell
$ pepcli list -P '*' -c DeviceHistory -C WatchData -T ticket
[{
    "data": {
        "DeviceHistory": "{\n    \"entries\": [\n        {\n            \"type\": \"start\",\n            \"serial\": \"4545454545454545\",\n            \"date\": \"1539085557696\"\n        },\n        {\n            \"type\": \"stop\",\n            \"serial\": \"4545454545454545\",\n            \"date\": \"1539085559216\"\n        },\n        {\n            \"type\": \"start\",\n            \"serial\": \"7777777777777777\",\n            \"date\": \"1539085563332\"\n        }\n    ]\n}\n",
    },
    "ids": {
        "WatchData.Week23": "0A0F4DE8CA173079056F6FBA2CEAC00655121036C7CE0842144E4BC62F3922381A57501A10C6E41E3BBF84738DA9D39EA63F1DDD8B"
        "WatchData.Week22": "0A0F7AA6D4C68A4B7DCE44D813717E31051210F0F483A6C75B30FEDEBBCE013F07ABCF1A10C8FB326728DB027C2B705FB3FFCBC081"
    },
    "pp": "1AD0CB2F25F3F96232F26083B211AF47BA3546A627E2B07E801084121A6C2330:9E554CFCC4E7724AE565BB3120EBB0EC6EC743F2AA314AB585F4EEDB666B1C72:CAB97D7E594B0DCA79BDFBDB090B08C153E761CF964CDA32D829BB746934B34B"
}
,{
    "data": {
        "DeviceHistory": "{\n    \"entries\": [\n        {\n            \"type\": \"start\",\n            \"serial\": \"1212121212121212\",\n            \"date\": \"1539085521343\"\n        },\n        {\n            \"type\": \"stop\",\n            \"serial\": \"1212121212121212\",\n            \"date\": \"1539085524948\"\n        },\n        {\n            \"type\": \"start\",\n            \"serial\": \"1212121212121212\",\n            \"date\": \"1539085529762\"\n        },\n        {\n            \"type\": \"stop\",\n            \"serial\": \"1212121212121212\",\n            \"date\": \"1539085534882\"\n        },\n        {\n            \"type\": \"start\",\n            \"serial\": \"3434343434343434\",\n            \"date\": \"1539085538277\"\n        }\n    ]\n}\n"
    },
    "pp": "BEB675E48D18F039FCCBE8B8FCEA1D9F82CA17797C473EB44E7AA86D22CC4948:A6EB38130C6A6ECBFCC2B35632E105E67CC6C1A0380FBF9EB73010A44CA34673:CAB97D7E594B0DCA79BDFBDB090B08C153E761CF964CDA32D829BB746934B34B"
}
]
```

To retrieve the watch data of the first participant, we need a follow-up
query and therefore we ask `pepcli` to store the ticket in `ticket` with
the `-T flag`.

The limit on the size of the inlined data can be controlled with
the `--inline-data-size-limit` flag.  It can be completely disabled
with the `--no-inline-data` flag.


`get` — retrieve a file
-----------------------

With `pepcli get` one retrieves a file by identifier and ticket. To retrieve
the watch data of the 23rd week in the previous example, one would run:

```shell
$ pepcli get -i 0A0F4DE8CA173079056F6FBA2CEAC00655121036C7CE0842144E4BC62F3922381A57501A10C6E41E3BBF84738DA9D39EA63F1DDD8B -t ticket 
[ watch data written to stdout ]
```

`store` — store a file
----------------------

`pepcli store` stores a file for the given participant under the given column.
If permitted, it will automatically verify the file as well.

For instance, the following command stores watchdata from the file `watchdata`
for the 24th week under the first participant in the `pepcli list` example.

```shell
$ pepcli store -p 1AD0CB2F25F3F96232F26083B211AF47BA3546A627E2B07E801084121A6C2330:9E554CFCC4E7724AE565BB3120EBB0EC6EC743F2AA314AB585F4EEDB666B1C72:CAB97D7E594B0DCA79BDFBDB090B08C153E761CF964CDA32D829BB746934B34B -c WatchData.Week24 -i watchdata
{
    "id": "0A0EB5BB6A7870339EC5C5933ACF66501210A65D1571B0FB6C8B43942861383170561A10CC08783286546545C66D4A4B8728ED5C"
}
```

To retrieve the stored data immediately without going through `pepcli list`,
one can instruct `pepcli store` with the `--ticket-out` option to request
a ticket with read/write permissions and store it in the give file
(which can then be passed to `pepcli get` with the `-t` option.)
