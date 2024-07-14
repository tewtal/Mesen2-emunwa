#include "pch.h"
#include "EmuNwa/EmuNwaServer.h"
#include "EmuNwa/EmuNwaConnection.h"
#include "Shared/Emulator.h"
#include "Shared/MessageManager.h"
#include "Utilities/Socket.h"

EmuNwaServer::EmuNwaServer(Emulator* emu)
{
	_emu = emu;
	_stop = false;
	_initialized = false;
}

EmuNwaServer::~EmuNwaServer()
{
}

void EmuNwaServer::AcceptConnections()
{
	while(true) {
		unique_ptr<Socket> socket = _listener->Accept();
		if(!socket->ConnectionError()) {
			_openConnections.push_back(unique_ptr<EmuNwaConnection>(new EmuNwaConnection(this, _emu, std::move(socket))));
		} else {
			break;
		}
	}
	_listener->Listen(10);
}

void EmuNwaServer::UpdateConnections()
{
	vector<unique_ptr<EmuNwaConnection>> connectionsToRemove;
	for(int i = (int)_openConnections.size() - 1; i >= 0; i--) {
		if(_openConnections[i]->ConnectionError()) {
			_openConnections.erase(_openConnections.begin() + i);
		} else {
			_openConnections[i]->ProcessMessages();
		}
	}
}

void EmuNwaServer::Exec()
{
	_listener.reset(new Socket());
	_listener->Bind(0xBEEF);
	_listener->Listen(10);
	_stop = false;
	_initialized = true;
	MessageManager::DisplayMessage("EmuNwa", "ServerStarted", std::to_string(0xBEEF));

	while(!_stop) {
		AcceptConnections();
		UpdateConnections();

		std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(1));
	}
}

void EmuNwaServer::StartServer()
{
	_serverThread.reset(new thread(&EmuNwaServer::Exec, this));
}

void EmuNwaServer::StopServer()
{
	if(!_serverThread) {
		return;
	}

	_stop = true;

	if(_serverThread) {
		_serverThread->join();
		_serverThread.reset();
	}

	_openConnections.clear();
	_initialized = false;
	_listener.reset();
	MessageManager::DisplayMessage("EmuNwa", "ServerStopped");

}

bool EmuNwaServer::Started()
{
	return _initialized;
}

int EmuNwaServer::GetNextConnectionId()
{
	return _nextConnectionId++;
}
