# Glossary

This page lists key concepts and terminology related to the PEP system, and provides concise descriptions of each term. Follow hyperlinks to find out more.

| Concept | Abbreviation | Description |
| ------- | ------------ | ----------- |
| Access Administrator | AA | The PEP role responsible for [data access management](access-control.md#data-access) and [assignment of users to access groups](access-control.md#role-determination). Users with this role are called "access administrators". Not to be confused with the "Acccess Manager" service. |
| Access group | | A named group of users that can be [authorized](access-control.md) for specific actions. Also called a "role" in the system. |
| Access Manager | AM | The PEP service responsible for [data access authorization](access-control.md#data-access). AM cooperates with the Transcryptor (TS) service to issue data access [tickets](access-control.md#tickets). Not to be confused with the "Access Administrator" role. |
| Application Programming Interface | API | The set of (primitive) operations that a software system provides. Raw API operations are intended for use from programming languages, allowing programs to be written that provide users with a simpler and/or more expressive level of interaction with the system. |
| [authorization](access-control.md#authorization) | | Determining whether a user is allowed to perform certain actions in a software system. |
| [authentication](access-control.md#authentication) | | The act providing a user's identity to a software system. |
| Authentication Server | AS | A server involved in PEP's [access control](access-control.md) mechanism. AS takes an externally provided identity as input and produces an OAuth token as output. |
| blending | | The combination of several data sets into a single, larger data set. |
| [Castor](https://www.castoredc.com/electronic-data-capture-system/) | | An Electronic Data Capturing (EDC) system that PEP [integrates with](castor-integration.md). |
| cell | | The intersection of a column and a row, usable to store a single piece of data associated with a single participant. |
| clinical research form | CRF | A facility for the entry of (clinical research) data on study subjects. CRFs are commonly filled out by assessors during participant visits to a research center. Contrast with surveys. |
| column | | A [data structuring](data-structure.md) primitive associated with a single type of data, or with a single measurement. Columns are referred to by their names. |
| column group | | A named set of columns that can be used as a basis for [data access management](access-control.md#data-access). |
| command line interface | CLI | A facility to interact with computer software by means of textual input and output. Contrast with GUI, which provides an image-based interface. PEP's main CLI utility is called `pepcli`. |
| Data Administrator | DA | The PEP role responsible for [data structuring](data-structure.md) and management. Users with this role are called "data administrators". |
| derived data |  | Data that is generated from (analysis of) data retrieved from PEP. Researchers that create derived data can contribute them back to PEP by uploading them, using the pseudonyms that the researcher received at the time of download. |
| device | | An instrument or apparatus that collects data to be stored by PEP. The term is commonly used to refer to wearable devices (such as smart watches) that collect biometric data. |
| [Docker](https://en.wikipedia.org/wiki/Docker_(software)) | | Virtualization software that allows software to be delivered and executed with a level of isolation that operating systems do not provide. PEP's Linux-based software is distributed in the form of Docker containers. |
| download | | As a verb: the act of retrieving data from PEP. As a noun: a data set that has been retrieved from PEP. |
| Electronic Data Capturing system | EDC | (Third-party) software used to collect and store data, e.g. from academic research study subjects. PEP provides out-of-the-box integration with the [Castor EDC](https://www.castoredc.com/electronic-data-capture-system/). |
| [ElGamal encryption](https://en.wikipedia.org/wiki/ElGamal_encryption) | | The cryptographic algorithm used as a basis for PEP's polymorphism. |
| encryption | | A technique to make information readable only to parties that are in possession of an associated secret. Also see "polymorphic encryption". |
| [enrollment](access-control.md#enrollment) | | Providing a user (or other party) with the cryptographic materials required to perform actions in the PEP system. |
| graphical user interface | GUI | An image-based facility enabling users to perform tasks on a computer. Contrast with CLI, which provides a text-based interface. PEP's main GUI application is `pepAssessor`. |
| identifier | ID | A piece of information that uniquely denotes a single piece of data. The polymorphic pseudonym (PP) is PEP's primary [identifier type for rows](pseudonymisation.md#identifiers-in-pep). |
| Key Server | KS | A server involved in PEP's [access control](access-control.md) mechanism. KS takes an OAuth token as input and produces enrollment data as output. |
| local pseudonym | LP | A row identifier that is unique to an access group. Different access groups use different local pseudonyms to refer to the same row. |
| OAuth | | A technical standard related to [access control](access-control.md). PEP's OAuth tokens allow parties to [enroll](access-control.md#enrollment) with the system. |
| participant | | A subject in an academic research study. The term is commonly used in PEP software and documentation as a synonym for a data row. |
| participant group | | A named set of rows that can be used as a basis for [data access management](access-control.md#data-access). |
| [participant identifier](pseudonymisation.md#participant-identifier) | | A fixed identifier associated with a single PEP row. Participant identifiers are (a.o.) used to create new rows in PEP. |
| pepAssessor | | A GUI application used for the registration, lookup and management of (academic research) subjects whose data are stored in PEP. Primarily intended for use by research assessors. |
| pepcli | | A CLI utility providing access to the PEP system. |
| pepLogon | | A CLI utility that allows a user to log on interactively, after which it saves enrollment data to file. The saved enrollment data can then be (re-)used by other applications, such as `pepcli`. |
| polymorphic encryption | | (Re-)encrypting data for different recipients without gaining access to the data itself. |
| Polymorphic Encryption and Pseudonymisation | PEP | A software system geared toward secure, confidential and privacy-preserving data storage and dissemination. |
| polymorphic pseudonym | PP | A row identifier that is partially randomized. While any PP refers to a single row, a row is not uniquely associated with a single PP. |
| polymorphism | | The property of the same information taking different forms in different contexts. PEP provides polymorphism for (participant) pseudonyms and for data encryption. |
| pseudonym | | An identifier that uniquely denotes data, but is not otherwise associated with a real-world entity. Pseudonyms are applied to allow data manipulation without needing to divulge sensitive (e.g. personally identifying) information. |
| [pseudonymisation](pseudonymisation.md) | | The application of pseudonyms to data, making it difficult or impossible to associate information with a particular person or real-world entity. |
| registration | | The addition of a new row to PEP in such a way that certain cells are initialized with appropriate data. Participants are usually registered using the `pepAssessor` application. |
| Registration Server | RS | A server that can add new rows to PEP, initializing the row's cells to appropriate values. In particular, RS can ensure that participants have a full set of unique short pseudonyms (SPs), and that associated EDC records are created as needed. It also keeps a backup of SP values for recovery in the event of catastrophic data loss. |
| research assessor | assessor | A person that takes measurements from (academic research) subjects. The `pepAssessor` application is primarily aimed at this type of user. |
| role | | Synonym for "access group". |
| row | | A [data structuring](data-structure.md) primitive associated with a single entity or data subject. Rows are referred to by their [identifiers](pseudonymisation.md#identifiers-in-pep). The term "participant" is sometimes used as a synonym. |
| [short pseudonym](pseudonymisation.md#short-pseudonyms) | SP | An identifier for a piece of data not stored in PEP. Usually these are non-digital specimens such as biosamples taken during medical research. The identifier is stored in PEP, allowing the external specimen to be associated with the PEP row. |
| Storage Facility | SF | The PEP server responsible for data storage and retrieval. SF may delegate raw (bulk) storage handling to other (e.g. third party and/or cloud) storage services. |
| survey | | A set of questions intended to be filled out by (research study) participants themselves. Contrast with clinical research forms (CRFs). |
| table | | The conceptual structure of PEP's storage, in which rows refer to data entries (participants or subjects) and columns to different types of data (or separate measurements). |
| ticket | | Proof of authorization to access specific data stored in PEP. Issued by (a combination of) Access Manager and Transcryptor, users must present their tickets to Storage Facility to gain access to data. |
| Transcryptor | TS | A PEP service that cooperates with Access Manager (AM) to issue data access [tickets](access-control.md#tickets). |
| upload | | The act of storing data in PEP. |
| user | | A person or party that interacts with the PEP software. |
| user interface | UI | A facility enabling users to perform tasks on a computer. Although UI types include both command line interfaces (CLIs) and graphical user interfaces (GUIs), the term "UI" is often used as a synonym for "GUI". |
| user pseudonym | | An abbreviated form of the local pseudonym. |
