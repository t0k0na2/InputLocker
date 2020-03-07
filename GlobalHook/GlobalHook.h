#pragma once
#ifdef GLOBALHOOK_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif

enum class GlobalHookTypes
{
	Keyboard = 0,
	LowLevelKeyboard = 1,
	Mouse = 2,
	LowLevelMouse = 3,
};

static constexpr int NumGlobalHookTypes = 4;

EXPORT_API int RegisterGlobalHook(HWND hWnd, HOOKPROC proc, GlobalHookTypes type);
EXPORT_API int UnregisterGlobalHook(HWND hWnd, GlobalHookTypes type);