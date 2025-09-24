# Data structure

PEP allows for the storage and retrieval of tabular data. Conceptually, PEP provides a single table consisting of rows and columns, e.g.:

| Name    | HatStyle       | BankAccountNr      | LastDoctorVisit | BloodPressure | ... |
| ------- | -------------- | ------------------ | --------------- | ------------- | --- |
| Scrooge | top hat        | NL50ABNA3690200148 | 1843-12-19      | 131/77        | ... |
| Donald  | sailor cap     | *&lt;none&gt;*     | 2021-01-06      | 141/76        | ... |
| Ariel   | *&lt;none&gt;* | DK3650519625773963 | 1989-11-17      | 119/64        | ... |
| Eric    | crown          | DK3650519625773963 | 2013-11-19      | 122/62        | ... |
| ...     | ...            | ...                | ...             | ...           | ... |

Each row represents a single entity or data subject. Data are stored in the same row if they are associated with the same subject. Data for different subjects should be stored in separate rows. Rows are denoted by means of one of [PEP's identifiers](pseudonymisation.md#identifiers-in-pep). A new row is created by storing data into a row with a previously unused [participant identifier](pseudonymisation.md#participant-identifier).

Columns are used to split data into conceptual units. Different pieces of data should be stored in different columns. Columns are referred to by name, and the name is determined when the column is created. Only members of the `Data Administrator` role can [create columns](using-pepcli.md#ama-column) and perform other [administrative tasks](using-pepcli.md#ama) on them.

A major difference with traditional data(base) storage systems is that no separate column should be used to store a [fixed row identifier](pseudonymisation.md#traditional-fixed-identifiers). Rows are instead referred to by means of PEP's [polymorphic identifiers](pseudonymisation.md#identifiers-in-pep).

The intersection of a row and column is called a cell. Cells do not have an identifier of their own. Instead, when storing or retrieving data, users can identify cells by specifying the associated column name and row identifier.

Access cannot be [managed](access-control.md#data-access) at the cell level: rows and columns are the smallest units for which access can be granted or revoked. Data Administrator should take this into account in their column management. If (at some point) data are to be disseminated separately, they must be stored in separate cells. Consequently, separate columns must be used to store these data.

## Retention

After data has been stored into a PEP cell, downloaders can retrieve that data from there. When new data are stored into the same cell, new downloads will receive the updated version instead. But the old data are never discarded: PEP retains a complete record of all data that has ever been stored into the system. This allows PEP to reconstruct its data set as it was at any time in the past. Such "snapshots" are intended to be made accessible to users for download (although the functionality has not yet been created). This allows the exact same data to be retrieved multiple times, which is usable e.g. for scientific replication studies.

A similar policy applies to column management. When a Data Administrator removes a column, the data stored in that column is retained for future use. Therefore (once the feature is available) when users retrieve an older snapshot, they will also receive the data from the "removed" column. Data Administrators should be aware that, if they remove and then re-add a column with the same name, the newly created column will immediately contain the previously stored data.

## Grouping

Members of the `Data Administrator` role can group

- [columns into column groups](using-pepcli.md#ama-column), and
- rows into participant groups.

Such groups serve as a basis for [data access management](access-control.md#data-access). For example, a `MedicalInfo` column group might contain the `LastDoctorVisit` and `BloodPressure` columns, and the rows for `Scrooge` and `Donald` might be included in a participant group called `Ducks`. An Access Administrator can then grant certain user(group)s access to `MedicalData` and to the participants classified as `Ducks`. Such users are then authorized to access all `MedicalData` for all `Ducks` stored in PEP.

PEP provides a number of predefined column groups and participant groups. Their names should be considered reserved words, i.e. not be (re)used for other purposes. The only predefined participant group is named `*` and contains all rows. Newly added rows are automatically added to this participant group. Predefined column groups and their purposes are listed in the following table:

| Name | Contains | Content updates |
| ---- | -------- | ------- |
| `*` | All columns<!--- @@@ why does this group exist? No one should have access (except perhaps Data Administrator) @@@ ---> | Automatically kept up to date. |
| `Castor` | Columns storing data [imported](castor-integration.md#import) from the [Castor electronic data capture (EDC) system](https://www.castoredc.com/electronic-data-capture-system/). | Managed by Data Administrator. |
| `CastorShortPseudonyms` | Columns storing [short pseudonyms](data-structure.md#short-pseudonyms) that refer to Castor EDC records. | Automatically kept synchronized with environment configuration. |
| `Device` | Columns storing (wearable) device registration histories. | Automatically kept synchronized with environment configuration. |
| `ShortPseudonyms` | Columns storing [short pseudonyms](data-structure.md#short-pseudonyms). | Automatically kept synchronized with environment configuration. |
| `VisitAssessors` | Columns storing identifiers for the assessors that administered a(n academic study's) participant measurement session (i.e. visit to the research center). | Automatically kept synchronized with environment configuration. |
| `WatchData` | Columns containing data collected by wearable devices | Managed by Data Administrator. |

## Step by step

PEP's data structure depends on the actions of different parties:

- Column management is performed by different parties, depending on the specific column:
  - Some of PEP's columns are predefined and automatically created when a PEP environment is provisioned.
  - The PEP (support) team manages an environment's basic configuration. Short pseudonym columns are created (and removed) as a result.
  - Data Administrator can create and remove columns by means of [the `pepcli ama column` command](using-pepcli.md#ama-column).
- Rows are managed by users storing data into PEP:
  - When specifying an existing [identifier](pseudonymisation.md#identifiers-in-pep), data are stored in the specified row.
  - When specifying a new (i.e. previously unknown) identifier, data are stored in a newly created row.
- Columns and rows are grouped by different parties:
  - The contents of some column groups depend on environment configuration. These column groups are automatically updated by the PEP system.
  - Other column groups are managed by Data Administrator using the [`pepcli ama columnGroup`](using-pepcli.md#ama-columngroup) and [`pepcli ama column`](using-pepcli.md#ama-column) commands.
  - Row groups are managed by Data Administrator using the [`pepcli ama group`](using-pepcli.md#ama-group).
  