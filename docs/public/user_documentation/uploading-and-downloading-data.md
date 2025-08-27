# Uploading and downloading data

PEP's primary function is the storage of (pseudonymized) data. Most users will therefore access the system to either upload data into it, or to download data from it. This page describes how to do so using the `pepcli` command line utility. Extensively on a [separate page](using-pepcli.md), this page provides some additional information and examples on

- [uploading data](#uploading-data) using the `pepcli store` command, and
- [downloading data](#downloading-data) using the `pepcli pull` command.

The documentation on this page assumes that the user is familiar with PEP's [data structure](data-structure.md). The examples are based on the use of a Unix-like environment; users running `pepcli` on different platforms should adapt their command lines accordingly. Additionally, users are expected to pass command line switches [appropriate for their situation](using-pepcli.md#general-usage), and to [ensure that they are enrolled](using-pepcli.md#enrollment) with the PEP system. For brevity, the examples on this page do not include such switches.

## Limitations

PEP can only store a single file in any given cell. If multiple files are to be distributed together (e.g. because one is unusable without another), they should be stored in multiple columns, and all those columns should be made available to downloaders. Alternatively, uploaders can package files into an archive (e.g. using the `tar` utility) and upload that archive to a single PEP column. Downloaders would then need to unpack the archive before they can analyze the original file contents.

Additionally, PEP does not (by default) perform any processing on the data it stores. The consequence is that downloaders will receive the exact same data that the uploader stored. Uploaders should therefore ensure that their data is stripped of information unsuitable for dissemination. This includes any fixed identifiers that the data may contain, since those could be used to blend the downloads from different access groups.

PEP eases some of these limitations with its built-in support for some data formats. See the [section on that topic](#data-format-processing) for details.

## Uploading data

Data can be uploaded into the PEP system by means of [the `pepcli store` command](using-pepcli.md#store). In its most basic form, the command is used to upload a file's contents to a specific cell, specifying a participant identifier and a column name. E.g.:

```shell
/app/pepcli store -p POM162733743006 -c Holter.Visit1 -i ~/holter/49170044-1C98-4A21-886C-A6E18DE1F885.ecg
```

Because of PEP's [pseudonymisation](pseudonymisation.md) goals, uploaders will often not be privy to a participant identifier. Instead they'll usually have a *short pseudonym*, i.e. an identifier for the data sample. The `pepcli store` command is therefore more commonly invoked using the `--sp` switch instead of the `-p` switch. E.g.:

```shell
/app/pepcli store --sp POM1EC7713461 -c Holter.Visit1 -i ~/holter/49170044-1C98-4A21-886C-A6E18DE1F885.ecg
```

Instead of reading data from a file, the `pepcli store` command can also process data provided over the `stdin` standard input stream. This feature allows it to be included in command redirection pipelines. The use of `stdin` is denoted by omitting the `-i` switch, or by passing a hyphen `-` to it. E.g.:

```shell
curl https://ecg.ppth.com/49170044-1C98-4A21-886C-A6E18DE1F885/ | /app/pepcli store --sp POM1EC7713461 -c Holter.Visit1
```

The `pepcli` utility exits with code `0` (zero) if the `store` command succeeds, or with a different value if it fails. Automated (scripted) invocations of `pepcli store` should check for these values. When the command fails, the utility writes details to the log and/or to the standard error stream `stderr`.

Note that the PEP system can only accommodate a single file in any given cell. If multiple files should be stored together (e.g. because the files would be useless individually, or because the data format requires it), the files must be packaged into a single file (e.g. a `tar` archive) before being `pepcli store`d. Downloaders will then receive the packaged file, which they should process accordingly.

PEP provides built-in processing facilities for some data formats. Special considerations may apply when uploading data to affected columns. See the [section on that topic](#data-format-processing) for details.

## Deleting data

It is also possible to delete data using `pepcli`. Note that data is never truly deleted, since PEP keeps a version history, and this history is also kept for a file that is deleted. But the data will no longer be visible in new downloads from PEP.

The `pepcli delete` command works very similar to the `pepcli store` command. The only difference is that it does not get any input data. We can for example delete data using:

```shell
/app/pepcli delete -p POM162733743006 -c Holter.Visit1
```

Or by using a short pseudonym:

```shell
/app/pepcli delete --sp POM1EC7713461 -c Holter.Visit1
```

## Downloading data

Users commonly invoke [the `pepcli pull` command](using-pepcli.md#pull) to download data from PEP. In its most basic form, the command is invoked to download all data the user has access to:

```shell
/app/pepcli pull --all-accessible
```

You can also download data for a specific participant (identifier) and column (name), e.g.:

```shell
/app/pepcli pull -p POM162733743006 -c Holter.Visit1
```

Data for multiple participants and/or columns can be downloaded in one fell swoop by specifying corresponding switches multiple times. E.g.:

```shell
/app/pepcli pull -p POM162733743006 -p POM390792184872 -p 834735705658 -c Holter.Visit1 -c Holter.Visit2 -c IsTestParticipant
```

The command also allows participant groups (as opposed to individual participant IDs) and column groups (as opposed to individual column names) to be specified. Group-denoting switches have uppercase letters, can also be specified multiple times, and can be combined with individual specifications. E.g.:

```shell
/app/pepcli pull -P all-pit -P all-denovo -C DeNovoWatchData -C Castor -c IsTestParticipant
```

Whatever data is downloaded, the `pepcli pull` command writes it to a directory tree with the following structure:

- A top level directory for the download, containing:
  - One subdirectory per data subject (named after the subject's [local pseudonym](pseudonymisation.md#identifiers-in-pep)), containing:
    - One file per cell downloaded for this subject. The file is named after the column.

By default the top level directory is named `pulled-data` and placed into the current working directory. This behavior can be overridden by means of the `-o` switch, e.g.:

```shell
/app/pepcli pull -o ~/analyze/todays-data -P all-pit -P all-denovo -C DeNovoWatchData -C Castor -c IsTestParticipant
```

When users have previously downloaded data from PEP, they can use the `--update` switch to retrieve any updates from PEP. Upon the command's completion, their local directory will once again match the data currently stored in PEP, e.g. having added files that were uploaded after the initial data was downloaded. Data will be updated for all participants and columns that were included in the original download, so no participant(group)s and/or column(group)s should be specified when using the `--update` switch. E.g.:

```shell
/app/pepcli pull -o ~/analyze/todays-data --update
```

To prevent data loss, the `pepcli pull` command will be aborted

- when performing a regular (initial) download: if the output directory already exists.
- when performing an update: if directory and/or file contents have been altered after the initial download.

Use the `--force` switch to have the command (discard/overwrite local data and) proceed anyway.

Use the `--report-progress` switch to have the command show progress updates.

PEP provides built-in processing facilities for some data formats. Special considerations may apply when downloading data from affected columns. See the [section on that topic](#data-format-processing) for details.

### Manually `list`ing and `get`ting data

Like its `pull` command, the `pepcli` utility's `list` and `get` commands allow data to be downloaded from PEP. But although they provide more fine-grained control over the download process, they do not provide PEP's built-in support for [data format processing](#data-format-processing), making them unusable for certain types of data. **Use of these commands is therefore strongly discouraged.** They are retained only for backward compatibility purposes, and may be removed from future versions of PEP.

## Data format processing

PEP has the ability to perform special processing for some data formats that would otherwise be cumbersome to distribute. The PEP team can configure appropriate columns to enable PEP's support for these formats.

### MRI data in the BIDS format

MRI data is commonly stored in the [BIDS format](https://bids.neuroimaging.io/specification.html). Raw data in this format is not suitable for storage into PEP because:

- it consists of multiple files, and
- some files may contain (short pseudonym) identifiers that should be [pseudonymized](pseudonymisation.md), and
- individual files may contain data on multiple subjects.

PEP provides built-in facilities to address the first two issues, but not the third. PEP must be properly configured to enable BIDS support for specific columns. Contact the PEP team to have such configuration applied.

#### Uploading BIDS data

To store BIDS data into PEP, uploaders themselves should ensure that a BIDS data set contains information on a single subject, for example using (third-party) tooling to split an existing multi-subject data set into multiple sets for individual subjects. Once each subject's BIDS data has been placed into a separate directory, uploaders invoke the `pepcli store` command, using the `--input-path` (or `-i`) switch to specify (the path to) that directory, e.g.

```shell
/app/pepcli store --sp POM1MR3956833 -c Visit1.MRI.Anat --i ~/mri/anat/5EEDECA6-B70D-11EB-8529-0242AC130003
```

PEP will (look up and) replace the participant's short pseudonym by a placeholder, then put the entire directory's contents into a `tar` archive and store that into the specified column. Downloaders of the column's data should [ensure](#downloading-bids-data) that they (have PEP) apply appropriate post-processing.

#### Downloading BIDS data

BIDS data can be downloaded by means of the `pepcli pull` command by just including it in the set of columns, or by specifying a column group containing the column. E.g.:

```shell
/app/pepcli pull -P all-ppp -c Visit1.MRI.Anat -c IsTestParticipant
```

While data are usually downloaded onto the local file system as a single file, columns configured with BIDS support will produce a *directory* instead. The directory will be named after the column (e.g. `Visit1.MRI.Anat` in the example) and filled with the files that were [originally uploaded](#uploading-bids-data). Within those files, however, any values originally containing the MRI data's short pseudonym will have been replaced by the [user pseudonym](pseudonymisation.md#identifiers-in-pep) for that participant. The user pseudonym can then be used as a identifier for the subject (and/or for the subject's MRI data) during further processing. It should be noted that

- a substring of the user pseudonym will be used, matching the length of the original short pseudonym.
- if downloaders wish to join MRI data for multiple subjects into a single BIDS data set, they must do so themselves (e.g. using third-party tooling).
- the `pepcli get` command provides no support for the BIDS format, so its use will produce the originally uploaded `tar` archive, containing placeholder values instead of user pseudonyms.
