#include <pep/accessmanager/tests/TestSuiteGlobalConfiguration.hpp>

// Contents were copied from core/config/projects/gum/accessmanager/GlobalConfiguration.json

//language=json
const std::string pep::tests::TEST_SUITE_GLOBAL_CONFIGURATION = R"JSON(
{
  "participant_identifier_formats": [
    {"generable" : {"prefix" : "GUM", "digits" : 10}},
    {"regex" : {"pattern" : "[a-zA-Z0-9]{15}"}}
  ],

  "column_specifications": [
    { "column": "OnlyPseudonymise",
      "associated_short_pseudonym_column": "ShortPseudonym.Bugs.Visit1.FMRI"
    },
    { "column": "OnlyDirectory",
      "requires_directory": true
    },
    { "column": "BothPseudoAndDir",
      "associated_short_pseudonym_column": "ShortPseudonym.Bugs.Visit1.FMRI",
      "requires_directory": true
    },
    { "column": "Visit1.MRI.Func",
      "associated_short_pseudonym_column": "ShortPseudonym.Visit1.FMRI",
      "requires_directory": true
    },
    { "column": "Visit1.MRI.Anat",
      "associated_short_pseudonym_column": "ShortPseudonym.Visit1.FMRI",
      "requires_directory": true
    },
    { "column": "Bugs.Visit1.MRI.Func",
      "associated_short_pseudonym_column": "ShortPseudonym.Bugs.Visit1.FMRI",
      "requires_directory": true
    },
    { "column": "Bugs.Visit1.MRI.Anat",
      "associated_short_pseudonym_column": "ShortPseudonym.Bugs.Visit1.FMRI",
      "requires_directory": true
    },
    { "column": "Bugs.Visit2.MRI.Func",
      "associated_short_pseudonym_column": "ShortPseudonym.Bugs.Visit2.FMRI",
      "requires_directory": true
    },
    { "column": "Bugs.Visit2.MRI.Anat",
      "associated_short_pseudonym_column": "ShortPseudonym.Bugs.Visit2.FMRI",
      "requires_directory": true
    },
    { "column": "Visit3.MRI.Func",
      "associated_short_pseudonym_column": "ShortPseudonym.Visit3.FMRI",
      "requires_directory": true
    },
    { "column": "Visit3.MRI.Anat",
      "associated_short_pseudonym_column": "ShortPseudonym.Visit3.FMRI",
      "requires_directory": true
    }
  ],

  "study_contexts": [
    { "id": "GUM" },
    { "id": "BUGS" }
  ],

  "short_pseudonyms": [
    {"column": "ShortPseudonym.Castor.Reports", "prefix": "GUMRE", "length": 5,
      "castor": {
        "study_slug": "pomreportsa",
        "site_abbreviation": "umcn"}},
    {"column": "ShortPseudonym.Castor.ECG", "prefix": "GUMEC", "length": 5, "description": "ECG findings",
      "castor": {
        "study_slug": "pomvis1ecga",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "STUDY",
          "data_column": "Castor.ECG",
          "import_study_slug": "pomvis1ecgp",
          "immediate_partial_data": true
        }}},
    {"column": "ShortPseudonym.Castor.DeviceDeficiency", "prefix": "GUMDD", "length": 5, "description": "Device deficiencies",
      "castor": {
        "study_slug": "pomdevidefa",
        "site_abbreviation": "umcn"}},
    {"column": "ShortPseudonym.Castor.EndOfStudy", "prefix": "GUMES", "length": 5, "description": "End of Study Survey",
      "castor": {
        "study_slug": "cHR2JwNU8M99vWEEXQgJ5n",
        "site_abbreviation": "umcn"}},


    {"column": "ShortPseudonym.Visit1.FMRI", "prefix": "GUM1FM", "length": 5, "description": "fMRI"},
    {"column": "ShortPseudonym.Visit1.ECG", "prefix": "GUM1EC", "length": 5, "stickers": 1, "suppress_additional_stickers": true},
    {"column": "ShortPseudonym.Visit1.PBMC", "prefix": "GUM1PM", "length": 5, "stickers": 4},
    {"column": "ShortPseudonym.Visit1.Plasma", "prefix": "GUM1PL", "length": 5, "stickers": 4},
    {"column": "ShortPseudonym.Visit1.DNA", "prefix": "GUM1DN", "length": 5, "stickers": 1},
    {"column": "ShortPseudonym.Visit1.RNA", "prefix": "GUM1RN", "length": 5, "stickers": 3},
    {"column": "ShortPseudonym.Visit1.Serum", "prefix": "GUM1SE", "length": 5, "stickers": 3},
    {"column": "ShortPseudonym.Visit1.CSF", "prefix": "GUM1CS", "length": 5, "stickers": 3},
    {"column": "ShortPseudonym.Visit1.Stool", "prefix": "GUM1ST", "length": 5, "stickers": 2},
    {"column": "ShortPseudonym.Visit1.QAVideo", "prefix": "GUM1VD", "length": 5, "description": "QA Video",
      "castor": {
        "study_slug": "dQ4onnU5YGztyWKptKzRqG",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "REPEATING_DATA",
          "data_column": "Castor.Updrs1",
          "import_study_slug": "egtjShrkrotkUPgFoGaoY7"
        }}},
    {"column": "ShortPseudonym.Visit1.Castor.Visit", "prefix": "GUM1VS", "length": 5,
      "castor": {
        "study_slug": "pomvis1crfa",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "STUDY",
          "data_column": "Castor.Visit1",
          "import_study_slug": "pomvis1crfp"
        }}},
    {"column": "ShortPseudonym.Visit1.FoodQuestionnaire", "prefix": "GUM1FQ", "length": 5, "description": "Food Questionnaire"},
    {"column": "ShortPseudonym.Visit1.Castor.HomeQuestionnaires", "prefix": "GUM1HQ", "length": 5, "description": "Home questionnaires",
      "castor": {
        "study_slug": "pomvis1homa",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "SURVEY",
          "data_column": "Castor.HomeQuestionnaires1",
          "import_study_slug": "pomvis1homp"
        }}},
    {"column": "ShortPseudonym.Visit1.Castor.StoolQuestionnaire", "prefix": "GUM1SQ", "length": 5, "description": "Stool questionnaire",
      "castor": {
        "study_slug": "pomsto1crfa",
        "site_abbreviation": "umcn"}},

    {"column": "ShortPseudonym.Visit2.ECG", "prefix": "GUM2EC", "length": 5, "stickers": 1, "suppress_additional_stickers": true},
    {"column": "ShortPseudonym.Visit2.Plasma", "prefix": "GUM2PL", "length": 5, "stickers": 4},
    {"column": "ShortPseudonym.Visit2.RNA", "prefix": "GUM2RN", "length": 5, "stickers": 3},
    {"column": "ShortPseudonym.Visit2.Serum", "prefix": "GUM2SE", "length": 5, "stickers": 3},
    {"column": "ShortPseudonym.Visit2.Stool", "prefix": "GUM2ST", "length": 5, "stickers": 2},
    {"column": "ShortPseudonym.Visit2.QAVideo", "prefix": "GUM2VD", "length": 5, "description": "QA Video",
      "castor": {
        "study_slug": "cevfptZSSog4Jq3AjY76ag",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "REPEATING_DATA",
          "data_column": "Castor.Updrs2",
          "import_study_slug": "PwT86vrJKN762vdMhRGizJ"
        }}},
    {"column": "ShortPseudonym.Visit2.Castor.Visit", "prefix": "GUM2VS", "length": 5,
      "castor": {
        "study_slug": "pomvis2crfa",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "STUDY",
          "data_column": "Castor.Visit2",
          "import_study_slug": "pomvis2crfp"
        }}},
    {"column": "ShortPseudonym.Visit2.FoodQuestionnaire", "prefix": "GUM2FQ", "length": 5, "description": "Food Questionnaire"},
    {"column": "ShortPseudonym.Visit2.Castor.HomeQuestionnaires", "prefix": "GUM2HQ", "length": 5, "description": "Home questionnaires",
      "castor": {
        "study_slug": "pomvis2homa",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "SURVEY",
          "data_column": "Castor.HomeQuestionnaires2",
          "import_study_slug": "pomvis2homp"
        }}},
    {"column": "ShortPseudonym.Visit2.Castor.StoolQuestionnaire", "prefix": "GUM2SQ", "length": 5, "description": "Stool questionnaire",
      "castor": {
        "study_slug": "pomsto2crfa",
        "site_abbreviation": "umcn"}},

    {"column": "ShortPseudonym.Visit3.FMRI", "prefix": "GUM3FM", "length": 5, "description": "fMRI"},
    {"column": "ShortPseudonym.Visit3.ECG", "prefix": "GUM3EC", "length": 5, "stickers": 1, "suppress_additional_stickers": true},
    {"column": "ShortPseudonym.Visit3.Plasma", "prefix": "GUM3PL", "length": 5, "stickers": 4},
    {"column": "ShortPseudonym.Visit3.RNA", "prefix": "GUM3RN", "length": 5, "stickers": 3},
    {"column": "ShortPseudonym.Visit3.Serum", "prefix": "GUM3SE", "length": 5, "stickers": 3},
    {"column": "ShortPseudonym.Visit3.Stool", "prefix": "GUM3ST", "length": 5, "stickers": 1},
    {"column": "ShortPseudonym.Visit3.CSF", "prefix": "GUM3CS", "length": 5, "stickers": 3},
    {"column": "ShortPseudonym.Visit3.QAVideo", "prefix": "GUM3VD", "length": 5, "description": "QA Video",
      "castor": {
        "study_slug": "BhnRRqzRMBJbmeiBecP3jg",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "REPEATING_DATA",
          "data_column": "Castor.Updrs3",
          "import_study_slug": "mtidLy9N9Sd9SKFMRRNzHX"
        }}},
    {"column": "ShortPseudonym.Visit3.Castor.Visit", "prefix": "GUM3VS", "length": 5,
      "castor": {
        "study_slug": "pomvis3crfa",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "STUDY",
          "data_column": "Castor.Visit3",
          "import_study_slug": "pomvis3crfp"
        }}},
    {"column": "ShortPseudonym.Visit3.FoodQuestionnaire", "prefix": "GUM3FQ", "length": 5, "description": "Food Questionnaire"},
    {"column": "ShortPseudonym.Visit3.Castor.HomeQuestionnaires", "prefix": "GUM3HQ", "length": 5, "description": "Home questionnaires",
      "castor": {
        "study_slug": "u5BUTAmZfTsqajjjNuzRo",
        "site_abbreviation": "umcn",
        "storage": {
          "study_type": "SURVEY",
          "data_column": "Castor.HomeQuestionnaires3",
          "import_study_slug": "pomvis3homp"
        }}},
    {"column": "ShortPseudonym.Visit3.Castor.StoolQuestionnaire", "prefix": "GUM3SQ", "length": 5, "description": "Stool questionnaire",
      "castor": {
        "study_slug": "pomsto3crfa",
        "site_abbreviation": "umcn"}},


    {"study_context": "BUGS", "column": "ShortPseudonym.Bugs.DNA", "prefix": "BUGDN", "length": 5, "stickers": 1},
    {"study_context": "BUGS", "column": "ShortPseudonym.Bugs.Serum", "prefix": "BUGSE", "length": 5, "stickers": 1},

    {"study_context": "BUGS", "column": "ShortPseudonym.Bugs.Castor.Reports", "prefix": "BUGRE", "length": 5, "description": "Castor",
      "castor": {
        "study_slug": "pitreportsacceptance",
        "site_abbreviation": "DCCN",
        "storage": [
          { "study_type": "SURVEY",
            "data_column": "Bugs.Castor.HomeQuestionnaires",
            "import_study_slug": "pitreportsproduction"
          },
          { "study_type": "STUDY",
            "data_column": "Bugs.Castor.Visit",
            "import_study_slug": "pitreportsproduction",
            "immediate_partial_data": true
          }
        ]}},

    {"study_context": "BUGS", "column": "ShortPseudonym.Bugs.Visit1.FMRI", "prefix": "BUG1MR", "length": 5, "description": "fMRI"},
    {"study_context": "BUGS", "column": "ShortPseudonym.Bugs.Visit1.SerialChoice", "prefix": "BUG1CH", "length": 5, "description": "Serial Choice Task"},
    {"study_context": "BUGS", "column": "ShortPseudonym.Bugs.Visit2.FMRI", "prefix": "BUG2MR", "length": 5, "description": "fMRI"}
  ],

  "short_pseudonym_errata": [
    {"value": "GUM1ST1234567", "column": "ShortPseudonym.Visit1.Serum"}
  ],

  "user_pseudonym_format":
    {"prefix" : "GUMU", "minLength" : 1, "length" : 16},

  "additional_stickers": [
    {"visit": 1, "column": "ShortPseudonym.Visit2.Stool", "stickers": 1},
    {"visit": 1, "column": "ShortPseudonym.Visit2.Castor.StoolQuestionnaire", "stickers": 0},
    {"visit": 2, "column": "ShortPseudonym.Visit3.Stool", "stickers": 1},
    {"visit": 2, "column": "ShortPseudonym.Visit3.Castor.StoolQuestionnaire", "stickers": 0}
  ],

  "devices": [
    {"column": "DeviceHistory",        "serial_number_format": "[a-zA-Z0-9]{16}", "description": "GUM horloge",     "tooltip": "Voer 16 karakters in bestaande uit hoofdletters of cijfers"},
    {"column": "Bugs.DeviceHistory", "serial_number_format": "[a-zA-Z0-9]{16}", "description": "BUGS horloge", "tooltip": "Voer 16 karakters in bestaande uit hoofdletters of cijfers", "study_context": "BUGS"}
  ],

  "assessors": [
    {"id": 1, "name": "Popeye the Sailor Man"},
    {"id": 2, "name": "Mickey Mouse", "study_contexts": ["GUM"]},
    {"id": 3, "name": "Donald Duck", "study_contexts": ["BUGS"]},
    {"id": 4, "name": "Skeletor", "study_contexts": ["GUM", "BUGS"]}
  ]
}
)JSON";
