# Distribution of Trust (transcriptors and for eyes principle)

## Encryption

## Planned improvements, addressing residual vulnerabilities

Being able to reprovision the following PEP Server modules could allow access to data. Implementing additional distribution of trust methods could prevent this too:

- Access Manager
- AuthServer (#928)

## (Malicious) methods to link data subjects between user groups

PEP's local pseudonyms prevent the straight forward coupling of minimised datasets based on identifiers. Of course, the entropy of the data might allow to couple records within subsets.

Additionally, when using a modified ('hacked') client, it would be possible to link pseudonymised records from minimised subsets via:

### Polymorphic pseudonyms

Currently, the PEP Repository shares polymorphic pseudonyms (PPs, not to be confused with local pseudonyms that are the default pseudonyms to communicate by within the client) for data subjects with the client. These PPs have a different presentation each time they are generated, but can (currently) be used to point to the same row from a client regardless the UserGroup.

There are designs for PEP Repository improvements that would no longer require to share PPs from data subjects in the repository with PEP clients, these are on the to do list of software improvements.

### AES keys as identifiers

The PEP (elGamal) crypto is relatively expensive to perform in terms of computational processing required. Because of this, larger entries in PEP are encrypted symmetrically using AES, and only the AES key is encrypted with the PEP polymorphic cryto. This cascaded process of decryption is performed by the PEP client. By default, the PEP Repository clients do not present the AES keys to the end user, but this key *is* shared with the client and should thereby be consideren 'information available to the user' since with limited tweaking this AES key can be made available to the user, allowing the AES keys for cells as persistent identifiers among different `User Groups`.

Often, larger cell sized may be more 'identifying' than smaller cell sizes. For example: a 2 byte integer will mostly have a higher chance of not beinig unique within a dataset than a 2 megabyte audio fragment. Such an audio fragment will likely be unique throughout the dataset and could inevitably be used to couple minimised subsets that contain this value. A future version of the PEP Repository should allow the system admin to define a cell size threshold (for example 16 bytes). All cells smaller than this threshold would be encrypted polymorphically and thereby not being able to identify by the encryption key, whereas the AES key vulnerability would exclusively remain for larger cell sizes, a risk that may be considered acceptable.

### Prevent malicious use via (data use) agreements

Linking two datasets via the methods above, would require the use of modified clients with (at least) two legitimate UserGroups. Obviously, data use agreements between data source and secundary use environment, should forbid using modified clients for these purposes.
