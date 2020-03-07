// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "framework.h"
#include "GlobalHook.h"

#include <map>
#include <array>

#pragma comment(linker, "/SECTION:.shared,RWS")
#pragma data_seg(".shared")
int refCnt_ = 0;
std::array<std::map<HWND, HHOOK>*, NumGlobalHookTypes> hookerMaps_;
#pragma data_seg()

#define MUTEX_NAME TEXT("GlobalHookMutex")

HANDLE hMutex_;
HINSTANCE hInstance_;

EXPORT_API int RegisterGlobalHook(HWND hWnd, HOOKPROC proc, GlobalHookTypes type)
{
	int result = -1;
	auto m = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
	WaitForSingleObject(m, INFINITE);
	
	auto hookerMap = hookerMaps_[static_cast<int>(type)];

	if (hookerMap->find(hWnd) == hookerMap->end())
	{
		int idHook = 0;
		if(type == GlobalHookTypes::Keyboard)
			idHook = WH_KEYBOARD;
		else if (type == GlobalHookTypes::LowLevelKeyboard)
			idHook = WH_KEYBOARD_LL;
		else if (type == GlobalHookTypes::Mouse)
			idHook = WH_MOUSE;
		else if (type == GlobalHookTypes::LowLevelMouse)
			idHook = WH_MOUSE_LL;


		auto hHook = SetWindowsHookEx(idHook, proc, hInstance_, 0);
		if (hHook != NULL)
		{
			hookerMap->insert(std::make_pair(hWnd, hHook));
			result = 0;
		}
		else
		{
			OutputDebugStringA("gyaaaa!!");
		}
	}
	ReleaseMutex(m);
	return result;
}

EXPORT_API int UnregisterGlobalHook(HWND hWnd, GlobalHookTypes type)
{
	int result = -1;
	auto m = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
	WaitForSingleObject(m, INFINITE);
	auto hookerMap = hookerMaps_[static_cast<int>(type)];
	if (hookerMap->find(hWnd) != hookerMap->end())
	{
		UnhookWindowsHookEx(hookerMap->at(hWnd));
		hookerMap->erase(hWnd);

	}
	ReleaseMutex(m);
	return result;
}

// エントリポイント
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		hMutex_ = CreateMutex(NULL, FALSE, MUTEX_NAME);
		
		{
			auto m = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
			WaitForSingleObject(m, INFINITE);
			++refCnt_;
			if (refCnt_ == 1)
			{
				for(auto& e : hookerMaps_)
					e = new std::map<HWND, HHOOK>();
			}
			ReleaseMutex(m);
		}
		
		hInstance_ = hModule;
		break;
	case DLL_PROCESS_DETACH:
		{
			auto m = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
			WaitForSingleObject(m, INFINITE);
			--refCnt_;
			if (refCnt_ == 0)
			{
				for (auto& e : hookerMaps_)
					delete e;
			}
			ReleaseMutex(m);
		}
		
		CloseHandle(hMutex_);
		break;
	}
	return TRUE;
}
