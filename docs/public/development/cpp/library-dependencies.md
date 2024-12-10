# Library Dependencies

PEP's most basic C++ libraries have the following (inter-)dependencies. See below for more specific (i.e. dependent) libraries.

```mermaid
---
title: Basic library dependencies
---

%% (Ab)using class diagram type because Mermaid has no separate type for dependency graphs
classDiagram

%% Utils
class Utils {
    Boost::log +  log_setup
    MBedTLS::mbedcrypto
    OpenSSL::Crypto
    xxHash::xxHash
}

%% Testing
class Testing {
    GTest::gtest
}
Utils <|-- Testing

%% Gui
class Gui {
    Qt6::...
    rxcpp
}
Utils <|-- Gui

%% Archiving
class Archiving {
    Boost::iostreams
    LibArchive::LibArchive
}
Utils <|-- Archiving

%% Versioning
Protohash <|-- Versioning

%% Proto
class Proto {
    protobuf::libprotobuf
}

%% Serialization
Utils <|-- Serialization
Proto <|-- Serialization

%% Async
class Async {
    rxcpp
}
Utils <|-- Async

%% Crypto
class Crypto {
    MbedTLS::mbedx509
}
Serialization <|-- Crypto

%% Application
Versioning <|-- Application

%% Networking
class Networking {
    OpenSSL::SSL
}
Versioning <|-- Networking
Async <|-- Networking
Crypto <|-- Networking

%% ServiceApplication
Application <|-- ServiceApplication
Networking <|-- ServiceApplication

%% Httpserver
class Httpserver {
    civetweb
    civetweb::civetweb-cpp
}
Networking <|-- Httpserver

%% Structure
RskPep <|-- Structure

%% Castor
class Castor {
    entities
}
Networking <|-- Castor
Structure <|-- Castor

%% Elgamal
class Elgamal {
    panda
}
Crypto <|-- Elgamal

%% Rsk
Elgamal <|-- Rsk

%% RskPep
Rsk <|-- RskPep

%% Morphing
RskPep <|-- Morphing
```

PEP's more specific (i.e. dependent) libraries have have the following (inter-)dependencies. See above for more basic (i.e. less dependent) libraries.

```mermaid
---
title: More specific library dependencies
---

%% (Ab)using class diagram type because Mermaid has no separate type for dependency graphs
classDiagram

%% Serialization
class Serialization {
    - see basic dependencies -
}

%% Crypto
class Crypto {
    - see basic dependencies -
}
Serialization <|-- Crypto

%% Elgamal
class Elgamal {
    - see basic dependencies -
}
Crypto <|-- Elgamal

%% Rsk
Elgamal <|-- Rsk

%% RskPep
Rsk <|-- RskPep

%% Morphing
RskPep <|-- Morphing
Auth <|-- Morphing

%% Networking
class Networking {
    - see basic dependencies -
}
Crypto <|-- Networking

%% Ticketing
RskPep <|-- Ticketing

%% Registration Server
RskPep <|-- RegistrationServerApi

%% Structure
class Structure {
    - see basic dependencies -
}
%%Morphing <|-- Structure

%% Transcryptor
Ticketing <|-- TranscryptorApi
Morphing <|-- TranscryptorApi

%% AccessManager
class AccessManagerApi {
    Boost::random
}
Structure <|-- AccessManagerApi
TranscryptorApi <|-- AccessManagerApi

%% StorageFacility
Ticketing <|-- StorageFacilityApi
Morphing <|-- StorageFacilityApi

%% Auth
Crypto <|-- Auth

%% Authserver
Crypto <|-- AuthserverApi

%% KeyServer
Serialization <|-- KeyServerApi

%% Metrics
class Metrics {
    prometheus-cpp::push
}

%% Server
Rsk <|-- Server
Auth <|-- Server
Networking <|-- Server
Metrics <|-- Server

%% CoreClient
AccessManagerApi <|-- CoreClient
StorageFacilityApi <|-- CoreClient
Server <|-- CoreClient

%% Client
Content <|-- Client
RegistrationServerApi <|-- Client
CoreClient <|-- Client
AuthserverApi <|-- Client
KeyServerApi <|-- Client
```

Server libraries and executable targets have not been graphed.
