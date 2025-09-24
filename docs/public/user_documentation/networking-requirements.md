# Networking requirements

For the PEP client software to function, it must be able to communicate with various PEP servers and other services. While this should work out of the box for installations on regular (standalone) machines, network connectivity may need to be explicitly allowed on more limited host systems such as machines provisioned by IT departments or secure processing environments (SPEs). This document details the various services that must be accessible for client software to work properly.

Required connectivity is described in general terms, specifying which hosts, IP addresses and ports must be accessible for PEP client software to function properly. You'll need to enable (outgoing) connectivity to each of these locations. Please contact your environment's vendor or support organization if you need help setting things up. While the PEP team does not support nor endorse any specific limited environment(s), you'll find some instructions for specific situations near the bottom of this page.

Most required network connections will depend on the environment (project/repository) that the software will connect to. Service locations for specific environments are specified in the `ClientConfig.json` file that is deployed with the client software.

- On Windows you'll find the `ClientConfig.json` file in the directory where the PEP software was installed, which defaults to `C:\Program Files\PEP-Client (project+type)` (where `project+type` are the name and flavor of the environment).
- On MacOS you'll find the `ClientConfig.json` file by right clicking the PEP application (default location at `/Applications/PEP Command Line Interface (project+type).app`), selecting `Show Package Contents` and then navigating to `Contents/Resources/ClientConfig.json`.
- In Docker you'll find the `ClientConfig.json` file in the `/config` directory within the container.
- When using Flatpak, the contents of `ClientConfig.json` can be found using the command `flatpak run nl.ru.cs.pep.[ENVIRONMENT] cat /app/bin/ClientConfig.json` (where `[ENVIRONMENT]` is replaced with the specific repo identifier. E.g. creating something like `nl.ru.cs.pep.proj-acc`).

If a requirement is documented in terms of a host(name) but your environment requires IP addresses to be specified, use a utility such as `ping` to find out the IP address associated with a host.

## Connections to PEP services

Client software must be able to communicate with the PEP servers of the environment that they will connect to. The locations of these servers are specified in the `Address` and `Port` settings in specific nodes of `ClientConfig.json` file:

| Service/node name    | Usual port |
| -------------------- | ---------- |
| `AccessManager`      | `16501`    |
| `StorageFacility`    | `16519`    |
| `KeyServer`          | `16511`    |
| `Transcryptor`       | `16516`    |
| `RegistrationServer` | `16518`    |
| `Authserver`         | `16512`    |

Ensure that the host system allows outgoing network connections to each of the configured `Address` and `Port` combinations.

## Connections to (HTTPS) Web servers

Client software needs to establish HTTPS connections to some Web servers. You'll therefore need to allow outgoing connections to port `443` on the following hosts:

| Address        | Description |
| -------------- | ----------- |
| *varies*       | Server(s) specified in URLs under `ClientConfig.json`'s `AuthenticationServer` node. Required for interactive logon to work. |
| `pep.cs.ru.nl` | Required to download (updated versions of) the PEP software. The `pepAssessor` GUI client will not run if it cannot check for updates during startup, and all client software will fail to run if it is (too) outdated. The Website also hosts documentation for the PEP software, which may be convenient to keep accessible during use. |
| `data.castoredc.com` | Only required if the PEP environment interfaces with [the Castor EDC](https://www.castoredc.com/) and if the `pepAssessor` GUI application is used. Without access to this Website, the browser will show an error message when using a GUI button to open a participant's Castor record. |

### Connections to authentication services

PEP allows users to log on interactively with 3rd-party authentication services. Since such authentication services are Web based, you'll need to allow HTTPS connectivity to the appropriate Web location(s).

| Service    | Address         | Description |
| ---------- | --------------- | ----------- |
| SURFconext | `surfconext.nl` | Central authentication service for Dutch academic and educational organizations. See more information below.

SURFconext is a frontend for authentication services of individual organizations, and will forward users to their specific organization's logon page. For SURFconext authentication to work, you'll therefore also need to enable HTTPS traffic to the authentication services used by (the organizations of) the users of the DRE:

| SURFconext organization | Required connectivity        |
| ----------------------- | ---------------------------- |
| Radboud University      | `conext.authenticatie.ru.nl` |
| RadboudUMC              | `microsoftonline.com` and `msauth.net` |

Note that the required connectivity for RadboudUMC was specified by anDREa support, and will likely apply to every organization that uses the Microsoft services to authenticate users.

## Configuration of the `myDRE` environment by `anDREa`

This section documents some requirements to get PEP client software to work on the [myDRE](https://mydre.org/) environment offered by [anDREa](https://andrea-cloud.com/).

### Workspace settings

In the [myDRE Web interface](https://mydre.org/workspaces), log on with administrative privileges and edit settings for the `External Access` of your workspace:

- under `Domain-Allowlisting`, add rules to allow outgoing connections to the (HTTPS) Web servers documented elsewhere on this page. This mechanism will keep connectivity working even if the destination service switches to a different IP address.
- under `IP-Allowlisting`, add rules to allow outgoing connections to
  - the IP addresses and ports of the PEP services listed elsewhere on this page.
  - port `443` on the IP address of the server specified in the URL under `ClientConfig.json`'s `AuthenticationServer` -> `TokenURL` node. While this server's name is (likely) already allowlisted through a domain-based rule, you'll also need this IP-based rule for interactive logon to work.

Ensure that each IP-based rule is enabled, i.e. that the slider for the rule is set to the appropriate position.

### Windows settings

Within the DRE's Windows environment:

- To allow the PEP software to start: under Start Menu -> type `Control Panel` and open the app -> Change date, time, or number formats -> tab "Administrative" -> click button "Change system locale...". In the dialog that pops up, check the box marked "Beta: Use Unicode UTF-8 for worldwide language support".
- To make domain-based allowlist rules work: under Start Menu -> Settings -> Network & Internet -> Proxy, enable the use of a proxy server for outgoing connections. According to anDREa support staff, in addition to the default proxy settings, an entry for `filerepositorychntiwhy.blob.core.windows.net` should be added to the list of excluded addresses.
