#include "Common.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/EmuNwa/EmuNwaServer.h"

extern unique_ptr<Emulator> _emu;

extern "C" {
	DllExport void __stdcall StartEmuNwaServer() { _emu->GetEmuNwaServer()->StartServer(); }
	DllExport void __stdcall StopEmuNwaServer() { _emu->GetEmuNwaServer()->StopServer(); }
	DllExport bool __stdcall IsEmuNwaServerRunning() { return _emu->GetEmuNwaServer()->Started(); }
}
