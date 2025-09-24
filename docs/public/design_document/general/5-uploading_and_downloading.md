# Uploading and downloading data

## Uploading data to the repository

After data production/collection and sanitizing of the data can be uploaded to the PEP repository. A user can upload data using PEPCLI after the `Authorization Context` for the upload has been defined.

There are two cases:

- Data rows are identified by a `Short Pseudonym` (for data extracted from initial sample streams)
- Data rows are identified by a `Participant Alias` (for derived data constructed from data previously downloaded)

The upload using a Participant Alias is the general case. If data are identified by a short pseudonym, the software will look up the Short Pseudonym1 for the corresponding Participant Alias. In this case read access to the Data-COLUMN containing the Short Pseudonym is required

## Requesting data from the repository

A `Data Request` can be performed by a user (using PEPCLI) after the definition process (see table 1) of the `Autorizahtion Context` for the `Data Request` has been completed.  A `Data Request` is an actual request that leads to downloaded and decrypted data, within the boundaries of the user’s `Authorization Context`.  The result of a `Data Request` is a subset of data a `User-Group` is authorized for.

The user can apply restrictions in such a request, to reduce the amount of data received:

- `Subset of participants`:
  - One or more specific participants, identified by a `Participant Alias`, or
  - All participants in one or more `Participant Groups`.
- `Subset of Data-COLUMNS`:
  - zero or more specific data-COLUMNS and/or
  - zero or more `Column-Groups`

Apart from performing a fresh download users can also update earlier requests:

- Update download: Only retrieves the data needed to update an existing download directory to the current server state. Downloads data that has been added or updated since the directory was last updated and deletes local copies of data for cells that have been cleared since the last download.

## Modifying data

At any time, a situation may occur that previously uploaded data are considered obsolete. Data can then be made unavailable. Once made unavailable data will not be included in datasets to which access is granted after this point in time. They will occur though in queries for dates prior to the date that they were made unavailable.

Two cases can be distinguished:

- The contents of a cell are no longer correct, and correct content is not available: the cell is cleared, and the content cannot be downloaded in future `Data Requests` anymore. The deleted content will appear exclusively in `Data Requests` with access to dates prior to the moment of deletion. Clearing a cell requires the `write privilege`.
- The contents of a cell are no longer correct, and a correct version is available. In this case the newer content replaces the invalid content and will appear in `Data Requests` for downloaders with access to dates after the correction occurred. The older, replaced, content will still appear in `Data Requests` for dates prior to the moment of replacement. Rewriting a cell in this way requires the `write privilege`.

Withdrawal of consent by a participant is an example of the first case, where the contents of all Data-COLUMNS for that participant are marked as deleted.

To uphold reproducibility for formerly shared datasets, data is not actually deleted from the servers, but rather made unavailable. A so called 'tombstone' is placed on the cell at a point in time, rendering it empty afterwards.

<!-- NOTES to process:
Jean (12-21-2023): TBD: simple implementation is removal of  a participant from participant groups. As this is prone to (later) error, overwriting of all cells with content for the participant who withdraws consent may be a more secure option. 

-->

## Versioned or Rolling access

In many cases, datasets are not final before the sharing of (parts of the) data commences. Among the reasons for this are:

- The collection phase has not yet ended (if limited at all) before the first subsets of the data are shared.
- Errors in the dataset are corrected after the first sharing of data has started, or erroneous values have been deleted or marked as invalid.
- Data-subjects have withdrawn their consent.
- Data consumers added analyses to the data collection (and by doing so have also become data producers).

Following the functional design of the PEP system, once uploaded, data cannot be deleted from the repository. They may however be marked as invalid, after which they are not available to newly granted research projects or users of PEP (see also [Modifying Data](#modifying-data) ).

Additionally, previously shared data should remain available in the exact same state for data consumers while in the meantime the current (most recent) dataset may alter. Because of this, studies are always reproducible, and data consumers can be directed to delete their source datasets after their analyses, since the data remains available through PEP, thereby reducing the risk of data breaches and orphaned datasets.

User Groups in PEP can have two different kinds of access to data: `Timestamp Bound Data` and `Rolling Data`. Access to `Rolling Data` is particularly relevant for uploaders/data producers, since they likely want to review the data in the current state. Data downloaders on the other hand should generally be granted access to `Timestamp Bound Data`, referring to a snapshot of the data at a specific moment in time (by Access Rules at a specific point in time, see also [Timestamp Bound Access](./4-data_access.md#user-access-parameters-timestamp-bound-access) ), meaning the most recent data at that point in time for an [Authorization Context](./4-data_access.md#authorization-contexts).

### Example 1: fixing errors requires new data release/snapshot

Example: `Timestamp Bound Data` allows projects that use PEP to work with data releases. A data release would in that case be coupled to a point in the past for an authorization context. The data in a data release cannot be altered in that same release. Say an error is found in a released dataset, the only way to correct it would be to create a new data release based on the current data. But the current data may contain more modifications on the previous data release than just the fix of the error. Additional modifications within the Authorization Context of a `User Group` will be part of the new data subset. PEP does support branches in the repository data.

### Example 2: producers (download and upload) may require two UserGroups

This example is described as part of the section [Contributing derived data from an immutable dataset](./4-data_access.md#contributing-derived-data-from-an-immutable-dataset).
