# Ticket request

A user requests a ticket for access to certain participants and columns. They either specify a list of subject groups or polymorphic pseudonyms.
The servers will translate the PPs and return encrypted local pseudonyms for servers and the user in the ticket.
The ticket can be presented to e.g. the Storage Facility later on.

```mermaid
sequenceDiagram
    title Ticket request
    actor User
    participant AM as Access Manager
    participant TS as Transcryptor

    User->>+AM: Subject groups or PPs, columns,<br/>signature + log signature

        AM-->>AM:
        note right of AM: Check signature
        note right of AM: Check column access
        opt Request with subject groups
            note right of AM: Check subject group access
            note right of AM: Retrieve PPs from DB
            note right of AM: Re-randomize PPs
        end
        note right of AM: RSK with proof on PPs for AM, SF, TS, user

        AM->>+TS: User request with only log signature#59;<br/>PPs, partial pseudonyms, proofs

            TS-->>TS:
            note right of TS: Check log signature
            note right of TS: Check RSK proofs for AM, SF, TS
            note right of TS: Finish RSK for AM, SF, TS, user
            note right of TS: Log: user request with only log signature#59;<br/>TS pseudonyms#59; access modes#59;<br/>hash of PPs & translated AM, SF, user pseudonyms

        TS->>-AM: PPs & translated AM, SF, user pseudonyms#59; log ID

        AM-->>AM:
        note right of AM: Check participant access<br/>(using decrypted AM pseudonyms)
        opt Write request with PPs
            note right of AM: Add PPs for LPs to DB
        end
        note right of AM: Create ticket signed by AM with<br/>timestamp#59; access modes#59; columns#59; user group#59;<br/>PPs & translated AM, SF, user pseudonyms

        AM->>+TS: Ticket signed by AM, log ID

            TS-->>TS:
            note right of TS: Check signature
            note right of TS: Log: log ID, columns, timestamp
            note right of TS: Sign ticket

        TS->>-AM: TS ticket signature

        AM-->>AM:
        note right of AM: Add TS signature to ticket

    AM->>-User: Ticket signed by AM & TS
```
