#!/bin/bash
# TODO: To improve the script for 4x4 nAntenna configuration for VVDN RU. - Safwan. 

# Parse arguments
show_help() {
    echo "Usage: $0 --username USER --ip-address IP --antenna-config N"
    echo "Options:"
    echo "  --username USER        Username for SSH login"
    echo "  --ip-address IP        IP address of the remote unit"
    echo "  --antenna-config N     Antenna configuration (1, 2, or 4)"
    echo "  -h, --help             Show this help message and exit"
}

while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        --username)
            RU_USERNAME="$2"
            shift; shift
            ;;
        --ip-address)
            RU_IP_ADDRESS="$2"
            shift; shift
            ;;
        --antenna-config)
            nAntenna="$2"
            shift; shift
            ;;
        -h|--help)
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

# Ensure nAntenna is set and valid
if [ -z "$nAntenna" ]; then
    echo "Error: nAntenna variable is not set."
    exit 1
fi

setup_nAntenna_1_1(){
    remote_command="xml_parser 1x1-config.xml"
    config_file="1x1-config.xml"
}

setup_nAntenna_2_2(){
    remote_command="xml_parser 2x2-config.xml"
    config_file="2x2-config.xml"
}

setup_nAntenna_4_4(){
    remote_command="xml_parser 2x2-config.xml"
    config_file="4x4-config.xml"
}

if [ $nAntenna -eq 1 ]; then
    setup_nAntenna_1_1
elif [ $nAntenna -eq 2 ]; then
    setup_nAntenna_2_2
fi
elif [ $nAntenna -eq 4 ]; then
    setup_nAntenna_4_4
else
    echo "Error: Invalid nAntenna value. Please set nAntenna to 1, 2, or 4."
    exit 1
fi

# !! This script expects SSH login over public key already setup. !!

# Check if the XML file exists on the remote unit
ssh "$RU_USERNAME@$RU_IP_ADDRESS" "[ -f $config_file ]" 

if [ $? -ne 0 ]; then
    echo "$config_file not found on remote unit. Creating it..."
    
    # If the missing file is eaxcid_modify_siso.xml, create it with default content
    if [ "$config_file" == "1x1-config.xml" ]; then
        ssh "$RU_USERNAME@$RU_IP_ADDRESS" "cat > 1x1-config.xml <<EOF
<vvdn_config>
    <prach_layer0_PCID>1</prach_layer0_PCID>
    <prach_layer1_PCID>3</prach_layer1_PCID>
    <prach_layer2_PCID>6</prach_layer2_PCID>
    <prach_layer3_PCID>7</prach_layer3_PCID>
    <pxsch_layer0_PCID>0</pxsch_layer0_PCID>
    <pxsch_layer1_PCID>2</pxsch_layer1_PCID>
    <pxsch_layer2_PCID>4</pxsch_layer2_PCID>
    <pxsch_layer3_PCID>5</pxsch_layer3_PCID>
</vvdn_config>
EOF"
    fi
    if [ "$config_file" == "2x2-config.xml" ]; then
        ssh "$RU_USERNAME@$RU_IP_ADDRESS" "cat > 2x2-config.xml <<EOF
<vvdn_config>
    <prach_layer0_PCID>2</prach_layer0_PCID>
    <prach_layer1_PCID>3</prach_layer1_PCID>
    <prach_layer2_PCID>6</prach_layer2_PCID>
    <prach_layer3_PCID>7</prach_layer3_PCID>
    <pxsch_layer0_PCID>0</pxsch_layer0_PCID>
    <pxsch_layer1_PCID>1</pxsch_layer1_PCID>
    <pxsch_layer2_PCID>4</pxsch_layer2_PCID>
    <pxsch_layer3_PCID>5</pxsch_layer3_PCID>
</vvdn_config>
EOF"
    fi
    if [ "$config_file" == "4x4-config.xml" ]; then
        ssh "$RU_USERNAME@$RU_IP_ADDRESS" "cat > 4x4-config.xml <<EOF
<vvdn_config>
    <prach_layer0_PCID>4</prach_layer0_PCID>
    <prach_layer1_PCID>5</prach_layer1_PCID>
    <prach_layer2_PCID>6</prach_layer2_PCID>
    <prach_layer3_PCID>7</prach_layer3_PCID>
    <pxsch_layer0_PCID>0</pxsch_layer0_PCID>
    <pxsch_layer1_PCID>1</pxsch_layer1_PCID>
    <pxsch_layer2_PCID>2</pxsch_layer2_PCID>
    <pxsch_layer3_PCID>3</pxsch_layer3_PCID>
</vvdn_config>
EOF"
    fi
fi

#SSH using public key into the RU and run the remote command 
ssh "$RU_USERNAME@$RU_IP_ADDRESS" "$remote_command"
