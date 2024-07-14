#include "pch.h"
#include "EmuNwa/EmuNwaConnection.h"
#include "EmuNwa/EmuNwaServer.h"
#include "Shared/MessageManager.h"
#include "Shared/Emulator.h"
#include <sstream>
#include <cstring>
#include <map>
#include <cstdint> // For fixed-size integer types (uint8_t, etc.)
#include <algorithm> // For std::copy_n

// Platform-specific headers for endianness conversion (replace with appropriate headers)
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

std::string toLower(const std::string& str)
{
	std::string result = str;
	std::transform(result.begin(), result.end(), result.begin(),
						[](unsigned char c) { return std::tolower(c); });
	return result;
}

// Utility function to check if a string starts with a prefix
bool startsWith(const std::string& str, const std::string& prefix)
{
	return str.compare(0, prefix.size(), prefix) == 0;
}

// Utility function to split a string into a vector based on a delimiter
std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while(std::getline(tokenStream, token, delimiter)) {
		tokens.push_back(token);
	}
	return tokens;
}

EmuNwaConnection::EmuNwaConnection(EmuNwaServer* server, Emulator *emu, unique_ptr<Socket> socket)
{
	_server = server;
	_emu = emu;
	_socket = std::move(socket);
	_connectionId = server->GetNextConnectionId();
	MessageManager::DisplayMessage("EmuNwa", "Client connected");
}

EmuNwaConnection::~EmuNwaConnection()
{
	MessageManager::DisplayMessage("EmuNwa", "Client disconnected");
	Disconnect();
}

void EmuNwaConnection::ReadSocket()
{
	auto lock = _socketLock.AcquireSafe();
	int bytesReceived = _socket->Recv((char*)_readBuffer + _readPosition, (int)(EmuNwaConnection::MaxMsgLength - _readPosition), 0);
	if(bytesReceived > 0) {
		_readPosition += (size_t)bytesReceived;
	}
}

bool EmuNwaConnection::ConnectionError()
{
	return _socket->ConnectionError();
}

void EmuNwaConnection::Disconnect()
{
	auto lock = _socketLock.AcquireSafe();
	_socket->Close();
}

void EmuNwaConnection::ProcessMessages()
{
	ReadSocket();

	while(_readPosition > 0) {
		if(_readBuffer[0] == '\0') {
			// Binary Message Handling:
			if(_readPosition < 5) {
				// Not enough data for size prefix, wait for more
				break;
			}

			// Get message size from the next 4 bytes
			uint32_t messageSize = ntohl(*reinterpret_cast<uint32_t*>(_readBuffer + 1));

			if(_readPosition < messageSize + 5) {
				// Not enough data for the entire message, wait for more
				break;
			}

			// Extract the binary data
			std::vector<uint8_t> messageData(_readBuffer + 5, _readBuffer + 5 + messageSize);

			// Process the binary message
			HandleBinaryMessage(messageData);

			// Adjust read buffer position
			_readPosition -= messageSize + 5;
			memmove(_readBuffer, _readBuffer + messageSize + 5, _readPosition);

		} else {
			// ASCII Message Handling:
			char* newLinePtr = (char*)memchr(_readBuffer, '\n', _readPosition);
			if(newLinePtr != nullptr) {
				// Copy the message to a new buffer
				int messageLength = newLinePtr - (char*)_readBuffer;
				char* messageBuffer = new char[messageLength + 1];
				memcpy(messageBuffer, _readBuffer, messageLength);
				messageBuffer[messageLength] = '\0';

				// Process the message
				HandleMessage(messageBuffer);

				// Free the allocated message buffer
				delete[] messageBuffer;

				// Move the rest of the buffer to the beginning of _readBuffer
				size_t remainingLength = _readPosition - messageLength - 1;
				memmove(_readBuffer, newLinePtr + 1, remainingLength);
				_readPosition = remainingLength;

			} else {
				// No complete ASCII message yet, wait for more data
				break;
			}
		}
	}
}

void EmuNwaConnection::HandleMessage(const char* message)
{
	// Parse the incoming message
	std::string messageStr(message);
	std::istringstream iss(messageStr);
	std::string command;
	std::getline(iss, command, ' ');

	// Remove trailing newline characters
	command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());

	// Convert the command to lowercase for case-insensitive comparison
	command = toLower(command);

	std::string argumentsStr;
	std::getline(iss, argumentsStr); // Get the rest of the line

	// Split the arguments
	std::vector<std::string> arguments = split(argumentsStr, ';');

	// Handle commands
	if(command == "my_name_is") {
		HandleMyNameIs(arguments[0]);
	} else if(command == "emulator_info") {
		HandleEmulatorInfo();
	} else if(command == "emulation_status") {
		HandleEmulationStatus();
	} else if(command == "game_info") {
		HandleGameInfo();
	} else if(command == "cores_list") {
		HandleCoresList();
	} else if(command == "core_memories") {
		HandleCoreMemories();
	} else if(command == "core_info") {
		HandleCoreInfo(arguments[0]);
	} else if(command == "core_current_info") {
		HandleCoreCurrentInfo();
	} else if(command == "core_reset" || command == "emulation_reset") {
		HandleCoreReset();
	} else if(command == "emulation_stop") {
		HandleEmulationStop();
	} else if(command == "emulation_pause") {
		HandleEmulationPause();
	} else if(command == "emulation_resume") {
		HandleEmulationResume();
	} else if(command == "emulation_reload") {
		HandleEmulationReload();
	} else if(command == "core_read") {
		std::string memoryName = arguments[0];
		arguments.erase(arguments.begin()); // Remove memory name from arguments
		HandleCoreRead(memoryName, arguments);
	} else if(command == "bcore_write" && arguments.size() > 0) {
		_binaryMessageType = BinaryMessageType::CORE_WRITE;
		_binaryMessageArguments = arguments;
	} else {
		SendError("invalid_command", "Unknown command");
	}
}

void EmuNwaConnection::HandleBinaryMessage(const std::vector<uint8_t>& messageData)
{
	if(_binaryMessageType == BinaryMessageType::CORE_WRITE) {
		HandleCoreWrite(_binaryMessageArguments, messageData);
	} else {
		  SendError("invalid_command", "Unknown binary command");
	}

	SendResponse("\n\n");
}

void EmuNwaConnection::HandleMyNameIs(const std::string& clientName)
{
	_clientName = clientName;
	MessageManager::DisplayMessage("EmuNwa", "Client set the name to: " + _clientName);

	// Send success response
	SendResponse("\nname:" + _clientName + "\n\n");
}

void EmuNwaConnection::HandleEmulatorInfo()
{
	std::string name = "Mesen";
	std::string version = "2.0";
	std::string nwaVersion = "1.0";
	std::string id = std::to_string(_connectionId);
	std::string commands = "EMULATOR_INFO,EMULATION_STATUS,CORES_LIST,CORE_MEMORIES,CORE_INFO,CORE_CURRENT_INFO,MY_NAME_IS,CORE_READ,bCORE_WRITE,CORE_RESET,EMULATION_PAUSE,EMULATION_STOP,EMULATION_RESET,EMULATION_RESUME,EMULATION_RELOAD";

	// Send the response
	SendResponse(
		 "\n"
		 "name:" + name + "\n"
		 "version:" + version + "\n"
		 "nwa_version:" + nwaVersion + "\n"
		 "id:" + id + "\n"
		 "commands:" + commands + "\n"
		 "\n"
	);
}

void EmuNwaConnection::HandleEmulationStatus()
{
	std::string state = _emu->IsRunning() ? (_emu->IsPaused() ? "paused" : "running") : "no_game";
	std::string gameId = _emu->GetRomInfo().RomFile.GetFileName();

	// Send the response
	SendResponse(
		 "\n"
		 "state:" + state + "\n"
		 "game:" + gameId + "\n"
		 "\n"
	);
}

void EmuNwaConnection::HandleCoreReset()
{
	_emu->Reset();
	SendResponse("\n\n");
}

void EmuNwaConnection::HandleEmulationStop()
{
	_emu->Stop(true, false, true);
	SendResponse("\n\n");
}

void EmuNwaConnection::HandleEmulationPause()
{
	_emu->Pause();
	SendResponse("\n\n");
}

void EmuNwaConnection::HandleEmulationResume()
{
	_emu->Resume();
	SendResponse("\n\n");
}

void EmuNwaConnection::HandleEmulationReload()
{
	_emu->PowerCycle();
	SendResponse("\n\n");
}

void EmuNwaConnection::HandleGameInfo()
{
	std::string file = _emu->GetRomInfo().RomFile.GetFileName();
	std::string name = _emu->GetRomInfo().RomFile.GetFileName();
	std::string region = "Unknown";
	std::string type = "Unknown";

	// Send the response
	SendResponse(
		 "\n"
		 "name:" + name + "\n"
		 "file:" + file + "\n"
		 "region:" + region + "\n"
		 "type:" + type + "\n"
		 "\n"
	);
}

void EmuNwaConnection::HandleCoresList()
{

	// List of supported cores and their platforms
	// Only support SNES for now
	std::vector<std::pair<std::string, std::string>> cores = {
		//{"NesCore", "NES"},
		{"SnesCore", "SNES"},
		//{"GbaCore", "GBA"},
		//{"GbcCore", "GBC"},
		//{"PceCore", "PCE"},
		//{"SmsCore", "SMS"},
	};

	// Send the response
	std::string response = "\n";
	for(auto& core : cores) {
		response += "name:" + core.first + "\n";
		response += "platform:" + core.second + "\n";
	}
	response += "\n";

	SendResponse(response);

}

void EmuNwaConnection::HandleCoreMemories()
{
	std::map<MemoryType, std::string> memoryTypes = {
		{MemoryType::SnesPrgRom, "CARTROM"},
		{MemoryType::SnesSaveRam, "SRAM"},
		{MemoryType::SnesWorkRam, "WRAM"},
		{MemoryType::SnesVideoRam, "VRAM"},
		{MemoryType::SnesSpriteRam, "OAM"},
		{MemoryType::SnesCgRam, "CGRAM"},
		{MemoryType::SnesMemory, "CPUBUS"},
		{MemoryType::SpcMemory, "APUBUS"},
	};

	std::string response = "\n";
	for(auto& memoryType : memoryTypes) {
		int memory_size = _emu->GetMemory(memoryType.first).Size;
		
		if(memoryType.second == "CPUBUS") {
			memory_size = 0x1000000;
		} else if(memoryType.second == "APUBUS") {
			memory_size = 0x10000;
		}

		std::string memory_size_str = std::to_string(memory_size);

		response += "name:" + memoryType.second + "\n";
		response += "access:rw\n";
		response += "size:" + memory_size_str + "\n";
	}

	response += "\n";

	SendResponse(response);
}

void EmuNwaConnection::HandleCoreInfo(const std::string& coreName)
{
	// Only support SNES for now
	if(coreName != "SnesCore") {
		SendError("invalid_core", "Invalid core name");
		return;
	}

	// Send the response
	SendResponse(
		 "\n"
		 "platform:SNES\n"
		 "name:SnesCore\n"
		 "version:2.0\n"
		 "file:Mesen.exe\n"
		 "\n"
	);
}

void EmuNwaConnection::HandleCoreCurrentInfo()
{
	if(_emu->GetConsoleType() != ConsoleType::Snes) {
		SendError("invalid_core", "Unsupported core loaded");
		return;
	}
	else 
	{
		HandleCoreInfo("SnesCore");
	}
}

void EmuNwaConnection::HandleCoreRead(const std::string& memoryName, const std::vector<std::string>& arguments)
{
	// 1. Validate Arguments:
	if(arguments.empty() || (arguments.size() % 2 != 0)) { // At least one pair, and even number
		SendError("invalid_argument", "CORE_READ requires at least memory name and offset, and pairs of offset/size.");
		return;
	}

	std::map<std::string, MemoryType> memoryTypes = {
		{"CARTROM", MemoryType::SnesPrgRom},
		{"SRAM", MemoryType::SnesSaveRam},
		{"WRAM", MemoryType::SnesWorkRam},
		{"VRAM", MemoryType::SnesVideoRam},
		{"OAM", MemoryType::SnesSpriteRam},
		{"CGRAM", MemoryType::SnesCgRam},
		{"CPUBUS", MemoryType::SnesMemory},
		{"APUBUS", MemoryType::SpcMemory},
	};

	ConsoleMemoryInfo memory = _emu->GetMemory(memoryTypes[memoryName]);


	std::vector<uint8_t> readData;
	size_t memory_size = memory.Size;
	
	if(memoryName == "CPUBUS")
	{
		memory_size = 0x1000000;
	}
	else if(memoryName == "APUBUS") 
	{
		memory_size = 0x10000;
	}

	// 2. Process Each Offset/Size Pair:
	for(size_t i = 0; i < arguments.size(); i += 2) {
		const std::string& offsetStr = arguments[i];
		const std::string& sizeStr = arguments[i + 1];

		size_t offset = 0;
		try 
		{
			if(offsetStr[0] == '$') 
			{
				offset = std::stoul(offsetStr.substr(1), nullptr, 16); // Hexadecimal
			} else {
				offset = std::stoul(offsetStr, nullptr, 10); // Decimal
			}
      }
		catch(const std::exception&) 
		{
			SendError("invalid_argument", "Invalid offset: " + offsetStr);
			return;
		}

		// Parse size:
		size_t size = 0;
		if(sizeStr.empty()) {
			size = memory_size - offset; // Read to end of memory
		} else {
			try {
				if(sizeStr[0] == '$') {
					size = std::stoul(sizeStr.substr(1), nullptr, 16); // Hexadecimal
				} else {
					size = std::stoul(sizeStr, nullptr, 10); // Decimal
				}
			}
			catch(const std::exception&) 
			{
				SendError("invalid_argument", "Invalid size: " + sizeStr);
				return;
			}
		}

		// 3. Bounds Checking:
		if(offset >= memory_size || (offset + size) > memory_size) {
			SendError("not_allowed", "Memory read out of bounds.");
			return;
		}


		// 4. Perform Memory Read:
		readData.resize(readData.size() + size); // Ensure enough space
		std::copy_n((uint8_t*)memory.Memory + offset, size, readData.data() + readData.size() - size);
	}

	// 5. Send Binary Reply:
	SendBinaryMessage(readData);
}

void EmuNwaConnection::HandleCoreWrite(const std::vector<std::string>& arguments,
												  const std::vector<uint8_t>& data)
{
	if(arguments.empty() || ((arguments.size() - 1) % 2 != 0)) {
		SendError("invalid_argument",
					 "bCORE_WRITE requires at least memory name and offset, and pairs of offset/size.");
		return;
	}

	std::string memoryName = arguments[0];

	std::map<std::string, MemoryType> memoryTypes = {
		 {"CARTROM", MemoryType::SnesPrgRom},
		 {"SRAM", MemoryType::SnesSaveRam},
		 {"WRAM", MemoryType::SnesWorkRam},
		 {"VRAM", MemoryType::SnesVideoRam},
		 {"OAM", MemoryType::SnesSpriteRam},
		 {"CGRAM", MemoryType::SnesCgRam},
		 {"CPUBUS", MemoryType::SnesMemory},
		 {"APUBUS", MemoryType::SpcMemory},
	};

	if(memoryTypes.find(memoryName) == memoryTypes.end()) {
		SendError("invalid_argument", "Invalid memory name: " + memoryName);
		return;
	}

	ConsoleMemoryInfo memory = _emu->GetMemory(memoryTypes[memoryName]);

	size_t memory_size = memory.Size;

	if(memoryName == "CPUBUS") {
		memory_size = 0x1000000;
	} else if(memoryName == "APUBUS") {
		memory_size = 0x10000;
	}

	size_t dataOffset = 0;

	auto lock = _emu->AcquireLock();	

	for(size_t i = 1; i < arguments.size(); i += 2) {
		const std::string& offsetStr = arguments[i];
		const std::string& sizeStr = arguments[i + 1];

		size_t offset = 0;
		try 
		{
			if(offsetStr[0] == '$') {
				offset = std::stoul(offsetStr.substr(1), nullptr, 16); // Hexadecimal
			} else {
				offset = std::stoul(offsetStr, nullptr, 10); // Decimal
			}
		} 
		catch(const std::exception&) 
		{
			SendError("invalid_argument", "Invalid offset: " + offsetStr);
			return;
		}

		size_t size = 0;
		if(sizeStr.empty()) {
			size = memory_size - offset; // Read to end of memory
		} else {
			try 
			{
				if(sizeStr[0] == '$') {
					size = std::stoul(sizeStr.substr(1), nullptr, 16); // Hexadecimal
				} else {
					size = std::stoul(sizeStr, nullptr, 10); // Decimal
				}
			}
			catch(const std::exception&)
			{
				SendError("invalid_argument", "Invalid size: " + sizeStr);
				return;
			}
		}

		// Bounds Checking
		if(offset >= memory_size || (offset + size) > memory_size) {
			SendError("not_allowed", "Memory write out of bounds.");
			return;
		}

		// Check if enough data has been received
		if((dataOffset + size) > data.size()) {
			SendError("protocol_error", "Insufficient data received for bCORE_WRITE.");
			return;
		}

		// Copy the data to the memory
		std::copy_n(data.begin() + dataOffset, size, (uint8_t*)memory.Memory + offset);

		dataOffset += size;
	}

}


// Example helper function to send an error response
void EmuNwaConnection::SendError(const std::string& errorType, const std::string& message)
{
	SendResponse(
		 "\n"
		 "error:" + errorType + "\n"
		 "reason:" + message + "\n"
		 "\n"
	);
}

void EmuNwaConnection::SendResponse(const std::string& response)
{
	auto lock = _socketLock.AcquireSafe();
	int bytesSent = _socket->Send(const_cast<char*>(response.c_str()), (int)response.size(), 0);
	if(bytesSent < 0) {
		Disconnect();
		return;
	}
}

void EmuNwaConnection::SendBinaryMessage(const std::vector<uint8_t>& data)
{
	// 1. Prepare the complete message buffer
	std::vector<uint8_t> message(data.size() + 5); // 1 byte indicator + 4 bytes size + data

	// 2. Set the binary reply indicator (0x00)
	message[0] = '\0';

	// 3. Set the size prefix (in network byte order)
	uint32_t size = htonl((u_long)data.size());
	std::memcpy(&message[1], &size, sizeof(size));

	// 4. Copy the actual data
	std::copy(data.begin(), data.end(), message.begin() + 5);

	// 5. Send the entire message in one go
	{
		auto lock = _socketLock.AcquireSafe();
		int bytesSent = _socket->Send(reinterpret_cast<char*>(message.data()), (int)message.size(), 0);
		if(bytesSent < 0) {
			Disconnect();
			return;
		}
	}
}
