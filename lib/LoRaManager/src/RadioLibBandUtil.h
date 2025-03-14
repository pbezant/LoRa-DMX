#ifndef RADIOLIB_BAND_UTIL_H
#define RADIOLIB_BAND_UTIL_H

#include <RadioLib.h>

// Define band type constants
#define BAND_TYPE_US915 1
#define BAND_TYPE_EU868 2
#define BAND_TYPE_OTHER 0

/**
 * @brief Get the band type from a LoRaWANBand_t by checking its name
 * 
 * @param band The LoRaWANBand_t object
 * @return uint8_t Band type constant (BAND_TYPE_US915, BAND_TYPE_EU868, or BAND_TYPE_OTHER)
 */
inline uint8_t getBandTypeFromBand(const LoRaWANBand_t& band) {
  if (band.name) {
    String bandName = String(band.name);
    if (bandName.indexOf("US915") >= 0) {
      return BAND_TYPE_US915;
    } else if (bandName.indexOf("EU868") >= 0) {
      return BAND_TYPE_EU868;
    }
  }
  return BAND_TYPE_OTHER;
}

/**
 * @brief Check if a band is the US915 band
 * 
 * @param band The LoRaWANBand_t object
 * @return true if the band is US915
 * @return false if the band is not US915
 */
inline bool isUS915Band(const LoRaWANBand_t& band) {
  return getBandTypeFromBand(band) == BAND_TYPE_US915;
}

/**
 * @brief Check if a band is the EU868 band
 * 
 * @param band The LoRaWANBand_t object
 * @return true if the band is EU868
 * @return false if the band is not EU868
 */
inline bool isEU868Band(const LoRaWANBand_t& band) {
  return getBandTypeFromBand(band) == BAND_TYPE_EU868;
}

#endif // RADIOLIB_BAND_UTIL_H
