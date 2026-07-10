#!/bin/bash

ISOLATED_CORES=""

print_help() {
    echo "Usage: $0 --isolated-cores <cores>"
    echo "Options:"
    echo "  --isolated-cores <cores>   Comma-separated list of CPU cores to isolate (e.g., 1,2,3)"
    echo "  -h, --help                 Show this help message"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --isolated-cores)
            ISOLATED_CORES="$2"
            shift 2
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_help
            exit 1
            ;;
    esac
done

if [[ -z "$ISOLATED_CORES" ]]; then
    echo "Error: --isolated-cores is required."
    print_help
    exit 1
fi

dpkg -l | grep -qw tuned || apt install -y tuned

# Modify /etc/tuned/realtime-variables.conf
sed -i "s/^isolated_cores=.*/isolated_cores=$ISOLATED_CORES/" /etc/tuned/realtime-variables.conf

# Apply the tuned profile
tuned-adm profile realtime
tuned-adm profile
