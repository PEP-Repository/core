#!/usr/bin/env bash

set -o errexit
set -o pipefail
#set -o xtrace
set -o nounset
set -o noglob

OPENSSLBINARY="${PEP_OPENSSL_LOCATION:-openssl}"
SCRIPTPATH=$(dirname "$0")

function help {
    echo "USAGE: pki.sh [which=all] [ca_config_file=$SCRIPTPATH/ca_ext.cnf] "
    echo
    echo "Possible values for 'which':"
    echo "  all        Create everything new: root, intermediate and leaf certificates"
    echo "  no-root    Create new intermediate and leaf certificates, but not the root"
}

function setup_exit_handlers {
    declare -a exit_handlers
    trap exit_handlers EXIT
}

# shellcheck disable=SC2329,SC2317 # Shellcheck thinks this function is not called
function exit_handlers {
    local i

    if [ -n "${exit_handlers+x}" ]; then
        # execute the exit_handlers in the reverse order of which they were added
        for ((i=${#exit_handlers[@]}-1; i>=0; --i)); do
            ${exit_handlers[$i]}
        done

        unset exit_handlers
    fi
}

function add_exit_handler {
    if [ -z ${exit_handlers+x} ]; then
        setup_exit_handlers
    fi

    exit_handlers+=("$1")
}

function setup_tmp_files {
    declare -a tmp_files
    add_exit_handler 'cleanup_tmp_files'
}

function setup_password_files {
    declare -a password_files
    add_exit_handler 'log_password_files'
}

# shellcheck disable=SC2329,SC2317 # Shellcheck thinks this function is not called
function cleanup_tmp_files {
    local i

    if [ -n "${tmp_files+x}" ]; then
        for ((i=0; i<${#tmp_files[@]}; i++)); do
            rm "${tmp_files[$i]}"
        done

        unset tmp_files
    fi
}

# shellcheck disable=SC2329,SC2317 # Shellcheck thinks this function is not called
function log_password_files {
    local i

    if [ -n "${password_files+x}" ]; then
        echo
        for ((i=0; i<${#password_files[@]}; i++)); do
            echo Leaving password file \'"${password_files[$i]}"\'. Please secure it.
        done

        unset password_files
    fi
}

function add_tmp_file {
    if [ -z ${tmp_files+x} ]; then
        setup_tmp_files
    fi

    tmp_files+=("$1")
}

function add_password_file {
    if [ -z ${password_files+x} ]; then
        setup_password_files
    fi

    password_files+=("$1")
}

function generate_private_key() {
    local key_file_name="$1"
    local key_size="$2"
    local password_file="$3"

    local password_option=""
    if [ -z "$password_file" ] ; then
         password_option="";
    else
         password_option="-aes256 -passout file:$password_file";
    fi

    # shellcheck disable=SC2086 # $password_option should NOT be quoted. We want the several options it contains to be parsed as separate arguments
    "$OPENSSLBINARY" genrsa $password_option -out "$key_file_name" "$key_size"
}

function request_certificate() {
    local subject="$1"
    local common_name="$2"
    local key_file_name="$3"
    local csr_file_name="$4"
    local password_file="$5"

    if [ -z "$password_file" ] ; then
         password_option="";
    else
         password_option="-passin file:$password_file";
    fi

    # shellcheck disable=SC2086 # $password_option should NOT be quoted. We want the several options it contains to be parsed as separate arguments
    "$OPENSSLBINARY" req $password_option -new -sha256 -key "$key_file_name" -out "$csr_file_name" \
        -subj "${subject}${subject_delimiter}OU=$common_name${subject_delimiter}CN=$common_name" \
        -addext "subjectAltName = DNS:$common_name"
}

function self_signed_certificate() {
    local ca_key_file_name="$1"
    local csr_file_name="$2"
    local cert_file_name="$3"
    local password_file="$4"

    if [ -z "$password_file" ] ; then
         password_option="";
    else
         password_option="-passin file:$password_file";
    fi

    # shellcheck disable=SC2086 # $password_option should NOT be quoted. We want the several options it contains to be parsed as separate arguments
    "$OPENSSLBINARY" x509 -req -sha256 -in "$csr_file_name" -signkey "$ca_key_file_name" \
        -days 3653 -out "$cert_file_name" \
        -extfile "$ca_config_file_name" -extensions ca $password_option
}

function sign_certificate_request() {
    local ca_key_file_name="$1"
    local ca_key_cert="$2"
    local csr_file_name="$3"
    local cert_file_name="$4"
    local extensions="$5"
    local common_name="$6"
    local password_file="$7"

    if [ -z "$password_file" ] ; then
         password_option="";
    else
         password_option="-passin file:$password_file";
    fi

    # Create a config file that includes the common name as a subject alternative name (SAN).
    # See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1537#note_23770
    local san_config_file_name
    san_config_file_name="$(basename "$csr_file_name").$(basename "$ca_config_file_name")"
    add_tmp_file "$san_config_file_name"
    cp "$ca_config_file_name" "$san_config_file_name"
    echo "DNS.2 = \"$common_name\"" >> "$san_config_file_name"

    # shellcheck disable=SC2086 # $password_option should NOT be quoted. We want the several options it contains to be parsed as separate arguments
    "$OPENSSLBINARY" x509 -req -sha256 -in "$csr_file_name" -CAkey "$ca_key_file_name" -CA "$ca_key_cert" \
     -out "$cert_file_name" -days 365 -extfile "$san_config_file_name" -extensions "$extensions"  \
     -CAcreateserial -CAserial ca_serial.srl $password_option
}

function generate_server_certficate() {
    local ca="$1"
    local server="$2"
    local extensions="$3"

    generate_private_key "$server.key" 2048 ""
    add_tmp_file "$server.csr"
    request_certificate "$subject" "$server_name" "$server.key" "$server.csr" ""
    add_tmp_file "$server.cert"
    sign_certificate_request "$ca.key" "$ca.cert" "$server.csr" "$server.cert" "$extensions" "$server_name" "$ca.password"
    cat "$server.cert" "$ca.chain" > "$server.chain"
}

function generate_server_tls_certificate() {
    local server_name="$1"

    local ca="tlsCA"

    generate_server_certficate "$ca" "TLS$server_name" "server_cert"
}

function generate_server_pep_certificate() {
    local server_name="$1"

    local ca="pepServerCA"

    generate_server_certficate "$ca" "PEP$server_name" "pep_cert"
}

function generate_server_tls_and_pep_certificate {
    local server_name="$1"

    generate_server_tls_certificate "$server_name"
    generate_server_pep_certificate "$server_name"
}

function generate_intermediate_ca_certificate {
    local parent_ca="$1"
    local intermediate_ca="$2"
    local intermediate_common_name="$3"
    local key_size="$4"
    local password="$5"

    generate_private_key "$intermediate_ca.key" "$key_size" "$password"
    add_tmp_file "$intermediate_ca.csr"
    request_certificate "$subject" "$intermediate_common_name" "$intermediate_ca.key" "$intermediate_ca.csr" "$password"
    add_tmp_file "$intermediate_ca.cert"
    sign_certificate_request "$parent_ca.key" "$parent_ca.cert" "$intermediate_ca.csr" "$intermediate_ca.cert" "intermediate_ca" "$intermediate_common_name" "$parent_ca.password"
    cat "$intermediate_ca.cert" "$parent_ca.chain" > "$intermediate_ca.chain"
}

#"$OPENSSLBINARY" genrsa -out pepServerCA.key 2048
#"$OPENSSLBINARY" req -new -sha256 -key pepServerCA.key -out pepServerCA.csr -subj "/C=NL/ST=Gelderland/L=Nijmegen/O=Radboud Universiteit/OU=Institute for Computer and Information Sciences/CN=Intermediate PEP CA - PEP servers"
#"$OPENSSLBINARY" x509 -req -in pepServerCA.csr -CAkey rootCA.key -CA rootCA.cert -out pepServerCA.cert -days 365 -extfile ca_ext.cnf -extensions intermediate_ca  -CAcreateserial -CAserial ca_serial.srl

# "$OPENSSLBINARY" genrsa -aes256 -out private/ca.key.pem 4096


# offline

# send csr

# receive cert

# check crt


# intermediate CA

# root CA

# role
# -> execute(party)

# party
# -> email, organsiation
# -> perform(role)

# ca signs crl

function main() {
    command=${1:-"all"}
    ca_config_file_name=${2:-"$SCRIPTPATH/ca_ext.cnf"}

    if [ "$#" -gt 2 ]; then
        help
        exit
    fi

    if [ ! -f "$ca_config_file_name" ]; then
        echo "$ca_config_file_name does not exist"
        help
        exit
    fi

    case "$command" in
        all)
            create_root=true ;;
        no-root)
            create_root=false ;;
        *)
            help
            exit
    esac

    # Deal with (back)slash substitution under MinGW: adapted from https://stackoverflow.com/a/31990313 . Environment detection adapted from https://stackoverflow.com/a/3466183
    unameOut="$(uname -s)"
    case "${unameOut}" in
        MINGW*) subject_prefix="//";subject_delimiter="\\";;
        *)      subject_prefix="/"; subject_delimiter="/";;
    esac

    local subject="${subject_prefix}C=NL${subject_delimiter}ST=Gelderland${subject_delimiter}L=Nijmegen${subject_delimiter}O=Radboud Universiteit"

    if [ "$create_root" = true ]; then
        add_password_file 'rootCA.password'
        "$OPENSSLBINARY" rand -base64 -out "rootCA.password" 32

        generate_private_key "rootCA.key" 4096 "rootCA.password"
        add_tmp_file 'rootCA.csr'
        request_certificate "$subject" "PEP Root CA" "rootCA.key" "rootCA.csr" "rootCA.password"
        self_signed_certificate "rootCA.key" "rootCA.csr" "rootCA.cert" "rootCA.password"
        touch "rootCA.chain"
    fi

    add_password_file 'tlsCA.password'
    "$OPENSSLBINARY" rand -base64 -out "tlsCA.password" 32

    add_password_file 'pepServerCA.password'
    "$OPENSSLBINARY" rand -base64 -out "pepServerCA.password" 32

    generate_intermediate_ca_certificate "rootCA" "tlsCA" "PEP Intermediate TLS CA" 4096 "tlsCA.password"
    generate_intermediate_ca_certificate "rootCA" "pepServerCA" "PEP Intermediate PEP Server CA" 4096 "pepServerCA.password"
    generate_intermediate_ca_certificate "rootCA" "pepClientCA" "PEP Intermediate PEP Client CA" 2048 ""

    generate_server_tls_certificate "KeyServer"
    generate_server_tls_certificate "S3"

    #  for S3 frontend
    mkdir -p s3certs
    cp TLSS3.chain s3certs/public.crt
    cp TLSS3.key s3certs/private.key

    generate_server_tls_and_pep_certificate "AccessManager"
    generate_server_tls_and_pep_certificate "Transcryptor"
    generate_server_tls_and_pep_certificate "StorageFacility"
    generate_server_tls_and_pep_certificate "RegistrationServer"
    generate_server_tls_and_pep_certificate "Authserver"

    exit 0
}

main "$@"
