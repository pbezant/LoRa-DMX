#pragma once

#include "driver/sx126x.h"
#include "loramac/LoRaMac.h"
#include "radio/radio.h"

// Radio driver function declarations
#ifdef __cplusplus
extern "C" {
#endif

// Radio constants and enums
#define PACKET_TYPE_GFSK                         0x00
#define PACKET_TYPE_LORA                         0x01
#define PACKET_TYPE_NONE                         0x0F

#define RADIO_RAMP_10_US                         0x00
#define RADIO_RAMP_20_US                         0x01
#define RADIO_RAMP_40_US                         0x02
#define RADIO_RAMP_80_US                         0x03
#define RADIO_RAMP_200_US                        0x04
#define RADIO_RAMP_800_US                        0x05
#define RADIO_RAMP_1700_US                       0x06
#define RADIO_RAMP_3400_US                       0x07

#define LORA_CAD_01_SYMBOL                       0x00
#define LORA_CAD_02_SYMBOL                       0x01
#define LORA_CAD_04_SYMBOL                       0x02
#define LORA_CAD_08_SYMBOL                       0x03
#define LORA_CAD_16_SYMBOL                       0x04

#define LORA_IQ_NORMAL                           0x00
#define LORA_IQ_INVERTED                         0x01

#define LORA_CRC_ON                              0x01
#define LORA_CRC_OFF                             0x00

#define LORA_PACKET_VARIABLE_LENGTH              0x00
#define LORA_PACKET_FIXED_LENGTH                 0x01

#define LORA_MAC_PRIVATE_SYNCWORD                0x12
#define LORA_MAC_PUBLIC_SYNCWORD                 0x34

// Basic radio operations
int RadioInit(RadioEvents_t *events);
RadioState_t RadioGetStatus(void);
void RadioSetModem(RadioModems_t modem);
void RadioSetChannel(uint32_t freq);
bool RadioIsChannelFree(RadioModems_t modem, uint32_t freq, int16_t rssiThresh, uint32_t maxCarrierSenseTime);
uint32_t RadioRandom(void);

// Configuration
void RadioSetRxConfig(RadioModems_t modem, uint32_t bandwidth, uint32_t datarate,
                     uint8_t coderate, uint32_t bandwidthAfc, uint16_t preambleLen,
                     uint16_t symbTimeout, bool fixLen, uint8_t payloadLen,
                     bool crcOn, bool freqHopOn, uint8_t hopPeriod,
                     bool iqInverted, bool rxContinuous);

void RadioSetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
                     uint32_t bandwidth, uint32_t datarate,
                     uint8_t coderate, uint16_t preambleLen,
                     bool fixLen, bool crcOn, bool freqHopOn,
                     uint8_t hopPeriod, bool iqInverted, uint32_t timeout);

// Operation modes
void RadioSleep(void);
void RadioStandby(void);
void RadioRx(uint32_t timeout);
void RadioStartCad(uint8_t symbols);
void RadioSetTxContinuousWave(uint32_t freq, int8_t power, uint16_t time);

// Data operations
void RadioSend(uint8_t *buffer, uint8_t size);
void RadioWrite(uint16_t addr, uint8_t data);
uint8_t RadioRead(uint16_t addr);
void RadioWriteBuffer(uint16_t addr, uint8_t *buffer, uint8_t size);
void RadioReadBuffer(uint16_t addr, uint8_t *buffer, uint8_t size);

// Advanced features
bool RadioCheckRfFrequency(uint32_t frequency);
uint32_t RadioTimeOnAir(RadioModems_t modem, uint8_t pktLen);
int16_t RadioRssi(RadioModems_t modem);
void RadioSetSyncWord(uint8_t data);
void RadioSetMaxPayloadLength(RadioModems_t modem, uint8_t max);
void RadioSetPublicNetwork(bool enable);
uint32_t RadioGetWakeupTime(void);
void RadioIrqProcess(void);

// SX126x specific functions
void RadioRxBoosted(uint32_t timeout);
void RadioSetRxDutyCycle(uint32_t rxTime, uint32_t sleepTime);

#ifdef __cplusplus
}
#endif 