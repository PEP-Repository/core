# Repository Management

## Access Rules

A user is assigned to User-Group following authentication. Access rules (authorizations) apply to **User-Groups** only. Users in a single **User-Group** have the same permissions and they share a pseudonymisation space (i.e., they obtain the same Participant Aliases).  

**Access Rules** are defined as a permission to access data of a named list of participants (**Participant-Group**) and a named list of data-COLUMNS (**Column-Group**). All entries in a single cell in the above data model (i.e., all files that share the same data-COLUMN and PP) share the same access permissions.
An access rule for a Column-Group grants one of four types of privileges (`access modes`):

### Access Modes

* the `read` privilege allows the holder to read data from (cells in columns in) the Column-Group.
* the `read-meta` privilege allows the holder to retrieve information about data in (cells in columns in) the Column-Group, but not to read the data itself. Using this privilege, users can e.g. determine how many cells are nonempty, or when data has been last uploaded into a specific cell. When the "read" privilege is granted, holders automatically receive the "read-meta" privilege as well.
* the `write` privilege allows the holder to write ("upload") data to (cells in columns in) the Column-Group.
* the `write-meta` privilege allows the holder to overwrite (cells in columns in) the Column-Group without uploading new (payload) data. This can be used to retroactively store information (such as e.g. a file extension) for data that has been previously uploaded without such information. When the `write-meta` privilege is granted, holders automatically receive the `write` privilege as well.

Multiple access rules may be defined for the same Column-Group, e.g. to grant both “read” and “write” privileges to a User-Group. All column group privileges apply to all (accessible) cells in all columns included in that Column-Group.

There is one permission in an access rule for Participant-Groups: `access`.
