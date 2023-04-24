#include "hook.h"
#include "hook_absolution.h"
#include "hook_sniper.h"
#include "proxy.h"

bool OptionsEnabled = false;
bool OptionsLog = false;

IHook* Hook;
std::ofstream LogFile;
INIReader IniFile;

void InitializeOptions()
{
	LogFile.open("hook.log", std::ios_base::out);

	LogTime();
	LogFile << "InitializeOptions" << std::endl;

	IniFile = INIReader("hook.ini");

	if (IniFile.ParseError() != 0) {
		LogFile << "Can't load hook.ini!" << std::endl;

		return;
	}

	OptionsEnabled = IniFile.GetBoolean("options", "enabled", false);

	if (!OptionsEnabled)
	{
		return;
	}

	OptionsLog = IniFile.GetBoolean("options", "log", false);

	auto IsHitmanAbsolution = strcmp(
		IniFile.Get("options", "game", "hm5").c_str(),
		"hm5"
	) == 0;

	if (IsHitmanAbsolution)
	{
		LogFile << "Hitman Absolution" << std::endl;

		Hook = (IHook*)new AbsolutionHook();
	}
	else
	{
		LogFile << "Hitman Sniper Challenge" << std::endl;

		Hook = (IHook*)new SniperHook();
	}

	Hook->InitializeOptions();
}

void InitializeHook()
{
	LogFile << "InitializeHook" << std::endl;

	LogStatus("Initialize", MH_Initialize());

	Hook->PreInitializeHook();
}

void DeinitializeHook()
{
	LogTime();
	LogFile << "DeinitializeHook" << std::endl;

	LogStatus("Deinitialize", MH_Uninitialize());

	if (LogFile)
	{
		LogFile.close();
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID /*lpReserved*/)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);

		InitializeOptions();

		LoadProxyDll();

		if (OptionsEnabled)
		{
			InitializeHook();
		}
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		if (OptionsEnabled)
		{
			DeinitializeHook();
		}
	}

	return TRUE;
}