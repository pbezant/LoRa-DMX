#include "radio_driver.h"
#include "driver/sx126x.h"
#include "radio/radio.h"
#include "Arduino.h"

// SX126x radio constants
#define SLEEP_START_COLD 0x00
#define STDBY_RC        0x01
#define PACKET_TYPE_GFSK 0x00
#define PACKET_TYPE_LORA 0x01
#define LORA_PACKET_VARIABLE_LENGTH 0x00
#define LORA_PACKET_FIXED_LENGTH 0x01
#define LORA_CRC_ON  0x01
#define LORA_CRC_OFF 0x00
#define LORA_IQ_NORMAL   0x00
#define LORA_IQ_INVERTED 0x01
#define IRQ_NONE 0x0000
#define RADIO_RAMP_40_US 0x06
#define REG_LR_SYNCWORD 0x0740

// Forward declarations
void RadioOnDioIrq(void);

// Basic radio operations
int RadioInit(RadioEvents_t *events) {
    SX126xInit(events);
    SX126xSetStandby((RadioStandbyModes_t)STDBY_RC);
    SX126xSetPacketType((RadioPacketTypes_t)PACKET_TYPE_LORA);
    SX126xSetRegulatorMode((RadioRegulatorMode_t)USE_DCDC);
    return 0;
}

RadioState_t RadioGetStatus(void) {
    RadioState_t state = RF_IDLE;
    RadioStatus_t status = SX126xGetStatus();
    
    switch(status.Fields.ChipMode) {
        case MODE_TX:
            state = RF_TX_RUNNING;
            break;
        case MODE_RX:
            state = RF_RX_RUNNING;
            break;
        case MODE_CAD:
            state = RF_CAD;
            break;
        default:
            state = RF_IDLE;
            break;
    }
    return state;
}

void RadioSetModem(RadioModems_t modem) {
    SX126xSetPacketType(modem == MODEM_LORA ? (RadioPacketTypes_t)PACKET_TYPE_LORA : (RadioPacketTypes_t)PACKET_TYPE_GFSK);
}

void RadioSetChannel(uint32_t freq) {
    SX126xSetRfFrequency(freq);
}

bool RadioIsChannelFree(RadioModems_t modem, uint32_t freq, int16_t rssiThresh, uint32_t maxCarrierSenseTime) {
    bool isFree = true;
    int16_t rssi = 0;
    uint32_t startTime = millis();

    RadioSetChannel(freq);
    RadioRx(0);

    // Perform carrier sense for maxCarrierSenseTime
    while((millis() - startTime) < maxCarrierSenseTime) {
        rssi = RadioRssi(modem);
        if(rssi > rssiThresh) {
            isFree = false;
            break;
        }
    }

    RadioSleep();
    return isFree;
}

uint32_t RadioRandom(void) {
    uint8_t buf[4];
    SX126xReadRegisters(RANDOM_NUMBER_GENERATORBASEADDR, buf, 4);
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

// Configuration
void RadioSetRxConfig(RadioModems_t modem, uint32_t bandwidth, uint32_t datarate,
                     uint8_t coderate, uint32_t bandwidthAfc, uint16_t preambleLen,
                     uint16_t symbTimeout, bool fixLen, uint8_t payloadLen,
                     bool crcOn, bool freqHopOn, uint8_t hopPeriod,
                     bool iqInverted, bool rxContinuous) {
    if (modem == MODEM_LORA) {
        ModulationParams_t modulationParams = { 0 };
        PacketParams_t packetParams = { 0 };

        modulationParams.PacketType = (RadioPacketTypes_t)PACKET_TYPE_LORA;
        modulationParams.Params.LoRa.Bandwidth = (RadioLoRaBandwidths_t)bandwidth;
        modulationParams.Params.LoRa.CodingRate = (RadioLoRaCodingRates_t)coderate;
        modulationParams.Params.LoRa.SpreadingFactor = (RadioLoRaSpreadingFactors_t)datarate;

        packetParams.PacketType = (RadioPacketTypes_t)PACKET_TYPE_LORA;
        packetParams.Params.LoRa.PreambleLength = preambleLen;
        packetParams.Params.LoRa.HeaderType = fixLen ? (RadioLoRaPacketLengthsMode_t)LORA_PACKET_FIXED_LENGTH : (RadioLoRaPacketLengthsMode_t)LORA_PACKET_VARIABLE_LENGTH;
        packetParams.Params.LoRa.PayloadLength = payloadLen;
        packetParams.Params.LoRa.CrcMode = crcOn ? (RadioLoRaCrcModes_t)LORA_CRC_ON : (RadioLoRaCrcModes_t)LORA_CRC_OFF;
        packetParams.Params.LoRa.InvertIQ = iqInverted ? (RadioLoRaIQModes_t)LORA_IQ_INVERTED : (RadioLoRaIQModes_t)LORA_IQ_NORMAL;

        SX126xSetModulationParams(&modulationParams);
        SX126xSetPacketParams(&packetParams);
    }
}

void RadioSetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
                     uint32_t bandwidth, uint32_t datarate,
                     uint8_t coderate, uint16_t preambleLen,
                     bool fixLen, bool crcOn, bool freqHopOn,
                     uint8_t hopPeriod, bool iqInverted, uint32_t timeout) {
    if (modem == MODEM_LORA) {
        ModulationParams_t modulationParams = { 0 };
        PacketParams_t packetParams = { 0 };

        modulationParams.PacketType = (RadioPacketTypes_t)PACKET_TYPE_LORA;
        modulationParams.Params.LoRa.Bandwidth = (RadioLoRaBandwidths_t)bandwidth;
        modulationParams.Params.LoRa.CodingRate = (RadioLoRaCodingRates_t)coderate;
        modulationParams.Params.LoRa.SpreadingFactor = (RadioLoRaSpreadingFactors_t)datarate;

        packetParams.PacketType = (RadioPacketTypes_t)PACKET_TYPE_LORA;
        packetParams.Params.LoRa.PreambleLength = preambleLen;
        packetParams.Params.LoRa.HeaderType = fixLen ? (RadioLoRaPacketLengthsMode_t)LORA_PACKET_FIXED_LENGTH : (RadioLoRaPacketLengthsMode_t)LORA_PACKET_VARIABLE_LENGTH;
        packetParams.Params.LoRa.PayloadLength = 255;
        packetParams.Params.LoRa.CrcMode = crcOn ? (RadioLoRaCrcModes_t)LORA_CRC_ON : (RadioLoRaCrcModes_t)LORA_CRC_OFF;
        packetParams.Params.LoRa.InvertIQ = iqInverted ? (RadioLoRaIQModes_t)LORA_IQ_INVERTED : (RadioLoRaIQModes_t)LORA_IQ_NORMAL;

        SX126xSetModulationParams(&modulationParams);
        SX126xSetPacketParams(&packetParams);
        SX126xSetTxParams(power, (RadioRampTimes_t)RADIO_RAMP_40_US);
    }
}

// Operation modes
void RadioSleep(void) {
    SleepParams_t params = { 0 };
    params.Fields.WarmStart = 1;
    SX126xSetSleep(params);
}

void RadioStandby(void) {
    SX126xSetStandby((RadioStandbyModes_t)STDBY_RC);
}

void RadioRx(uint32_t timeout) {
    SX126xSetRx(timeout);
}

void RadioStartCad(uint8_t symbols) {
    SX126xSetCad();
}

void RadioSetTxContinuousWave(uint32_t freq, int8_t power, uint16_t time) {
    SX126xSetRfFrequency(freq);
    SX126xSetTxParams(power, RADIO_RAMP_40_US);
    SX126xSetTxContinuousWave();
}

// Data operations
void RadioSend(uint8_t *buffer, uint8_t size) {
    SX126xSendPayload(buffer, size, 0);
}

void RadioWrite(uint16_t addr, uint8_t data) {
    SX126xWriteRegister(addr, data);
}

uint8_t RadioRead(uint16_t addr) {
    uint8_t data;
    SX126xReadRegister(addr, &data);
    return data;
}

void RadioWriteBuffer(uint16_t addr, uint8_t *buffer, uint8_t size) {
    SX126xWriteRegisters(addr, buffer, size);
}

void RadioReadBuffer(uint16_t addr, uint8_t *buffer, uint8_t size) {
    SX126xReadRegisters(addr, buffer, size);
}

// Advanced features
bool RadioCheckRfFrequency(uint32_t frequency) {
    return true; // SX126x can handle any frequency in its range
}

uint32_t RadioTimeOnAir(RadioModems_t modem, uint8_t pktLen) {
    return 0; // TODO: Implement actual time-on-air calculation
}

int16_t RadioRssi(RadioModems_t modem) {
    return SX126xGetRssiInst();
}

void RadioSetSyncWord(uint8_t data) {
    uint8_t syncWord[2] = {data, 0x00};
    SX126xWriteRegisters(REG_LR_SYNCWORD, syncWord, 2);
}

void RadioSetMaxPayloadLength(RadioModems_t modem, uint8_t max) {
    if(modem == MODEM_LORA) {
        PacketParams_t packetParams;
        packetParams.PacketType = PACKET_TYPE_LORA;
        packetParams.Params.LoRa.PayloadLength = max;
        SX126xSetPacketParams(&packetParams);
    }
}

void RadioSetPublicNetwork(bool enable) {
    RadioSetSyncWord(enable ? LORA_MAC_PUBLIC_SYNCWORD : LORA_MAC_PRIVATE_SYNCWORD);
}

uint32_t RadioGetWakeupTime(void) {
    return RADIO_WAKEUP_TIME;
}

void RadioIrqProcess(void) {
    SX126xOnDioIrq();
}

// SX126x specific functions
void RadioRxBoosted(uint32_t timeout) {
    SX126xSetRxBoosted(timeout);
}

void RadioSetRxDutyCycle(uint32_t rxTime, uint32_t sleepTime) {
    SX126xSetRxDutyCycle(rxTime, sleepTime);
}

// Interrupt handler
void RadioOnDioIrq(void) {
    SX126xOnDioIrq();
} 