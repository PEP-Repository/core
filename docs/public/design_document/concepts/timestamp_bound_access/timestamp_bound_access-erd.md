# Timestamp Bound Access ERD

Entity relations for Timestamp Bound Access configuration. Technical basis for feature development.

```mermaid
---
title: Access Rules- and Data Versioning Diagram
---

erDiagram

%% %%MINIMAL TAG exports minimalised version of graph (for overview purposes)
%% Minimalise using script: %CORE%/scripts/docs_subset_filter.sh timestamp_bound_access-erd.md timestamp_bound_access-erd-minimal_export.md %%MINIMAL

%%MINIMAL

  LocalPseudonym {
    string pseudonym UK "Unique sequence to describe a data subject for a UserGroup"
    string participantAlias UK "A capped version of the local pseudonym"
  }

  LocalPseudonym }o--o{ UserGroup: "defined by PseudonimisationSpace"
%%MINIMAL
  UserGroup { 
    string identifier PK "e.g. `CogPDim-uploader`"
    string PseudonymisationSpace "e.g. `CogPDim`"
    %%ColumnGroupAccessRule[] CGARs
    %%ParticipantGroupAccessRule[] PGARs
%%MINIMAL
    string accessVersion FK
%%MINIMAL
  }

  %% ACCESS RULE CGAR
  UserGroup ||--o{ ColumnGroupAccessRule: "ColumnGroup access granted by"
%%MINIMAL
  ColumnGroupAccessRule }o..o{ AccessVersion: "use CGAR's up to arSnapTime"
%%MINIMAL
  ColumnGroupAccessRule {
    string userGroup FK
    ColumnGroup CG
    ColumnAccess CA "{read, read-meta, write, write-meta}"
    Timestamp   mutationTS
%%MINIMAL
  }

%%MINIMAL
  UserGroup }o--o{ AccessVersion: "may access these data versions"
%%MINIMAL
  AccessVersion {
    string identifier PK "e.g. 'V2024-1'"
%%MINIMAL
    Timestamp arSnapTime "e.g. 2024-01-27(T00:00:00)" 
%%MINIMAL
    string dataVersion FK "Refer to by identifier (e.g. 'D2024-1')"
    string comments "Updated after participant revoked consent"
%%MINIMAL
  }


  %% ACCESS RULE PGAR
  UserGroup ||--o{ ParticipantGroupAccessRule: "ParticipantGroup access granted by"
%%MINIMAL
  ParticipantGroupAccessRule }o..o{ AccessVersion: "use PGAR's up to arSnapTime"
%%MINIMAL
  ParticipantGroupAccessRule {
    string userGroup FK
    ParticipantGroup PG
    ParticipantGroupAccess PA "{access}"
    Timestamp   mutationTS
%%MINIMAL
  }

  ColumnGroupAccessRule }o..|| ColumnGroup: "concerns"
   ColumnGroup {
    string identifier
    Column[] columns
    Timestamp mutationTS
   }

  ColumnGroup }o--o{ Column: "collection of"
  Column {
     string identifier
  }
  Column }o..o{ CellData: "organises"

%%MINIMAL
  AccessVersion }o--|| DataVersion: "repository data pinpointed at"
%%MINIMAL
  DataVersion {
    string identifier PK "e.g. 'D2024-1'"
%%MINIMAL
    Timestamp dataSnapTime "e.g. 2024-01-27(T00:00:00)" 
    string comments "e.g. 'First 2024 release. Only minor modifications since R2023-7'"
%%MINIMAL
  }

%%MINIMAL
  DataVersion }o--o{ CellData: "use data at dataSnapTime"

%%MINIMAL
  CellData {
    blob data
    string column
    string participant
%%MINIMAL
  }

  ParticipantGroupAccessRule }o..|| ParticipantGroup: "concerns"
  ParticipantGroup {
    string identifier
    Participant[] participants
    Timestamp mutationTS
  }

  ParticipantGroup }o--o{ Participant: "collection of"
  Participant {
     string identifier
  }
  Participant }o..o{ CellData: "organises"

```

