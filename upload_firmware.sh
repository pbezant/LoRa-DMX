#!/bin/bash

# Upload script for Heltec LoRa 32 V3 firmware

# Define colors for better output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== ESP32 Firmware Upload Script ===${NC}"
echo -e "${YELLOW}This script will upload the DMX LoRa firmware to your Heltec LoRa 32 V3 board.${NC}"
echo ""

# Check if esptool.py is available
if ! command -v esptool.py &> /dev/null; then
    echo -e "${RED}Error: esptool.py not found.${NC}"
    echo "Please make sure it's installed. You can install it with:"
    echo "pip install esptool"
    exit 1
fi

# Detect possible ports
echo "Detecting available ports..."
ports=$(ls /dev/cu.* 2>/dev/null)

if [ -z "$ports" ]; then
    echo -e "${RED}No serial ports found!${NC}"
    echo "Please make sure your device is connected."
    exit 1
fi

echo "Available ports:"
select port in $ports; do
    if [ -n "$port" ]; then
        break
    else
        echo -e "${RED}Invalid selection. Please try again.${NC}"
    fi
done

echo ""
echo -e "${YELLOW}IMPORTANT: To put your Heltec board in bootloader mode:${NC}"
echo "1. Press and hold the BOOT button (IO0)"
echo "2. Press and release the RESET button while still holding BOOT"
echo "3. Release the BOOT button"
echo ""
echo -e "${YELLOW}Please put your board in bootloader mode now...${NC}"
read -p "Press Enter when ready..."

# Binary file location
FIRMWARE=".pio/build/heltec_wifi_lora_32_V3/firmware.bin"

if [ ! -f "$FIRMWARE" ]; then
    echo -e "${RED}Error: Firmware binary not found: $FIRMWARE${NC}"
    echo "Please make sure you've built the project first."
    exit 1
fi

# Upload the firmware
echo -e "${YELLOW}Uploading firmware to $port...${NC}"
esptool.py --chip esp32s3 --port $port --baud 460800 --before default_reset --after hard_reset write_flash -z 0x0 $FIRMWARE

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Upload successful!${NC}"
    echo "The device should reboot automatically. If not, press the RESET button."
    echo ""
    echo -e "${YELLOW}To view the device output, use:${NC}"
    echo -e "screen $port 115200"
    echo -e "or"  
    echo -e "minicom -D $port -b 115200"
    echo -e "${YELLOW}(Press Ctrl+A followed by K to exit screen, or Ctrl+A followed by X to exit minicom)${NC}"
else
    echo -e "${RED}Upload failed!${NC}"
    echo "Please try again and make sure your device is in bootloader mode."
fi 