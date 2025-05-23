#include "LoRaWan_APP.h"
#include "secrets.h"
#include "driver/sx126x.h"
#include "radio_driver.h"

// LoRaWAN global variables required by Heltec library

// Device credentials
uint8_t devEui[] = { 0x90, 0xcf, 0xf8, 0x68, 0xef, 0x8b, 0xd4, 0xcc };
uint8_t appEui[] = { 0xED, 0x73, 0x32, 0x20, 0xD2, 0xA9, 0xF1, 0x33 };
uint8_t appKey[] = { 0xf7, 0xed, 0xcf, 0xe4, 0x61, 0x7e, 0x66, 0x70, 
                     0x16, 0x65, 0xa1, 0x3a, 0x2b, 0x76, 0xdd, 0x52 };

// For ABP mode (not used in OTAA but required for compilation)
uint8_t nwkSKey[] = { 0xf7, 0xed, 0xcf, 0xe4, 0x61, 0x7e, 0x66, 0x70, 
                      0x16, 0x65, 0xa1, 0x3a, 0x2b, 0x76, 0xdd, 0x52 };
uint8_t appSKey[] = { 0xf7, 0xed, 0xcf, 0xe4, 0x61, 0x7e, 0x66, 0x70, 
                      0x16, 0x65, 0xa1, 0x3a, 0x2b, 0x76, 0xdd, 0x52 };
uint32_t devAddr = 0x26011234; // Device address for ABP mode

// LoRaWAN configuration
bool overTheAirActivation = true; // Use OTAA
DeviceClass_t loraWanClass = CLASS_C;  // Changed to Class C
bool loraWanAdr = true; // Enable adaptive data rate
bool isTxConfirmed = false; // Use unconfirmed uplinks
uint8_t appPort = 2; // Application port
uint8_t confirmedNbTrials = 4; // Number of retries for confirmed messages

// Regional settings
LoRaMacRegion_t loraWanRegion = LORAMAC_REGION_US915;

// Timing settings
uint32_t appTxDutyCycle = 15000; // 15 seconds

// Channel mask for US915 - enable channels 8-15 (subband 2)
uint16_t userChannelsMask[6] = { 0xFF00, 0x0000, 0x0000, 0x0000, 0x0002, 0x0000 };  // Updated for channel 65

// Radio events function (required by Heltec library)
void RadioOnDioIrq(void) {
    Radio.IrqProcess();
}

// Radio driver structure for Heltec LoRaWAN library
const struct Radio_s Radio = {
    .Init = RadioInit,
    .GetStatus = RadioGetStatus,
    .SetModem = RadioSetModem,
    .SetChannel = RadioSetChannel,
    .IsChannelFree = RadioIsChannelFree,
    .Random = RadioRandom,
    .SetRxConfig = RadioSetRxConfig,
    .SetTxConfig = RadioSetTxConfig,
    .CheckRfFrequency = RadioCheckRfFrequency,
    .TimeOnAir = RadioTimeOnAir,
    .Send = RadioSend,
    .Sleep = RadioSleep,
    .Standby = RadioStandby,
    .Rx = RadioRx,
    .StartCad = RadioStartCad,
    .SetTxContinuousWave = RadioSetTxContinuousWave,
    .Rssi = RadioRssi,
    .Write = RadioWrite,
    .Read = RadioRead,
    .WriteBuffer = RadioWriteBuffer,
    .ReadBuffer = RadioReadBuffer,
    .SetMaxPayloadLength = RadioSetMaxPayloadLength,
    .SetPublicNetwork = RadioSetPublicNetwork,
    .GetWakeupTime = RadioGetWakeupTime,
    .IrqProcess = RadioIrqProcess,
    .RxBoosted = RadioRxBoosted,
    .SetRxDutyCycle = RadioSetRxDutyCycle
}; 