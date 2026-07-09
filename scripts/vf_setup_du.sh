#!/bin/bash
## Currently this script only works for creating virtual functions on Intel XXV E810 DA4T NIC and for binding igb_uio driver, 
## Further modificatio of this script is required for it to work with other NIC and drivers. -- Safwan

# Default values
FH_INTERFACE=""
FH_MAC_1="00:11:22:33:44:66"
FH_MAC_2="00:11:22:33:44:77"
FH_CU_VLAN=100
FH_MTU=9600
FH_DRIVER="vfio-pci"

print_usage() {
    echo "Usage: $0 --interface <iface> [--mac1 <mac>] [--mac2 <mac>] [--vlan <vlan>] [--mtu <mtu>] [--driver <driver>]"
    echo "       $0 --remove <iface>"
    echo ""
    echo "Options:"
    echo "  --interface   Network interface to use (required for VF creation)"
    echo "  --mac1        MAC address for VF 0 (default: $FH_MAC_1)"
    echo "  --mac2        MAC address for VF 1 (default: $FH_MAC_2)"
    echo "  --vlan        VLAN ID for VFs (default: $FH_CU_VLAN)"
    echo "  --mtu         MTU for VFs (default: $FH_MTU)"
    echo "  --driver      Driver to bind VFs to (default: $FH_DRIVER, options: vfio-pci, igb_uio)"
    echo "  --remove      Remove VFs from specified interface"
    echo "  --help        Show this help message"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --interface)
            FH_INTERFACE="$2"
            shift 2
            ;;
        --mac1)
            FH_MAC_1="$2"
            shift 2
            ;;
        --mac2)
            FH_MAC_2="$2"
            shift 2
            ;;
        --vlan)
            FH_CU_VLAN="$2"
            shift 2
            ;;
        --mtu)
            FH_MTU="$2"
            shift 2
            ;;
        --driver)
            FH_DRIVER="$2"
            shift 2
            ;;
        --remove)
            break
            ;;
        --help | -h)
            print_usage
            exit 0
            ;;
        *)
            break
            ;;
    esac
done

if [[ -z "$FH_INTERFACE" && "$1" != "--remove" ]]; then
    echo "Error: --interface argument is required."
    print_usage
    exit 1
fi

echo ----------------------- vf-setup --------------------------------

if [ $# -ne 0 ]; then
    if [ "$1" = "--remove" ]; then
        # Check if a valid interface name is provided
        if [ -n "$2" ]; then
            # Get the bus ID of the interface
            FH_NIC_BUS_ID=$(sudo lshw -c network -businfo | awk -v iface="$2" '$2 == iface {print substr($1, 5)}')
            
            if [ -n "$FH_NIC_BUS_ID" ]; then
                echo 0 > /sys/bus/pci/devices/$FH_NIC_BUS_ID/sriov_numvfs
                echo "SR-IOV VFs removed from interface: $2"
                exit 0
            else
                echo "Error: Could not determine Bus ID for interface: $2"
                exit 1
            fi
        else
            echo "Error: No interface name provided for --remove."
            exit 1
        fi
    else
        echo "Invalid argument. Use --remove <interface> or run without arguments to create VFs using the interface in .env"
        exit 1
    fi
fi

FH_NIC_BUS_ID=$(sudo lshw -c network -businfo | awk -v iface="$FH_INTERFACE" '$2 == iface {print substr($1, 5)}')
echo "[INFO] Creating 2 virtual functions for interface $FH_INTERFACE with businfo $FH_NIC_BUS_ID"

echo 2 > /sys/bus/pci/devices/$FH_NIC_BUS_ID/sriov_numvfs
sleep 1

# Handle driver-specific setup
if [[ "$FH_DRIVER" == "igb_uio" ]]; then
    echo "[INFO] Loading drivers into kernel modules."
    if lsmod | grep -q uio; then
        echo "[INFO] UIO module is loaded"
    else
        echo "[INFO] UIO module is not loaded"
        modprobe uio
        echo "[INFO] UIO module is loaded sucessfully!"
    fi
    if lsmod | grep -q igb_uio; then
        echo "[INFO] IGB_UIO module is loaded"
    else
        echo "[INFO] IGB_UIO module is not loaded. Searching for igb_uio.ko..."

        # Try to locate igb_uio.ko in common directories
        IGB_UIO_PATH=$(find "$HOME/dpdk-kmods" -name igb_uio.ko 2>/dev/null | head -n 1)

        if [ -n "$IGB_UIO_PATH" ]; then
            echo "[INFO] Found igb_uio.ko at $IGB_UIO_PATH"
        else
            echo "[INFO] igb_uio.ko not found. Cloning and building it..."

            # Ask for permission to clone and build
            read -p "Do you want to clone and build the igb_uio module? (y/N): " user_response
            user_response=${user_response,,}  # Convert to lowercase

            if [[ "$user_response" != "y" ]]; then
                echo "User denied permission. Exiting..."
                exit 1
            fi

            # Clone dpdk-kmods repository
            cd "$HOME"
            if [[ ! -d "dpdk-kmods" ]]; then
                git clone http://dpdk.org/git/dpdk-kmods || { echo "Failed to clone dpdk-kmods"; exit 1; }
            else
                echo "dpdk-kmods already exists. Skipping clone."
            fi

            # Compile igb_uio driver
            cd "$HOME/dpdk-kmods/linux/igb_uio"
            make || { echo "Failed to compile igb_uio"; exit 1; }

            # Set path to newly built module
            IGB_UIO_PATH="$HOME/dpdk-kmods/linux/igb_uio/igb_uio.ko"
        fi

        # Load the igb_uio module
        echo "Loading igb_uio module..."
        insmod "$IGB_UIO_PATH" || { echo "Failed to load igb_uio module"; exit 1; }

        echo "IGB_UIO module successfully loaded"
    fi
elif [[ "$FH_DRIVER" == "vfio-pci" ]]; then
    echo "[INFO] Using vfio-pci driver. Loading vfio modules..."
    modprobe vfio || { echo "Failed to load vfio module"; exit 1; }
    modprobe vfio-pci || { echo "Failed to load vfio-pci module"; exit 1; }
    echo "[INFO] vfio-pci modules loaded successfully"
else
    echo "[ERROR] Unknown driver: $FH_DRIVER"
    exit 1
fi

sleep 1
echo "[INFO] Setting MAC addresses for the virtual functions."
ip link set $FH_INTERFACE vf 0 mac $FH_MAC_1 vlan $FH_CU_VLAN trust on mtu $FH_MTU
ip link set $FH_INTERFACE vf 0 spoofchk off
sleep 1
ip link set $FH_INTERFACE vf 1 mac $FH_MAC_2 vlan $FH_CU_VLAN trust on mtu $FH_MTU
ip link set $FH_INTERFACE vf 1 spoofchk off

if ip link show "$FH_INTERFACE"v0 &>/dev/null; then
    ip link set "$FH_INTERFACE"v0 down
fi

if ip link show "$FH_INTERFACE"v1 &>/dev/null; then
    ip link set "$FH_INTERFACE"v1 down
fi

# Find Virtual Functions associated with this Physical Function using sysfs
VFS_PATH="/sys/bus/pci/devices/$FH_NIC_BUS_ID"

if [[ ! -d "$VFS_PATH" ]]; then
    echo "[ERROR] PCI device path $VFS_PATH not found."
    exit 1
fi

# Get Virtual Function Bus IDs
bus_ids=($(ls "$VFS_PATH" | grep -E 'virtfn[0-9]+' | xargs -I {} readlink "$VFS_PATH/{}" | awk -F'/' '{print $NF}'))

# Assign the bus IDs to individual variables if needed
FH_NIC_VIRT1_BUS_ID=${bus_ids[0]}
FH_NIC_VIRT2_BUS_ID=${bus_ids[1]}

sleep 1
echo "[INFO] The bus IDs of Virtual interfaces are $FH_NIC_VIRT1_BUS_ID $FH_NIC_VIRT2_BUS_ID"

# An empty list for VFs that need binding
VFS_TO_BIND=""

for VF_BUS_ID in "$FH_NIC_VIRT1_BUS_ID" "$FH_NIC_VIRT2_BUS_ID"; do
    DRIVER_PATH="/sys/bus/pci/devices/$VF_BUS_ID/driver"
    
    if [[ -e "$DRIVER_PATH" ]] && [[ "$(readlink -f $DRIVER_PATH)" == *"$FH_DRIVER"* ]]; then
        echo "[INFO] VF $VF_BUS_ID is already bound to $FH_DRIVER. Skipping."
    else
        VFS_TO_BIND+=" $VF_BUS_ID"
    fi
done

# If there are VFs to bind, run dpdk-devbind.py
if [[ -n "$VFS_TO_BIND" ]]; then
    echo "[INFO] Binding VFs: $VFS_TO_BIND to $FH_DRIVER"
    dpdk-devbind.py -b $FH_DRIVER $VFS_TO_BIND
    echo "[INFO] Virtual functions bound to $FH_DRIVER."
else
    echo "[INFO] No VFs need binding."
fi

echo ----------------------- VFs setup Done. ------------------------------------