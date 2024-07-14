#pragma once
#include "pch.h"
#include <thread>
#include "EmuNwa/EmuNwaConnection.h"
#include "Shared/Emulator.h"
#include "Utilities/Socket.h"

class EmuNwaConnection;

class EmuNwaServer : public std::enable_shared_from_this<EmuNwaServer>
{
private:
	Emulator* _emu;
	unique_ptr<thread> _serverThread;
	unique_ptr<Socket> _listener;
	atomic<bool> _stop;
	vector<unique_ptr<EmuNwaConnection>> _openConnections;
	int _nextConnectionId = 1;
	bool _initialized = false;
	void Exec();
	void AcceptConnections();
	void UpdateConnections();

public:
	EmuNwaServer(Emulator* emu);
	virtual ~EmuNwaServer();

	void StartServer();
	void StopServer();
	bool Started();
	int GetNextConnectionId();
};

