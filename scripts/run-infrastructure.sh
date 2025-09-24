#!/usr/bin/env bash

set -o errexit
set -o pipefail
# set -o xtrace
set -o nounset
set -o noglob

function setup_exit_handlers {
    declare -a exit_handlers
    trap exit_handlers EXIT
}

function exit_handlers {
    local i

    if [ -n "${exit_handlers+x}" ]; then
        # execute the exit_handlers in the reverse order
        # of which they were added
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

function setup_exit_processes {
    declare -a process_ids
    add_exit_handler 'exit_processes'
}

function exit_processes {
    local i

    if [ -n "${process_ids+x}" ]; then
        for ((i=0; i<${#process_ids[@]}; i++)); do
            kill "${process_ids[$i]}"
        done

        unset process_ids
    fi
}

function wait_processes {
    local i

    if [ -n "${process_ids+x}" ]; then
        for ((i=0; i<${#process_ids[@]}; i++)); do
            wait "${process_ids[$i]}"
        done

        unset process_ids
    fi
}

function add_exit_process {
    if [ -z ${process_ids+x} ]; then
        setup_exit_processes
    fi

    process_ids+=("$1")
}

function setup_tmp_files {
    declare -a tmp_files
    add_exit_handler 'cleanup_tmp_files'
}

function cleanup_tmp_files {
    local i

    if [ -n "${tmp_files+x}" ]; then
        for ((i=0; i<${#tmp_files[@]}; i++)); do
            rm "${tmp_files[$i]}"
        done

        unset tmp_files
    fi
}

function add_tmp_file {
    if [ -z ${tmp_files+x} ]; then
        setup_tmp_files
    fi

    tmp_files+=("$1")
}

function start_service {
    local binary_dir="$1"
    local config_dir="$2"
    local service_name="$3"
    local service_subdir="$4"

    local service_binary
    local service_config

    service_binary_dir="${binary_dir}"/"${service_subdir}"
    service_binary="${service_binary_dir}/pep${service_name}"
    service_config_dir="${config_dir}"/"${service_subdir}"
    service_config="${service_config_dir}/${service_name}.json"

    #Start ${service_name} to the baackground and store pid for cleanup
    echo "=== Starting ${service_name} ==="
    cd -- "${service_config_dir}"
    "${service_binary}" "${service_config}"&
    pid=$!
    add_exit_process "${pid}"
    echo "${service_name}" pid = "${pid}"
    sleep 0.5
}


function absolute_directory_path {
    (cd -- "$1"; pwd)
}

function run {
    local binary_root
    local config_root

    # handle input arguments
    if [ $# -gt 2 ]; then
        echo "useage: ./run-infrastructure.sh [binary_root] [config_root]"
        echo "    binary_root is optional (defaults to current directory)."
        echo "    config_root is optional (defaults to current directory)."
        exit 1
    fi


    # determine binary_root
    if [ -n "${1+x}" ]; then
        binary_root=$(absolute_directory_path "$1")
    else
        binary_root="$PWD"
    fi

    # determine config_root
    if [ -n "${2+x}" ]; then
        config_root=$(absolute_directory_path "$2")
    else
        config_root="$PWD"
    fi

    start_service "${binary_root}" "${config_root}" \
        "StorageFacility" "storagefacility"
    start_service "${binary_root}" "${config_root}" \
        "KeyServer" "keyserver"
    start_service "${binary_root}" "${config_root}" \
        "Transcryptor" "transcryptor"
    start_service "${binary_root}" "${config_root}" \
        "AccessManager" "accessmanager"
    start_service "${binary_root}" "${config_root}" \
        "RegistrationServer" "registrationserver"
    start_service "${binary_root}" "${config_root}" \
        "Authserver" "authserver"
}

function stdin_is_terminal {
    # test for interactive session (stdin is associated with a terminal)
    [ -t 0 ]
}

function run_interactive {
    echo "Interactive"

    run "$@"

    # Wait for user input before exiting infrastructure
    read -r -p "Press [Enter] key to kill infrastructure..."
}

function run_non_interactive {
    run "$@"

    wait_processes
}

function run_no_daemon {
    if stdin_is_terminal; then
        run_interactive "$@"
    else
        echo run_non_interactive
        run_non_interactive "$@"
    fi
}

# this is not full daemonisation (setsid not available on MacOS),
# but it'll have to do for now.
function run_daemon_child {
    local build_directory_root
    build_directory_root=$(absolute_directory_path "${1:-$PWD}")

    # redirect standard streams
    exec >"${build_directory_root}/daemon.out"
    exec 2>"${build_directory_root}/daemon.err"
    exec 0</dev/null

    # save this script's process id
    echo "$$" > "${build_directory_root}/daemon.pid"
    add_tmp_file "${build_directory_root}/daemon.pid"

    run_non_interactive "${build_directory_root}"
}

function run_daemon {
    # relaunch current script with detached standard streams
    "$0" "--daemon-run-child" "$@" < /dev/null > /dev/null 2>&1 &
    disown
    echo "Daemon started. Use \"kill $!\" to stop it"
}

function run_default {
    if stdin_is_terminal; then
        run_no_daemon "$@"
    else
        run_daemon "$@"
    fi
}

function main {
    if [ -z ${1+x} ]; then
        # no first argument
        run_default "$@"
    else
        # check first argument
        case "$1" in
            "--daemon")
                shift
                run_daemon "$@"
                ;;
            "--daemon-run-child")
                shift
                run_daemon_child "$@"
                ;;
            "--no-daemon")
                shift
                run_no_daemon "$@"
                ;;
            *)
                run_default "$@"
                ;;
        esac
    fi
}

main "$@"
