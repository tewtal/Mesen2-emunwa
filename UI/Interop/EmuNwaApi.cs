using Mesen.Config;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Mesen.Interop
{
	public class EmuNwaApi
	{
		private const string DllPath = EmuApi.DllName;
		[DllImport(DllPath)] public static extern void StartEmuNwaServer();
		[DllImport(DllPath)] public static extern void StopEmuNwaServer();
		[DllImport(DllPath)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool IsEmuNwaServerRunning();
	}
}
