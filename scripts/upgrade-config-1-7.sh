#!/usr/bin/env bash

# Upgrade config from PEP 1.6 to 1.7
# Define envvar NO_VALIDATE=1 to skip validation after upgrade

set -eu

script_dir="$(realpath "$(dirname -- "$0")")"

config_file_or_folder="$1"

upgrade_config_file() {
  local config_file="$1"
  local public_key_data="${2:-null}"
  local public_key_pseudonyms="${3:-null}"
  local access_manager_end_point="${4:-null}"

  if jq --exit-status '.CaCertificateFile' -- "$config_file" >/dev/null; then
    # Already upgraded
    return
  fi

  cp -f -- "$config_file" "$config_file.tmp"

  config_basename="$(basename -- "$config_file")"
  if [ "$config_basename" = Transcryptor.json ]; then
    # https://gitlab.pep.cs.ru.nl/pep/core/-/work_items/2801
    jq --argjson publicKeyPseudonyms "$public_key_pseudonyms" \
      --argjson accessManager "$access_manager_end_point" \
      '.PublicKeyPseudonyms = $publicKeyPseudonyms | .AccessManager = $accessManager' \
      -- "$config_file.tmp" >"$config_file.tmp2"
    mv -f -- "$config_file.tmp2" "$config_file.tmp"
  fi

  # https://gitlab.pep.cs.ru.nl/pep/core/-/work_items/2863
  jq --argjson public_key_data "$public_key_data" '
  {
    CaCertificateFile: .CACertificateFile,
  }
  + (if .ListenPort then
      {
        ListenPort: .ListenPort,
        TlsIdentity: {
          PrivateKeyFile: .TLSPrivateKeyFile,
          CertificateFile: .TLSCertificateFile
        }
      }
      + (if .PEPPrivateKeyFile then {
        SigningIdentity: {
          PrivateKeyFile: .PEPPrivateKeyFile,
          CertificateFile: .PEPCertificateFile
        }
      } else {} end)
    else {} end)
  + (
      {
        AccessManager: .AccessManager,
        Authserver: .Authserver,
        KeyServer: .KeyServer,
        RegistrationServer: .RegistrationServer,
        StorageFacility: .StorageFacility,
        Transcryptor: .Transcryptor
      }
      | with_entries(select(.value != null))
      | if . == {} then {} else {ServerEndPoints: .} end
    )
  + (if (.PublicKeyData != null or .PublicKeyPseudonyms != null) then {
      SystemPublicKeys: {
        PublicKeyData: (.PublicKeyData // $public_key_data),
        PublicKeyPseudonyms: .PublicKeyPseudonyms
      }
    } else {} end)
  + (if .KeysFile then { EnrolledPartyKeysFile: .KeysFile } else {} end)
  + (if .ClientCAPrivateKeyFile then {
      ClientCa: {
        PrivateKeyFile: .ClientCAPrivateKeyFile,
        CertificateFile: .ClientCACertificateChainFile
      }
    } else {} end)
  + (if .HTTPListenPort then { HttpListenPort: .HTTPListenPort } else {} end)
  + (if .HTTPSCertificateFile then { HttpsCertificateFile: .HTTPSCertificateFile } else {} end)
  + (if .HSM then { SystemPrivateKeysFile: .HSM.ConfigFile } else {} end)
  + (if SystemKeysFile then { SystemPrivateKeysFile: .SystemKeysFile } else {} end)
  + (pick(
      .OAuthTokenSecretFile,
      .BlocklistStoragePath,
      .ActiveGrantExpirationSeconds,
      .TokenExpirationSeconds,
      .SpoofKeyFile,
      .StorageFile,
      .GlobalConfigurationFile,
      .VerifiersFile,
      .ShadowStorageFile,
      .ShadowPublicKeyFile,
      .ProjectConfigFile,
      .StoragePath,
      .EncIdKeyFile,
      .DataSizeResolution,
      .ParallelisationWidth
    ) | with_entries(select(.value != null)))
  + (if .PageStore then {
      PageStore: (
        .PageStore |
        (
          {
            EndPoint: (.EndPoint | .Port |= tonumber),
            Credentials: .Credentials,
            WriteToBucket: .["Write-To-Bucket"],
            ReadFromBuckets: .["Read-From-Buckets"]
          }
          + (if .["Ca-Cert-Path"] then { CaCertificateFile: .["Ca-Cert-Path"] } else {} end)
          + (pick(.UseHttps, .Connections) | with_entries(select(.value != null)))
        ) as $s3 |
        if .Type == "local" then {
          Local: { DataDir: .DataDir, Bucket: .Bucket }
        } elif .Type == "s3" then {
          S3: $s3
        } elif .Type == "dual" then {
          Local: { DataDir: .DataDir, Bucket: .Bucket },
          S3: $s3
        } else . end
      )
    } else {} end)
  + (if .AuthenticationServer then {
      OAuthServer: (
        {
          RequestUrl: .AuthenticationServer.RequestURL,
          TokenUrl: .AuthenticationServer.TokenURL
        }
        + (if .AuthenticationServer.CaCertFilePath then {
            CaCertificateFile: .AuthenticationServer.CaCertFilePath
          } else {} end)
      )
    } else {} end)
  + (if .Castor then {
      Castor: (
        if .Castor.APIKeyFile then { ApiKeyFile: .Castor.APIKeyFile }
        elif .Castor.BaseURL then { BaseUrl: .Castor.BaseURL }
        else .Castor end
      )
    } else {} end)
  ' "$config_file.tmp" >"$config_file.tmp2"
  mv -f -- "$config_file.tmp2" "$config_file.tmp"

  if [ -z "${NO_VALIDATE-}" ] && ! "$script_dir/validate-config.sh" "$config_file.tmp"; then
    read -r -p "Failed to validate upgraded config $config_file.tmp (did you mean to set envvar NO_VALIDATE=1?). Proceed anyway [y/n]? " yn
    case $yn in
      [Yy]* ) ;;
      * ) exit 1 ;;
    esac
  fi
  mv -f -- "$config_file.tmp" "$config_file"
}

if [ -d "$config_file_or_folder" ]; then
  cd "$config_file_or_folder"
  upgrade_config_file ./client/ClientConfig.json
  public_key_data="$(jq .SystemPublicKeys.PublicKeyData ./client/ClientConfig.json)"
  public_key_pseudonyms="$(jq .SystemPublicKeys.PublicKeyPseudonyms ./client/ClientConfig.json)"
  access_manager_end_point="$(jq .ServerEndPoints.AccessManager ./client/ClientConfig.json)"
  
  # Find config files regardless of directory structure (e.g. pep-services subfolder).
  # Also include StorageFacility.local.json etc. for integration test config.
  find \
      -name AccessManager.json \
      -or -name Authserver.json \
      -or -name KeyServer.json \
      -or -name RegistrationServer.json \
      -or -name StorageFacility.json \
      -or -name 'StorageFacility.*.json' \
      -or -name Transcryptor.json |
  while read -r config_file; do
    upgrade_config_file "$config_file" "$public_key_data" "$public_key_pseudonyms" "$access_manager_end_point"
  done
else
  upgrade_config_file "$config_file_or_folder"
fi
