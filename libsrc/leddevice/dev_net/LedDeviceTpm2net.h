#ifndef LEDEVICETPM2NET_H
#define LEDEVICETPM2NET_H

// hyperhdr includes
#include "ProviderUdp.h"

///
/// Implementation of the LedDevice interface for sending LED colors via udp tpm2.net packets
///
class LedDeviceTpm2net : public ProviderUdp
{
public:

	///
	/// @brief Constructs a TPM2 LED-device fed via UDP
	///
	/// @param deviceConfig Device's configuration as JSON-Object
	///
	explicit LedDeviceTpm2net(const QJsonObject &deviceConfig);

	///
	/// @brief Constructs the LED-device
	///
	/// @param[in] deviceConfig Device's configuration as JSON-Object
	/// @return LedDevice constructed
	///
	static LedDevice* construct(const QJsonObject &deviceConfig);

private:

	///
	/// @brief Initialise the device's configuration
	///
	/// @param[in] deviceConfig the JSON device configuration
	/// @return True, if success
	///
	bool init(const QJsonObject &deviceConfig) override;

	///
	/// @brief Writes the RGB-Color values to the LEDs.
	///
	/// @param[in] ledValues The RGB-color per LED
	/// @return Zero on success, else negative
	///
	int write(const std::vector<ColorRgb> & ledValues) override;

	int _tpm2_max;
	int _tpm2ByteCount;
	int _tpm2TotalPackets;
	int _tpm2ThisPacket;
};

#endif // LEDEVICETPM2NET_H
