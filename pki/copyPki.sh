#!/usr/bin/env bash

targetdir=${1}

mkdir -p "$targetdir/accessmanager" "$targetdir/authserver" "$targetdir/registrationserver" "$targetdir/transcryptor" "$targetdir/keyserver" "$targetdir/storagefacility"
cp PEPAccessManager.chain  PEPAccessManager.key  TLSAccessManager.chain  TLSAccessManager.key "$targetdir/accessmanager/"
cp PEPAuthserver.chain  PEPAuthserver.key  TLSAuthserver.chain  TLSAuthserver.key "$targetdir/authserver/"
cp PEPRegistrationServer.chain  PEPRegistrationServer.key  TLSRegistrationServer.chain  TLSRegistrationServer.key "$targetdir/registrationserver/"
cp PEPTranscryptor.chain  PEPTranscryptor.key  TLSTranscryptor.chain  TLSTranscryptor.key "$targetdir/transcryptor/"
cp pepClientCA.chain  pepClientCA.key  TLSKeyServer.chain  TLSKeyServer.key "$targetdir/keyserver/"
cp -r s3certs/  PEPStorageFacility.chain  PEPStorageFacility.key  TLSStorageFacility.chain  TLSStorageFacility.key "$targetdir/storagefacility/"
