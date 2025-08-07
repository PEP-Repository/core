# Castor integration

Data from any source can be stored into PEP by manually uploading it, e.g. using the [`pepcli`](using-pepcli.md) utility. Users can automate the invocation of `pepcli`, e.g. to upload data that becomes available periodically or that can otherwise be processed algorithmically. For data stored in the Castor Electronic Data Capturing (EDC) system, PEP provides such automation out of the box, as well as other integration features.

## Overview

When the Castor EDC is used in a (scientific biomedical research) study to gather data on subjects, PEP can be configured to assist in several tasks. With proper configuration:

- PEP ensures that Castor records are created for participants known to PEP.
- The `pepAssessor` GUI application provides buttons to access participants' Castor records without needing to look them up manually.
- Data entered into Castor can be automatically and/or periodically imported into PEP.

The first two aspects ensure that assessors create and find proper Castor records for participants during data gathering. This minimizes manual work, and improves data quality by helping ensure that data entered into Castor ends up in the correct record. This level of integration can be achieved with minimal [configuration](#step-by-step) by the PEP (support) team. More configuration is needed and more considerations apply if data are also to be imported from Castor into PEP.

## Import

PEP can import (several types of) data from the Castor system. The import stores the data into PEP, from where they can be retrieved by PEP users. PEP ensures that Castor data are imported into the proper row by storing participants' Castor record IDs in a short pseudonym column.

### Data types

Depending on what is stored in a Castor study, PEP can import several types of entities from it.

- *Clinical research forms (CRFs)* are filled out by assessors during a study's data gathering stage. Entered data can then be imported into PEP. Imported data will include any reports generated due to CRFs containing repeated measure fields.
- *Reports* can be created in Castor by hand. PEP offers the option to import such standalone reports.
- *Surveys* can be filled out by participants, the data then being stored in Castor. PEP can import data from completed surveys.

PEP can import data from multiple Castor studies, and it can import multiple types of data from any single study.

### Steps imported individually

PEP imports data for individual Castor (CRF) [steps](https://helpdesk.castoredc.com/article/18-create-edit-and-delete-phases-and-steps) into separate PEP columns. Since [access to PEP is controlled](access-control.md) at the level of columns, the implication is that PEP can distribute imported Castor data with a granularity down to (CRF) steps. In other words: PEP users can be granted access to (answers to) individual Castor steps, but not to smaller units (e.g. answers to individual questions). This should be taken into account when configuring a Castor study meant for PEP integration. Questions should only be grouped together into a step if their answers should always be distributed (or withheld) together by PEP.

An equivalent principle applies to the import of Castor survey data: answers to a single survey step end up in a single PEP column. Surveys should therefore also be configured in such a way that their data are chunked appropriately.

### Reports imported together

When a CRF contains a repeated measurement question, Castor stores the associated answers in *reports*, with each report containing the answers for a single measurement. When PEP imports such CRF data, all reports associated with the (repeated measurement-type) question are included with the step containing the question. For example, if a subject takes four types of medication, these would be registered as four entries into a repeated measurement question. Castor would store these as four separate reports. When PEP imports the CRF step containing the repeated measurement question, it also includes all answers entered into all four reports.

When PEP is configured to import (only) reports from a Castor study, it puts the answers to all reports of all types into a single column.

### Data format

Castor data are stored in PEP as JSON: [a data format](https://www.json.org/) that associates keys with values. PEP stores Castor data as a top level JSON node with sub-nodes named `crf` and `reports`.

- The `crf` node's value is a JSON object containing the data entered into Castor.
  - Keys in this object correspond with Castor question IDs.
  - Values contain the (raw) answer entered into Castor.
- The `reports` node contains any Castor reports associated with the imported data, e.g. due to a CRF containing [repeated measurements](https://helpdesk.castoredc.com/article/191-the-repeated-measure-field). The node's value is an object with sub-nodes named after the types of reports. The value of each such sub-node is a JSON array containing objects representing individual reports of that type:
  - Keys in these objects correspond with Castor question IDs.
  - Values contain the (raw) answer entered into Castor.

For example, a PEP cell might contain the following data:

```json
{
  "crf": {
    "DiagParkYear": "1960",
    "PrefHand": "##USER_MISSING_97##",
    "FirstSympYear": "1950",
    "DiagParkMonth": "3",
    "WatchSide": "1",
    "PreferLeg": "3",
    "FirstSympMonth": "3",
    "MostAffSide": "2",
    "WalkingAid": {
      "0": "true",
      "1": "false",
      "2": "false",
      "3": "false",
      "4": "false",
      "5": "false",
      "6": "false"
    },
    "DiagParkNeurologist": "Piet Paulusma",
    "DiagParkDay": "3"
  },
  "reports": ""
}
```

In the sample data:

- the `DiagParkYear` node contains a value of `1960`, indicating that the answer `1960` was entered for a question with this ID. Presumably this patient was diagnosed with Parkinson's disease in the year 1960, but note that PEP does not deal with Castor data semantics.
- the `WalkingAid` node (apparently) contains the answers to a [checkbox](https://helpdesk.castoredc.com/article/185-create-a-field-with-option-groups-radio-button-checkbox-and-dropdown)-type question.
- the `reports` node contains no entries, indicating that no repeated measurements are associated with these Castor data. Perhaps the Castor configuration contained no questions of that type, or no (repeated) measurements were taken.

If PEP is configured to import standalone reports, data are placed into the `reports` node. Keys of sub-nodes indicate the *type* of report having been imported. The value will be a JSON array containing the answer sets for individual reports.

The import of standalone reports produces an equivalent JSON structure to keep things consistent across PEP columns. But note that the `crf` node will always be empty in these cases.

### Column naming

PEP's Castor import is configured with a prefix that differs between Castor studies and between the type(s) of data that are imported from each Castor study. The import routine then stores Castor data into columns whose names start with the configured prefix. The remaining part of the column name is derived from the Castor entity, as follows:

- CRF step data are imported into columns named `<prefix>.<phase>.<step>`.
- Report data are imported into columns named just `<prefix>`.
- Survey step data are imported into columns named `<prefix>.<package>.<survey>.<step>`.

The described column naming for surveys is used when only a single survey is (answered and) imported into PEP. When participants answer surveys multiple times, and if PEP is configured to import all these data sets, column names with further suffixes are used instead:

- Survey step data are imported into columns named `<prefix>.<package>.<survey>.<step>.AnswerSet<nnn>`.
- The week number when the survey was sent is stored into columns named `<prefix>.<package>.<survey>.<step>.AnswerSet<nnn>.WeekNumber`.

The `nnn` in these column names is a zero-based index: the first survey's data are imported as `AnswerSet0`, the second survey's as `AnswerSet1`, and so on.

By convention, the import routine runs as a member of access group `PullCastor`, which is [authorized](access-control.md#data-access) to store data into a column group named `Castor`. Since the access group can store data but not create columns, Data Administrator must ensure that appropriate columns have been made available before the import is attempted. If Castor contains data that should be stored into a column (with a name) that does not exist, the import process will fail. Data Administrator can use the [`pepcli castor` command](using-pepcli.md#castor) to determine names of required columns, and to create those columns.

#### Name mangling

Due to technical limitations, PEP column names may contain only

- basic alphabetic characters: uppercase `A` through `Z` and lowercase `a` through `z`.
- digits `0` through `9`.
- the underscore `_` character.
- the period (full stop) `.` character.

By convention, the period character `.` is used to split column names into separate sections. This principle is also applied during Castor import: names of different Castor entities are concatenated using a period `.` delimiter.

The import process applies name mangling to strip Castor names of disallowed characters before being used as PEP column name sections:

- all whitespace characters are replaced by an underscore character, and
- all remaining disallowed characters are dropped from the name.

To ensure that generated column names use the period `.` character exclusively as a section delimiter, it too is dropped from Castor names.

#### Column name mappings

PEP imports Castor data into columns based on the names of Castor entities. But Castor is configured for human consumption, causing configured names to be long and descriptive, and localized to the culture of the people entering the data. This results in PEP column names often becoming cumbersomely long and/or difficult to interpret by international users. To counteract, PEP offers Data Administrator the ability to configure column name mappings as a way to have PEP and its import process use different column names.

Mappings apply to the individual sections of Castor import column names. If PEP encounters a mapped Castor name during import, it names the corresponding column name section after the configured replacement instead of the raw Castor name. For example, a Castor survey step might be called `01. Demografische vragen voor de mantelzorger` ("01. Demographic questions for the caregiver"). Data Administrator could use [the `pepcli castor column-name-mapping` command](using-pepcli.md#castor-column-name-mapping) to have PEP use the `CgDemog` moniker instead of the long and descriptive Dutch original.

If a mapping is configured, it applies to all Castor entities with the same name. For example, when Data Administrator introduces a mapping for a survey step named `Stap 2`, the mapping will also be applied to all other entities (surveys, phases, steps etc.) that are also named `Stap 2`. Data Administrator should be aware that, when a mapping is introduced, the import of multiple Castor studies and/or data types may require differently named columns.

Note that column name mappings are applied to [mangled](#name-mangling) names rather than raw names. Castor entities named `pésca` ("fishing") and `pèsca` ("peach") would therefore be subject to the same mapping, since both are mangled to `psca`.

## Step by step

Configuring PEP's Castor integration functionality requires multiple administrative actions, as well as data entry by assessors and/or study participants.

1. Create and configure a Castor study, e.g. using the [Castor Web interface](https://data.castoredc.com/).
2. Grant appropriate access to the Castor study.<!--- @@@ which privileges? to whom? @@@ --->
3. Have the PEP (support) team configure a short pseudonym column for the Castor study.
4. Register new participants and/or use `pepAssessor` to open existing ones to generate short pseudonyms and create corresponding records in Castor.
5. Have assessors and/or participants enter data into Castor records.
6. Have the PEP (support) team configure the data type(s) to be imported from the Castor study.
7. Have Data Administrator configure PEP columns (details [below](#column-configuration)) required for the import.
8. Have the PEP (support) team perform an import run and/or schedule imports to be performed periodically.

### Column configuration

When PEP's import routine encounters Castor data, it simply attempts to store it into the appropriate column. Since the import routine cannot create the required column(s) itself, Data Administrator must have done so beforehand to prevent the import from failing:

1. Use [the `pepcli castor list-sp-columns` command](using-pepcli.md#castor-list-sp-columns) with its `--imported-only` switch to determine the name(s) of short pseudonym columns bound to Castor studies.
2. Use [the `pepcli castor list-import-columns` command](using-pepcli.md#castor-list-import-columns) to determine the [names of columns](#column-naming) required to import data from Castor.
3. If the column names listed in step 2 are not to your liking, use [the `pepcli castor column-name-mapping` command](using-pepcli.md#castor-column-name-mapping) to define [column name mappings](#column-name-mappings). Repeat steps 2 and 3 until all column names are acceptable.
4. Use [the `pepcli castor create-import-columns` command](using-pepcli.md#castor-create-import-columns) to add import columns that do not yet exist. The command automatically places newly created columns into the `Castor` column group, making them writable to the import process. (Alternatively, use the `pepcli ama column` command to create and group columns manually.)

## Limitations

When the configuration of the Castor study changes, different and/or additional columns may be needed when data are imported into PEP. This requires Data Administrator to reconfigure import columns, i.e. perform the [column configuration steps](#column-configuration) again.

Note that PEP's import routine does not deal well with removals in the Castor study's configuration. For example, when a survey is removed from a survey package, the corresponding column names can no longer be listed or created by [the `pepcli castor` command](using-pepcli.md#castor). But if data has been entered on the basis of the old configuration, the import process will still attempt to store it under the unlisted name. Castor and PEP administrators should take this into account when reconfiguring the Castor study and/or PEP's import columns.
