# Pseudonymisation

Perhaps the most central and unique feature of the PEP system is that different users receive different row identifiers to refer to the same row. This prevents downloaders from blending their respective data into a single, larger data set. Thus, with its built-in pseudonymisation mechanism, PEP provides some basic privacy safeguards when disseminating sensitive data such as medical or financial information.

## Traditional fixed identifiers

Data storage systems usually assign unique identifiers to the entries they store. For example, (relational) database tables typically include an `Id` column:

| Id  | Name    | HatStyle       | BankAccountNr      | LastDoctorVisit | BloodPressure | ... |
| --- | ------- | -------------- | ------------------ | --------------- | ------------- | --- |
|   1 | Scrooge | top hat        | NL50ABNA3690200148 | 1843-12-19      | 131/77        | ... |
|   2 | Donald  | sailor cap     | *&lt;none&gt;*     | 2021-01-06      | 141/76        | ... |
|   3 | Ariel   | *&lt;none&gt;* | DK3650519625773963 | 1989-11-17      | 119/64        | ... |
|   4 | Eric    | crown          | DK3650519625773963 | 2013-11-19      | 122/62        | ... |
| ... | ...     | ...            | ...                | ...             | ...           | ... |

Identifiers are commonly generated when the entry is first created. Once available, they remain the same as long as the row exists. The identifier is stored *within* the entry and becomes *part of* the data. Anyone with access to the data is usually granted access to the `Id` column as well, simply because it is needed to uniquely identify a record for many data manipulation tasks. The identifier also serves as a pseudonym for the entry it refers to: regardless of access to other data such as `Name`, a subject can be referred by its `Id`.

While a traditional `Id` column thus achieves some form of pseudonymisation, it is a privacy hazard when access to other data is restricted. For example, financial service professionals may be allowed to read the table's `BankAccountNr`, while medical personnel may be granted access to their `LastDoctorVisit`. But since both parties will also have access to the `Id` column, if an accountant and a doctor compare notes, they can build a combined data set on the basis of their common `Id` values. This will provide them with *a combination of* financial and medical information that no one has been granted access to.

Such "data blending" has been the subject of much debate, a.o. in the context of user profiling on the Internet.

## Identifiers in PEP

Instead of assigning fixed identifiers to rows, PEP uses identifiers called "polymorphic pseudonyms" (PPs) that are partially randomized. A new PP value is generated whenever a data entry is accessed, causing different parties to receive different PPs for the same row. Since parties cannot match PPs between their respective data sets, this eliminates a major underpinning of the data blending we're trying to avoid.

A downside of the use of PPs is that a single party would also not be able to associate data that they retrieve at different times. But since the party could create a complete data set by downloading data in one fell swoop (instead of in batches), the PP's volatility provides no security in this scenario. PEP therefore also has the ability to calculate "local pseudonyms" (LPs) for data rows. For any given party, the same row will be assigned the same LP value at all times. Data from multiple downloads by the same party can then be joined by matching LP values. But different parties will receive different LP values to refer to the same row, thus still preventing data received by different parties from being blended.

PEP also supports a derivative from the local pseudonym, called the "user pseudonym" (UP). This is simply an abbreviated form of the local pseudonym, which is sometimes more convenient to use. User pseudonyms provide the same pseudonymisation features as local pseudonyms.

### Participant identifier

Downloaders usually won't need to deal with PEP's [pseudonymous identifiers](#identifiers-in-pep), instead downloading data for "all rows" or a named set of rows. Uploaders, on the other hand, must specify the exact [cell location](data-structure.md) where their data is to be stored, i.e. a combination of a column name and a polymorphous pseudonym (PP).

When a new row is to be created, PEP will not have generated a PP yet, preventing the uploader from being able to specify it. This, in turn, prevents the uploader from creating the row and having PEP generate a PP for it! To resolve this circular dependency, PEP also allows the use of fixed identifiers to refer to rows. Known as participant identifiers, such identifiers can be taken from anywhere, as long as they are unique and sufficiently randomized. An initial uploader can create a new row by specifying a new participant identifier, to which PEP responds by issuing a PP (and other pseudonyms) for the row. Future uploads can then refer to the row by specifying either the PP, or keep using the original participant identifier.

New PEP rows can also be created by means of the `pepAssessor` application's registration feature. It prompts the user for some initial data for the new row, then generates a participant identifier and uses that to create a row and store the entered data. The user can then copy the participant identifier to a different application, e.g. a Salesforce system used for the registration of an academic study's participants.

PEP's generated participant identifiers are alphanumeric and rather short, to allow them to be copied and entered by hand. Additionally, to prevent them from getting lost (perhaps rendering the row inaccessible), generated identifiers are stored into PEP's `ParticipantIdentifier` column and can be listed from there. Needless to say, this information must be kept secret to prevent it from being used as a persistent identifier! Participant identifiers should therefore only be made available to PEP users that are already privy to a complete set of non-pseudonymous data, such as assessors processing a study's participants.

#### Short pseudonyms

Sometimes PEP rows must be associated with data stored outside PEP. (This is e.g. the case with non-digital specimens, such as biosamples taken during medical research.) These external data are often intended to be analyzed together with data stored in PEP, requiring them to be associated with a particular PEP row. But we want to make it impossible to associate different external samples with each other, precluding data blending by means of a common (fixed) identifier.

Although PPs could conceivably be used, they are often too long and unwieldy to be of practical use. For example, PPs are too long to print onto (stickers that fit) blood vials, and they are too long to (conveniently) manually type into different software systems. So instead of PPs, external samples are usually associated with shorter and more readable identifiers. Known as short pseudonyms (SPs), these identifiers are then stored in a separate PEP column. For example, a `ShortPseudonym.Visit1.Blood` column could store the identifier associated with a research subject's blood sample taken during their first visit to the research center.

PEP has the ability to generate SPs for participants registered using the `pepAssessor` application. These generated SPs can then be printed onto stickers that can be affixed to the external (bio)samples.

Once a short pseudonym has been introduced, it can be used to store further data into PEP without divulging a subject's identity. For example, a lab technician might store a value in a PEP column named `Blood.Visit1.PlateletCount` for the participant whose `ShortPseudonym.Visit1.Blood` is equal to what they read on the vial they analyzed. The technician would learn no additional details about the subject's identity, while still being able to contribute to the data set stored in PEP. These data can then be made available to others for further research.

Since SPs uniquely identify a single external sample, they are also usable to uniquely identify a single PEP row. Care should therefore be taken not to expose SPs to (different) access groups, preventing different sets of PEP data from being blended together.
