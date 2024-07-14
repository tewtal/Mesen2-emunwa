#pragma once
#include "pch.h"
#include "EmuNwa/EmuNwaServer.h"
#include "Shared/Emulator.h"
#include "Utilities/Socket.h"

enum BinaryMessageType
{
	INVALID = 0,
	CORE_WRITE = 1,
};

class EmuNwaConnection final
{
private:
	static constexpr size_t MaxMsgLength = 1500000;

	EmuNwaServer* _server = nullptr;
	Emulator* _emu = nullptr;
	unique_ptr<Socket> _socket;
	int _connectionId = 0;

	uint8_t _readBuffer[EmuNwaConnection::MaxMsgLength] = {};
	size_t _readPosition = 0;
	size_t _lastPosition = 0;
	SimpleLock _socketLock;

	std::string _clientName;

	BinaryMessageType _binaryMessageType = BinaryMessageType::INVALID;
	std::vector<std::string> _binaryMessageArguments;

	void ReadSocket();
	void Disconnect();

public:
	EmuNwaConnection(EmuNwaServer* server, Emulator *emu, unique_ptr<Socket> socket);
	virtual ~EmuNwaConnection();

	bool ConnectionError();
	void ProcessMessages();
	void HandleMessage(const char* message);
	void HandleBinaryMessage(const std::vector<uint8_t>& messageData);
	void HandleMyNameIs(const std::string& clientName);
	void HandleEmulatorInfo();
	void HandleEmulationStatus();
	void HandleCoreReset();
	void HandleEmulationStop();
	void HandleEmulationPause();
	void HandleEmulationResume();
	void HandleEmulationReload();
	void HandleGameInfo();
	void HandleCoresList();
	void HandleCoreMemories();
	void HandleCoreInfo(const std::string& coreName);
	void HandleCoreCurrentInfo();
	void HandleCoreRead(const std::string& memoryName, const std::vector<std::string>& arguments);
	void HandleCoreWrite(const std::vector<std::string>& arguments, const std::vector<uint8_t>& data);
	void SendError(const std::string& errorType, const std::string& message);
	void SendResponse(const std::string& response);
	void SendBinaryMessage(const std::vector<uint8_t>& data);
};