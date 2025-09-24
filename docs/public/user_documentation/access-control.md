# Access control

Actions in the PEP system are subject to access control: to be able to do anything in PEP, users must be authorized to perform that action. Authorization is granted on the basis of previously acquired enrollment data. Enrollment, in turn, is performed on the basis of an OAuth token. And such OAuth tokens are (usually) issued on the basis of prior authentication.

This page describes the authentication and authorization mechanism in some detail. Users can ignore most of the complexity by using higher-level operations supported by PEP's API and by some of its client applications.

## Authentication

PEP does not provide its own user authentication mechanism. Instead users authenticate themselves to an external service, e.g. providing user name and password to an interactive logon (Web)page. Currently only the [SURFconext](https://www.surf.nl/en/surfconext-global-access-with-1-set-of-credentials) authentication service is supported.

## Role determination

The user's identity is passed to PEP's Authentication Server (AS), which determines the role for which the user will be enrolled:

- If a user is included in a single access group, the user will be enrolled for the corresponding role.
- If a user is included in multiple access groups, they'll manually select the role for which they'll enroll.
- If a user is not assigned to an access group, they'll receive an error message and won't be able to proceed.

Authentication Server stores a list of known users and associated access groups. These assignments can only be managed by members of the `Access Administrator` role.

## OAuth token

Once the (single) access group is known, Authentication Server issues an OAuth token containing information on both the user's identity and the selected access group. Serving as proof of authentication (a "license to enroll" if you will), this OAuth token is passed to PEP's Key Server for enrollment.

Note that OAuth tokens can also be issued by Key Server<!--- @@@really?@@@ ---> administrators using the `pepToken` utility. Such OAuth tokens are structurally identical to tokens that are acquired interactively, and must also be passed to Key Server for enrollment.

## Enrollment

Users present their OAuth tokens to the PEP system to enroll themselves with the system. (Technically, the Access Manager and Transcryptor services work together to issue the enrollment data.). Enrollment produces the cryptographic materials required to perform actions in PEP:

- A combination of a private key and a certificate, which are used to cryptographically sign and verify requests to PEP's services.
- A pseudonym key, which is used to generate polymorphic pseudonyms for this particular user<!--- @@@really?@@@ --->.
- A data key, which is used to polymorphically encrypt data sent to this user<!--- @@@discuss level of indirection?@@@ --->.

The certificate's fields specify a user's identity:

- The "common name" field contains the user ID that was received from Authentication Server. PEP uses this information to log any actions the user performs.
- The "organizational unit" field contains the access group for which the user is enrolled. PEP uses this information to [authorize](#authorization) any actions the user performs.

The enrollment certificate issued to users is valid for 12 hours. After this period expires, PEP services will refuse to perform actions on the basis of this enrollment. Users must re-enroll to perform any further actions that require authorization.

### Server enrollment

PEP's server components are designed so that no single service can access stored data or otherwise compromise the system. To this end, PEP's functions are implemented as complementary, sequential actions performed by different servers. When a service is invoked, it can check, audit, and log the actions of previously invoked services. This makes the PEP system highly resilient against attacks against any single server.

Since services invoke each other to perform partial actions, servers must also be enrolled so that such invocations can be authorized. Server enrollment is similar to user enrollment (described above), producing a set of cryptographic materials needed for the server to perform its functions. Key differences are:

- Server certificates can be distinguished from user certificates, and certificates for different types of services can be distinguished from each other.
- Server certificates and associated private keys are generated *before* enrollment instead of during enrollment. Server certificates are usually issued with a longer validity period.
- Most servers do not receive a data key because they do not need to access any data. A notable exception is Registration Server, which generates [participant identifiers](pseudonymisation.md#participant-identifier) and [short pseudonyms](pseudonymisation.md#short-pseudonyms), and therefore requires access to those data columns.

## Authorization

PEP performs most of its functions by having server components handle requests. Most of these requests require the caller to provide their enrollment certificate, and to cryptographically sign the request using the corresponding private key. After signature validation, the receiving party uses the certificate to determine the caller's access group.

Server components usually log any action they are requested to perform. And since enrollment certificates are provided with server invocations, servers also log which user initiated an action. This produces an audit trail that can be used to monitor all (salient) access to PEP.

Once a caller's access group has been established, services determine whether that access group is allowed to perform the requested action. Some actions are subject to hard-coded access rules, such as [data structure](data-structure.md) management being limited to the `Data Administrator` access group.

## Data access

Data access rules are not hard-coded into PEP. Instead data access is subject to access rules that can be configured by a member of the `Access Administrator` access group. This restriction, in turn, is hard-coded into PEP: it is not possible to grant access rule configuration privileges to other access groups.

### Access rules

Data access management requires a `Data Administrator` to have [configured column and participant groups](data-structure.md#grouping). An `Access Administrator` then configures access rules on the basis of these groups:

- Access groups can be granted access to specific (named) column groups.
  - When `read` access is granted, members of the access group can download data from columns in the column group.
  - When `write` access is granted, members of the access group can upload data to columns in the column group.
- Access groups can be granted access to specific (named) participant groups.
  - When `enumerate` access is granted, members of the access group can list the participants included in the participant group.
<!---  - @@@TODO: describe `access` access@@@ --->

To be able to retrieve any data from PEP, users will need access to at least one column group *and* at least one participant group. When they are granted access to further column and/or participant groups, they'll be able to read and/or write any combination of cells matching the configured access rules. It is not possible to grant access to specific combinations of column and participant groups, e.g. "these columns for these participants, and those other columns for those other participants". If needed, such fine-grained access can be configured by assigning users to multiple access groups, and then granting appropriate privileges to those separate access groups. Users will need to [enroll](#enrollment) separately for each access group. Note that they'll receive [heterogeneous identifiers](pseudonymisation.md#identifiers-in-pep) for the different data sets, preventing them from being blended.

### Tickets

To access data stored in PEP, users must specify the row(s) and column(s) that they wish to access. As a matter of convenience, PEP also allows [row and column groups](data-structure.md#grouping) to be specified. Users must also indicate whether they'll want `read` or `write` access, or both. A user then submits a request to PEP's Access Manager (AM) server, which checks the configured [access rules(#access-rules):

- If `read` access is requested, the user's access group must have `read` privileges for the requested column(s).
- If `write` access is requested, the user's access group must have `write` privileges for the requested column(s).
- If participant groups have been specified, the user's access group must have `enumerate` privileges for the requested participant group(s).

If the user has the required privileges, AM authorizes the data access. Access Manager (AM) and Transcryptor (TS) services then perform complementary actions to issue a data access ticket. By requiring an interplay between AM and TS, even if one of these services' security is compromised, unauthorized data access will be prevented in many cases because the other service will (in most cases) refuse to cooperate with the compromised service. Also, since both services generate logging for any ticket being issued, all data access can be audited.

Successfully issued tickets are returned to the user, who can present it to PEP's Storage Facility to access any data covered by the ticket. A ticket can be used multiple times within its validity period of 24 hours. If users wish to access the same data after this time, they must have PEP issue a new ticket to them.
