#!/bin/bash

# Define the network interface for PTP
# Parse arguments
show_help() {
    echo "Usage: $0 --interface <network_interface> [--restart] [--remove] [--help|-h]"
    echo ""
    echo "  --interface <network_interface>   (required) Network interface to use for PTP"
    echo "  --restart                        Restart the PTP services"
    echo "  --remove                         Remove/disable the PTP services"
    echo "  --help, -h                       Show this help message"
}

# Function to check if a given network interface is valid
is_valid_interface() {
    local interface="$1"

    if ip link show "$interface" &>/dev/null; then
        echo "[SETUP_LINUXPTP.SH] Valid interface: $interface"
        exit 0
    else
        echo "[SETUP_LINUXPTP.SH] Invalid interface: $interface"
        exit 1
    fi
}

INTERFACE=""
RESTART=0
REMOVE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --interface)
            INTERFACE="$2"
            shift 2
            ;;
        --restart)
            RESTART=1
            shift
            ;;
        --remove)
            REMOVE=1
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            show_help
            exit 1
            ;;
    esac
done

if [[ -z "$INTERFACE" ]]; then
    echo "Error: --interface is required."
    show_help
    exit 1
fi

# Call the function with the provided argument
is_valid_interface $INTERFACE

if [[ $RESTART -eq 1 && $REMOVE -eq 1 ]]; then
    echo "Error: --restart and --remove cannot be used together."
    show_help
    exit 1
fi

if [[ $REMOVE -eq 1 ]]; then
    systemctl stop ptp4l@$INTERFACE.service
    systemctl stop phc2sys@$INTERFACE.service
    systemctl disable ptp4l@$INTERFACE.service
    systemctl disable phc2sys@$INTERFACE.service
    echo "[SETUP_LINUXPTP.SH] PTP services removed for interface $INTERFACE"
    exit 0
fi

if [[ $RESTART -eq 1 ]]; then
    # Check if the services are available for the given interface
    if systemctl list-units --type=service --all | grep -q "ptp4l@${INTERFACE}.service"; then
        systemctl daemon-reload
        systemctl restart ptp4l@$INTERFACE.service
        systemctl restart phc2sys@$INTERFACE.service
        echo "[Done] PTP services restarted for interface $INTERFACE"
        exit 0
    else
        echo "[SETUP_LINUXPTP.SH] PTP services not found for interface $INTERFACE. Proceeding with installation..."
        # Continue to installation steps below
    fi
fi

CONFIG_FILE="/etc/linuxptp/ptp4l.conf"
TEMP_FILE="$(mktemp)"

# Define environment variables and their default values
domainNumber=${DOMAIN_NUMBER:-24}
logAnnounceInterval=${LOG_ANNOUNCE_INTERVAL:--3}
logSyncInterval=${LOG_SYNC_INTERVAL:--4}
logMinDelayReqInterval=${LOG_MIN_DELAY_REQ_INTERVAL:--4}
clock_servo=${CLOCK_SERVO:-linreg}
network_transport=${NETWORK_TRANSPORT:-UDPv4}


change_ptp_config(){
    # Use sed to modify or remove lines in the config file
    sed -E \
        -e "s/^(socket_priority)/#\1/" \
        -e "s/^(domainNumber\s+).*/\1$domainNumber/" \
        -e "s/^(logAnnounceInterval\s+).*/\1$logAnnounceInterval/" \
        -e "s/^(logSyncInterval\s+).*/\1$logSyncInterval/" \
        -e "s/^(logMinDelayReqInterval\s+).*/\1$logMinDelayReqInterval/" \
        -e "s/^(operLogSyncInterval)/#\1/" \
        -e "s/^(operLogPdelayReqInterval)/#\1/" \
        -e "s/^(masterOnly)/#\1/" \
        -e "s/^(inhibit_delay_req)/#\1/" \
        -e "s/^(clock_servo\s+).*/\1$clock_servo/" \
        -e "s/^(msg_interval_request)/#\1/" \
        -e "/^servo_num_offset_values /d" \
        -e "/^servo_offset_threshold /d" \
        -e "s/^(write_phase_mode)/#\1/" \
        -e "s/^(network_transport\s+).*/\1$network_transport/" \
        "$CONFIG_FILE" > "$TEMP_FILE"
    
    # Replace the original file with the modified file
    mv "$TEMP_FILE" "$CONFIG_FILE"
    
    #comment_out_lines "socket_priority"
    echo "[SETUP_LINUXPTP.SH] Configuration updated and saved to $CONFIG_FILE"
    
}

# Install PTP
echo "[SETUP_LINUXPTP.SH] Installing linuxptp application as Grandmaster for LLS-C1 configuration"
dpkg -l | grep -qw linuxptp ||  apt install -y linuxptp

# Check installed version
install_version=$(ptp4l -v)
echo "[SETUP_LINUXPTP.SH] Installed version: $install_version"

if [ "$install_version" = "3.1.1" ]; then
    echo "[SETUP_LINUXPTP.SH] linuxptp version '$install_version' installed successfully"

    # Call the function to modify the ptp4l.conf configuration. 
    change_ptp_config
    
    # Reload systemd daemon and enable services
    systemctl daemon-reload
    systemctl enable ptp4l@$INTERFACE.service
    systemctl enable phc2sys@$INTERFACE.service
    systemctl start ptp4l@$INTERFACE.service
    systemctl start phc2sys@$INTERFACE.service

    echo "[SETUP_LINUXPTP.SH]  PTP installed and enabled successfully"
else
    echo "[SETUP_LINUXPTP.SH] PTP version not installed correctly. Please install manually or try again."
fi
