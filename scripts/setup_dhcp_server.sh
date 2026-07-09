#!/bin/bash

## Only run this script if you wish to setup the DHCP server for providing IP address to the RU. 
# Parse arguments
REMOVE_DHCP_SERVER=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --interface)
            DHCP_INTERFACE="$2"
            shift 2
            ;;
        --remove)
            REMOVE_DHCP_SERVER=1
            shift
            ;;
        -h|--help)
            echo "Usage: $0 --interface <interface> [--remove]"
            echo "  --interface <interface>   Specify the network interface for DHCP server"
            echo "  --remove                  Remove the DHCP server and its configuration"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 --interface <interface> [--remove]"
            exit 1
            ;;
    esac
done

if [[ $REMOVE_DHCP_SERVER -eq 1 ]]; then
    echo "Removing DHCP server and its configuration..."
    systemctl stop isc-dhcp-server
    apt remove -y isc-dhcp-server
    apt purge -y isc-dhcp-server
    rm -f /etc/dhcp/dhcpd.conf /etc/default/isc-dhcp-server
    echo "DHCP server removed."
    exit 0
fi

if [[ -z "$DHCP_INTERFACE" ]]; then
    echo "Error: --interface argument is required."
    echo "Usage: $0 --interface <interface> [--remove]"
    exit 1
fi

dpkg -l | grep -qw isc-dhcp-server || apt install -y isc-dhcp-server

# Modify /etc/dhcp/dhcpd.conf
sed -i 's/^default-lease-time.*/default-lease-time 300;/' /etc/dhcp/dhcpd.conf
sed -i 's/^max-lease-time.*/max-lease-time 600;/' /etc/dhcp/dhcpd.conf
sed -i 's/^#authoritative;/authoritative;/' /etc/dhcp/dhcpd.conf
sed -i 's/^option domain-name /#option domain-name /' /etc/dhcp/dhcpd.conf
sed -i 's/^option domain-name-servers /#option domain-name-servers /' /etc/dhcp/dhcpd.conf

grep -q 'subnet 192.168.4.0 netmask 255.255.255.0' /etc/dhcp/dhcpd.conf || echo -e '\nsubnet 192.168.4.0 netmask 255.255.255.0 {\n    range 192.168.4.20 192.168.4.50;\n}' >> /etc/dhcp/dhcpd.conf
echo "Setting up DHCP server on interface: $DHCP_INTERFACE"
echo "DHCP configuration:"
echo "  Interface: $DHCP_INTERFACE"
echo "  Subnet: 192.168.4.0/24"
echo "  Range: 192.168.4.20 - 192.168.4.50"
echo "  Default lease time: 300"
echo "  Max lease time: 600"

# Modify /etc/default/isc-dhcp-server
sed -i "s/^INTERFACESv4=.*/INTERFACESv4=\"$DHCP_INTERFACE\"/" /etc/default/isc-dhcp-server

systemctl restart isc-dhcp-server
