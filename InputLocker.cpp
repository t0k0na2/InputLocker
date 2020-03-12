// InputLocker.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "InputLocker.h"

#include "GlobalHook/GlobalHook.h"

#include <stdio.h>
#include <shellapi.h>
#include <array>

#include <stdarg.h>

#define MAX_LOADSTRING 100


// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// 追加分

namespace {
    constexpr UINT WM_NOTIFYICON = WM_APP + 0;
    constexpr DWORD BEEP_LENGTH = 300;
}

namespace {
    HWND hwnd_;
    bool inputLocked_ = false;
    NOTIFYICONDATA notifyIconData_;
    HICON hLockIcon_;
    HICON hUnlockIcon_;
    bool enableLockTimer_ = false;
    UINT lockTimerInteval_ = 10 * 1000;
    UINT_PTR lockTimerID_ = 1241321;
    BYTE lockKey_ = VK_END;
    bool lockKeyWithAlt = true;
    bool lockKeyWithControl = true;
    bool prevLockState_ = false;
    bool altDown_ = false;
    bool ctrlDown_ = false;
    bool lockKeyDown_ = false;
}

namespace {
    void Log(LPCTSTR format, ...)
    {
        TCHAR buf[128] = {};
        va_list ap;
        va_start(ap, format);
        _vstprintf_s(buf, _countof(buf), format, ap);
        va_end(ap);
        OutputDebugString(buf);
        OutputDebugString(TEXT("\n"));
    }

    void ResetLockTimer()
    {
        if (enableLockTimer_)
            lockTimerID_ = SetTimer(hwnd_, lockTimerID_, lockTimerInteval_, nullptr);
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: ここにコードを挿入してください。

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_INPUTLOCKER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // アプリケーション初期化の実行:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_INPUTLOCKER));

    MSG msg;

    // メイン メッセージ ループ:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_INPUTLOCKER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_INPUTLOCKER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   hwnd_ = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hwnd_)
   {
      return FALSE;
   }

   ShowWindow(hwnd_, SW_HIDE);
   UpdateWindow(hwnd_);

   return TRUE;
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND  - アプリケーション メニューの処理
//  WM_PAINT    - メイン ウィンドウを描画する
//  WM_DESTROY  - 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CREATE:
		{
			auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
			hLockIcon_ = LoadIcon(cs->hInstance, MAKEINTRESOURCE(IDI_LOCK));
			hUnlockIcon_ = LoadIcon(cs->hInstance, MAKEINTRESOURCE(IDI_UNLOCK));
			memset(&notifyIconData_, 0, sizeof(notifyIconData_));
			notifyIconData_.cbSize = sizeof(NOTIFYICONDATA);
			notifyIconData_.hWnd = hWnd;
			notifyIconData_.uID = 1;
			notifyIconData_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
			notifyIconData_.hIcon = hUnlockIcon_;
			notifyIconData_.uCallbackMessage = WM_NOTIFYICON;
			_stprintf_s(notifyIconData_.szInfoTitle, TEXT("InputLocker"));
			LoadString(cs->hInstance, IDS_APP_TITLE, notifyIconData_.szTip, _countof(notifyIconData_.szTip));
			if (Shell_NotifyIcon(NIM_ADD, &notifyIconData_) == TRUE)
			{

				RegisterGlobalHook(hWnd, [](int nCode, WPARAM wParam, LPARAM lParam)->LRESULT
					{
						if (nCode < 0)
						{
							return CallNextHookEx(NULL, nCode, wParam, lParam);
						}

                        auto param = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                        ResetLockTimer();

                        auto info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                        //Log(TEXT("%d %d"), wParam, info->vkCode);
                        if (wParam == WM_SYSKEYDOWN)
                        {
                            altDown_ = true;
                            if(info->vkCode == lockKey_)
                                lockKeyDown_ = true;
                            else if(info->vkCode == VK_LCONTROL || info->vkCode == VK_RCONTROL)
                                ctrlDown_ = true;
                        }
                        else if (wParam == WM_KEYDOWN)
                        {
                            if (info->vkCode == lockKey_)
                                lockKeyDown_ = true;
                            else if (info->vkCode == VK_LCONTROL || info->vkCode == VK_RCONTROL)
                                ctrlDown_ = true;
                            else if (info->vkCode == VK_LMENU || info->vkCode == VK_RMENU)
                                altDown_ = true;
                        }
                        else if (wParam == WM_SYSKEYUP)
                        {
                            if (info->vkCode == lockKey_)
                                lockKeyDown_ = false;
                            else if (info->vkCode == VK_LCONTROL || info->vkCode == VK_RCONTROL)
                                ctrlDown_ = false;
                        }
                        else if (wParam == WM_KEYUP)
                        {
                            if (info->vkCode == lockKey_)
                                lockKeyDown_ = false;
                            else if (info->vkCode == VK_LCONTROL || info->vkCode == VK_RCONTROL)
                                ctrlDown_ = false;
                            else if (info->vkCode == VK_LMENU || info->vkCode == VK_RMENU)
                                altDown_ = false;
                        }

                        //Log(TEXT("alt:%s ctrl:%s key:%s"), altDown_ ? TEXT("o") : TEXT("x"), ctrlDown_ ? TEXT("o") : TEXT("x"), lockKeyDown_ ? TEXT("o") : TEXT("x"));

                        bool nowLockState = (lockKeyWithAlt == false || altDown_) && (lockKeyWithControl == false || ctrlDown_) && lockKeyDown_;
                        
                        //if (nowLockState == true && prevLockState_ == false)
                        //    Log(TEXT("lock change!!!"));

                        if(nowLockState == true && prevLockState_ == false)
						{
							inputLocked_ = !inputLocked_;
							notifyIconData_.uFlags |= NIF_INFO;
							notifyIconData_.dwInfoFlags = NIIF_INFO;
							if (inputLocked_)
							{
								notifyIconData_.hIcon = hLockIcon_;
								_stprintf_s(notifyIconData_.szInfo, TEXT("入力をロックしました"));
							}
							else
							{
								notifyIconData_.hIcon = hUnlockIcon_;
								_stprintf_s(notifyIconData_.szInfo, TEXT("入力のロックを解除しました"));
							}
							Shell_NotifyIcon(NIM_MODIFY, &notifyIconData_);
						}
                        prevLockState_ = nowLockState;

						if (inputLocked_)
						{
							if ((param->flags & LLKHF_UP) != 0)
							{
								MessageBeep(0xFFFFFFFF);
							}
							return 1;
						}

						return CallNextHookEx(NULL, nCode, wParam, lParam);
					}, GlobalHookTypes::LowLevelKeyboard);


				RegisterGlobalHook(hWnd, [](int nCode, WPARAM wParam, LPARAM lParam)->LRESULT
					{
						if (nCode < 0)
						{
							return CallNextHookEx(NULL, nCode, wParam, lParam);
						}

                        ResetLockTimer();

						if (inputLocked_)
						{
							return 1;
						}

						return CallNextHookEx(NULL, nCode, wParam, lParam);
					}, GlobalHookTypes::LowLevelMouse);


                ResetLockTimer();
			}
			else
			{
				TCHAR buf[64];
				_stprintf_s(buf, _countof(buf), TEXT("NotifyIcon error 0x%x"), GetLastError());
				MessageBox(hWnd, buf, TEXT("Error!!"), MB_OK);
				DestroyWindow(hWnd);
			}
		}
		break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 選択されたメニューの解析:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: HDC を使用する描画コードをここに追加してください...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_TIMER:
        if (inputLocked_ == false)
        {
            inputLocked_ = true;
            notifyIconData_.uFlags |= NIF_INFO;
            notifyIconData_.dwInfoFlags = NIIF_INFO;
            notifyIconData_.hIcon = hLockIcon_;
            _stprintf_s(notifyIconData_.szInfo, TEXT("入力をロックしました"));
            Shell_NotifyIcon(NIM_MODIFY, &notifyIconData_);
        }
        break;
    case WM_DESTROY:
		UnregisterGlobalHook(hWnd, GlobalHookTypes::LowLevelKeyboard);
		UnregisterGlobalHook(hWnd, GlobalHookTypes::LowLevelMouse);
		Shell_NotifyIcon(NIM_DELETE, &notifyIconData_);
        PostQuitMessage(0);
        break;
	case WM_NOTIFYICON:
		if (lParam == WM_RBUTTONUP)
		{
			POINT curPos;
			if (GetCursorPos(&curPos) == TRUE)
			{
				TrackPopupMenu(GetSubMenu(GetMenu(hWnd), 0), 0, curPos.x, curPos.y, 0, hWnd, nullptr );
			}
		}
		break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
