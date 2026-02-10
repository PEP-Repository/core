# User Documentation

## Acquiring and running the software

See the **[installation page](/public/ops/latest/installation/)** for information on how to get the software. You'll need to ensure that your environment allows the [network connectivity required](networking-requirements.md) by the PEP client software.

## About PEP

PEP is an acronym for "Polymorphic Encryption and Pseudonymisation". Functionally, PEP is software for the storage and retrieval of tabular data. PEP's storage consists of a single table. This rather limited [data structure](data-structure.md) is offset by some features that run-of-the-mill database systems do not normally provide:

- PEP [encrypts](#encryption) data both [at rest](https://en.wikipedia.org/wiki/Data_at_rest#Encryption) and in transit, effectively providing [end-to-end encryption](https://en.wikipedia.org/wiki/End-to-end_encryption) between the data's uploader(s) and downloader(s).
- PEP [ensures](#trust-reduction) that no single server or administrator or hosting party can access the data (or provide access to it) by themselves.
- PEP [pseudonymizes data](pseudonymisation.md) to prevent multiple downloaders from blending data into a larger data set.
- PEP [keeps previous data](data-structure.md#retention) versions available after a cell's contents are overwritten.

Because of these features, PEP is usable for the storage of any (sensitive and/or confidential) information that must be made available in a pseudonymized form, and/or that must be kept available in multiple versions that may exist over time. Its current applications include the storage and dissemination of medical data for multiple academic research projects. Because of this, some of PEP's [terminology](glossary.md) is geared toward such use.

More information on PEP's four key features can be found [below](#features). More elaborate documentation can be found by following hyperlinks.

## Features

### Encryption

PEP applies strong cryptography to all data stored in the system. Cryptographic keys are only made available to authorized uploaders and downloaders, who (respectively) encrypt and decrypt the data on their local machines. Thus, data cannot be accessed by the PEP system itself, by its hosting parties, by its administrators, or by anyone else that may gain access to PEP's innards.

### Trust reduction

PEP is designed not to rely on any single party to safeguard data. Server components complement, check, and audit each others' actions. This ensures that data confidentiality cannot be compromised by breaching a single server. A similar "four eyes" principle applies to PEP's authorization system. Multiple administrators must cooperate to grant access, preventing any single administrator from being able to expose confidential data.

### Pseudonymisation

Perhaps the most central and unique feature of the system, different PEP users receive different identifiers to refer to the same data. This prevents downloaders from blending their respective data into a single, larger data set. Thus, with its built-in [pseudonymisation](pseudonymisation.md) mechanism, PEP provides some basic privacy safeguards when disseminating sensitive data such as medical or financial information.

### Retention

Data stored in PEP are [never overwritten](data-structure.md#retention). When users upload data into PEP, the system also retains any data that were previously stored at the same location. PEP can thus reconstruct its state as it was at any point in the past, allowing the exact same data set to be retrieved multiple times. This makes PEP eminently usable for the distribution of data for academic (replication) studies.

## Detailed documentation

The information in the below pages is to be merged into a more interlinked wiki structure:

- [General pepcli usage and examples](using-pepcli.md)
- Pseudonymized upload of MRI data

## Support

End users (such as researchers working with the data) should contact the support team for their respective environment when they have questions:

- For the Healthy Brain Project: <hbs-data@radboudumc.nl>
- For the Personalized Parkinson Project: <ppp-data@radboudumc.nl>

Project teams that use PEP, or parties interested in doing so, can direct their questions at <support@pep.cs.ru.nl> . Mail can be encrypted using [this GPG public key](PEP_team_0x4CD939B8_public.asc)

## NOLAI Workshop 2024/5/23

Go [to the NOLAI PEP Workshop](/public/nolai-sandbox/main/#).

## Development

To build PEP from source, see the [development resources](../index.md#Development-Resources).
