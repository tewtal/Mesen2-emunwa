#pragma once
#include "stdafx.h"
#include "Shared/Interfaces/IAudioProvider.h"
#include "Utilities/Audio/HermiteResampler.h"
#include "Utilities/ISerializable.h"
#include "PCE/PceConstants.h"
#include "PCE/PceTypes.h"

class Emulator;
class PceConsole;
class PceCdRom;
class PceScsiBus;

class PceAdpcm final : public IAudioProvider, public ISerializable
{
private:
	Emulator* _emu = nullptr;
	PceCdRom* _cdrom = nullptr;
	PceScsiBus* _scsi = nullptr;
	uint8_t* _ram = nullptr;
	PceAdpcmState _state = {};
	HermiteResampler _resampler;

	vector<int16_t> _samplesToPlay;
	int16_t _currentOutput = 0;
	uint8_t _magnitude = 0;
	
	double _clocksPerSample = PceConstants::MasterClockRate / 32000.0;
	double _nextSampleCounter = 0;

	constexpr static int _stepSize[392] =
	{
		0x0002, 0x0006, 0x000A, 0x000E, 0x0012, 0x0016, 0x001A, 0x001E,
		0x0002, 0x0006, 0x000A, 0x000E, 0x0013, 0x0017, 0x001B, 0x001F,
		0x0002, 0x0006, 0x000B, 0x000F, 0x0015, 0x0019, 0x001E, 0x0022,
		0x0002, 0x0007, 0x000C, 0x0011, 0x0017, 0x001C, 0x0021, 0x0026,
		0x0002, 0x0007, 0x000D, 0x0012, 0x0019, 0x001E, 0x0024, 0x0029,
		0x0003, 0x0009, 0x000F, 0x0015, 0x001C, 0x0022, 0x0028, 0x002E,
		0x0003, 0x000A, 0x0011, 0x0018, 0x001F, 0x0026, 0x002D, 0x0034,
		0x0003, 0x000A, 0x0012, 0x0019, 0x0022, 0x0029, 0x0031, 0x0038,
		0x0004, 0x000C, 0x0015, 0x001D, 0x0026, 0x002E, 0x0037, 0x003F,
		0x0004, 0x000D, 0x0016, 0x001F, 0x0029, 0x0032, 0x003B, 0x0044,
		0x0005, 0x000F, 0x0019, 0x0023, 0x002E, 0x0038, 0x0042, 0x004C,
		0x0005, 0x0010, 0x001B, 0x0026, 0x0032, 0x003D, 0x0048, 0x0053,
		0x0006, 0x0012, 0x001F, 0x002B, 0x0038, 0x0044, 0x0051, 0x005D,
		0x0006, 0x0013, 0x0021, 0x002E, 0x003D, 0x004A, 0x0058, 0x0065,
		0x0007, 0x0016, 0x0025, 0x0034, 0x0043, 0x0052, 0x0061, 0x0070,
		0x0008, 0x0018, 0x0029, 0x0039, 0x004A, 0x005A, 0x006B, 0x007B,
		0x0009, 0x001B, 0x002D, 0x003F, 0x0052, 0x0064, 0x0076, 0x0088,
		0x000A, 0x001E, 0x0032, 0x0046, 0x005A, 0x006E, 0x0082, 0x0096,
		0x000B, 0x0021, 0x0037, 0x004D, 0x0063, 0x0079, 0x008F, 0x00A5,
		0x000C, 0x0024, 0x003C, 0x0054, 0x006D, 0x0085, 0x009D, 0x00B5,
		0x000D, 0x0027, 0x0042, 0x005C, 0x0078, 0x0092, 0x00AD, 0x00C7,
		0x000E, 0x002B, 0x0049, 0x0066, 0x0084, 0x00A1, 0x00BF, 0x00DC,
		0x0010, 0x0030, 0x0051, 0x0071, 0x0092, 0x00B2, 0x00D3, 0x00F3,
		0x0011, 0x0034, 0x0058, 0x007B, 0x00A0, 0x00C3, 0x00E7, 0x010A,
		0x0013, 0x003A, 0x0061, 0x0088, 0x00B0, 0x00D7, 0x00FE, 0x0125,
		0x0015, 0x0040, 0x006B, 0x0096, 0x00C2, 0x00ED, 0x0118, 0x0143,
		0x0017, 0x0046, 0x0076, 0x00A5, 0x00D5, 0x0104, 0x0134, 0x0163,
		0x001A, 0x004E, 0x0082, 0x00B6, 0x00EB, 0x011F, 0x0153, 0x0187,
		0x001C, 0x0055, 0x008F, 0x00C8, 0x0102, 0x013B, 0x0175, 0x01AE,
		0x001F, 0x005E, 0x009D, 0x00DC, 0x011C, 0x015B, 0x019A, 0x01D9,
		0x0022, 0x0067, 0x00AD, 0x00F2, 0x0139, 0x017E, 0x01C4, 0x0209,
		0x0026, 0x0072, 0x00BF, 0x010B, 0x0159, 0x01A5, 0x01F2, 0x023E,
		0x002A, 0x007E, 0x00D2, 0x0126, 0x017B, 0x01CF, 0x0223, 0x0277,
		0x002E, 0x008A, 0x00E7, 0x0143, 0x01A1, 0x01FD, 0x025A, 0x02B6,
		0x0033, 0x0099, 0x00FF, 0x0165, 0x01CB, 0x0231, 0x0297, 0x02FD,
		0x0038, 0x00A8, 0x0118, 0x0188, 0x01F9, 0x0269, 0x02D9, 0x0349,
		0x003D, 0x00B8, 0x0134, 0x01AF, 0x022B, 0x02A6, 0x0322, 0x039D,
		0x0044, 0x00CC, 0x0154, 0x01DC, 0x0264, 0x02EC, 0x0374, 0x03FC,
		0x004A, 0x00DF, 0x0175, 0x020A, 0x02A0, 0x0335, 0x03CB, 0x0460,
		0x0052, 0x00F6, 0x019B, 0x023F, 0x02E4, 0x0388, 0x042D, 0x04D1,
		0x005A, 0x010F, 0x01C4, 0x0279, 0x032E, 0x03E3, 0x0498, 0x054D,
		0x0063, 0x012A, 0x01F1, 0x02B8, 0x037F, 0x0446, 0x050D, 0x05D4,
		0x006D, 0x0148, 0x0223, 0x02FE, 0x03D9, 0x04B4, 0x058F, 0x066A,
		0x0078, 0x0168, 0x0259, 0x0349, 0x043B, 0x052B, 0x061C, 0x070C,
		0x0084, 0x018D, 0x0296, 0x039F, 0x04A8, 0x05B1, 0x06BA, 0x07C3,
		0x0091, 0x01B4, 0x02D8, 0x03FB, 0x051F, 0x0642, 0x0766, 0x0889,
		0x00A0, 0x01E0, 0x0321, 0x0461, 0x05A2, 0x06E2, 0x0823, 0x0963,
		0x00B0, 0x0210, 0x0371, 0x04D1, 0x0633, 0x0793, 0x08F4, 0x0A54,
		0x00C2, 0x0246, 0x03CA, 0x054E, 0x06D2, 0x0856, 0x09DA, 0x0B5E
	};

	constexpr static int _stepFactor[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

	void Reset();
	void SetHalfReached(bool value);
	void SetEndReached(bool value);
	void SetControl(uint8_t value);
	void ProcessReadOperation();
	void ProcessWriteOperation();
	void ProcessDmaRequest();
	void PlaySample();

public:
	PceAdpcm(PceConsole* console, Emulator* emu, PceCdRom* cdrom, PceScsiBus* scsi);
	~PceAdpcm();

	void Exec();

	PceAdpcmState& GetState() { return _state; }

	void Write(uint16_t addr, uint8_t value);
	uint8_t Read(uint16_t addr);

	void MixAudio(int16_t* out, uint32_t sampleCount, uint32_t sampleRate) override;

	void Serialize(Serializer& s) override;
};