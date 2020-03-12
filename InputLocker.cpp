// InputLocker.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "InputLocker.h"

#include "GlobalHook/GlobalHook.h"

#include <stdio.h>
#include <shellapi.h>
#include <array>
#include <map>
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
INT_PTR CALLBACK    Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

namespace {
    constexpr UINT WM_NOTIFYICON = WM_APP + 0;
    constexpr DWORD BEEP_LENGTH = 300;
    constexpr DWORD NEWEST_SETTINGS_VERSION = 1;
}

namespace {
    HWND hwnd_;
    bool inputLocked_ = false;
    NOTIFYICONDATA notifyIconData_;
    HICON hLockIcon_;
    HICON hUnlockIcon_;
    UINT_PTR lockTimerID_ = 1241321;
    bool prevLockState_ = false;
    bool altDown_ = false;
    bool ctrlDown_ = false;
    bool lockKeyDown_ = false;


    struct SETTINGS
    {
        bool enableAutoLock = false;
        UINT autoLockInterval = 10 * 1000;
        BYTE lockKey = VK_END;
        bool lockKeyWithAlt = true;
        bool lockKeyWithControl = true;

    }settings_;
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

    void ResetAutoLockTimer()
    {
        if (settings_.enableAutoLock)
            lockTimerID_ = SetTimer(hwnd_, lockTimerID_, settings_.autoLockInterval, nullptr);
    }

    DWORD RegGetDWord(LPCTSTR key)
    {
        DWORD value = 0;
        DWORD size = sizeof(DWORD);
        RegGetValue(HKEY_CURRENT_USER, TEXT("Software\\InputLocker"), key, RRF_RT_REG_DWORD, nullptr, &value, &size);
        return value;
    }

    void RegSetDWord(LPCTSTR key, DWORD value)
    {
        RegSetKeyValue(HKEY_CURRENT_USER, TEXT("Software\\InputLocker"), key, RRF_RT_REG_DWORD, &value, sizeof(value));
    }

    void SaveSettings()
    {
        RegSetDWord(TEXT("settingsVersion"), NEWEST_SETTINGS_VERSION);
        RegSetDWord(TEXT("enableAutoLock"), settings_.enableAutoLock ? 1 : 0);
        RegSetDWord(TEXT("autoLockInterval"), static_cast<DWORD>(settings_.autoLockInterval));
        RegSetDWord(TEXT("lockKeyWithAlt"), settings_.lockKeyWithAlt ? 1 : 0);
        RegSetDWord(TEXT("lockKeyWithControl"), settings_.lockKeyWithControl ? 1 : 0);
        RegSetDWord(TEXT("lockKey"), static_cast<DWORD>(settings_.lockKey));
    }

    void LoadSettings()
    {
        HKEY key;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\InputLocker"), 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
        {
            auto version = RegGetDWord(TEXT("settingsVersion"));
            settings_.enableAutoLock = RegGetDWord(TEXT("enableAutoLock")) != 0 ? true : false;
            settings_.autoLockInterval = static_cast<DWORD>(RegGetDWord(TEXT("autoLockInterval")));
            if (version >= 1)
            {
                settings_.lockKeyWithAlt = RegGetDWord(TEXT("lockKeyWithAlt")) != 0 ? true : false;
                settings_.lockKeyWithControl = RegGetDWord(TEXT("lockKeyWithControl")) != 0 ? true : false;
                settings_.lockKey = static_cast<BYTE>(RegGetDWord(TEXT("lockKey")));
            }
        }
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
                        ResetAutoLockTimer();

                        auto info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                        //Log(TEXT("%d %d"), wParam, info->vkCode);
                        if (wParam == WM_SYSKEYDOWN)
                        {
                            altDown_ = true;
                            if(info->vkCode == settings_.lockKey)
                                lockKeyDown_ = true;
                            else if(info->vkCode == VK_LCONTROL || info->vkCode == VK_RCONTROL)
                                ctrlDown_ = true;
                        }
                        else if (wParam == WM_KEYDOWN)
                        {
                            if (info->vkCode == settings_.lockKey)
                                lockKeyDown_ = true;
                            else if (info->vkCode == VK_LCONTROL || info->vkCode == VK_RCONTROL)
                                ctrlDown_ = true;
                            else if (info->vkCode == VK_LMENU || info->vkCode == VK_RMENU)
                                altDown_ = true;
                        }
                        else if (wParam == WM_SYSKEYUP)
                        {
                            if (info->vkCode == settings_.lockKey)
                                lockKeyDown_ = false;
                            else if (info->vkCode == VK_LCONTROL || info->vkCode == VK_RCONTROL)
                                ctrlDown_ = false;
                        }
                        else if (wParam == WM_KEYUP)
                        {
                            if (info->vkCode == settings_.lockKey)
                                lockKeyDown_ = false;
                            else if (info->vkCode == VK_LCONTROL || info->vkCode == VK_RCONTROL)
                                ctrlDown_ = false;
                            else if (info->vkCode == VK_LMENU || info->vkCode == VK_RMENU)
                                altDown_ = false;
                        }

                        //Log(TEXT("alt:%s ctrl:%s key:%s"), altDown_ ? TEXT("o") : TEXT("x"), ctrlDown_ ? TEXT("o") : TEXT("x"), lockKeyDown_ ? TEXT("o") : TEXT("x"));

                        bool nowLockState = (settings_.lockKeyWithAlt == false || altDown_) && (settings_.lockKeyWithControl == false || ctrlDown_) && lockKeyDown_;
                        
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

                        ResetAutoLockTimer();

						if (inputLocked_)
						{
							return 1;
						}

						return CallNextHookEx(NULL, nCode, wParam, lParam);
					}, GlobalHookTypes::LowLevelMouse);


                ResetAutoLockTimer();

                LoadSettings();
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
            case IDM_SETTINGS:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG), hWnd, Settings);
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

INT_PTR CALLBACK    Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        SendMessage(GetDlgItem(hDlg, IDC_AUTO_LOCK_CHECK), BM_SETCHECK, settings_.enableAutoLock ? BST_CHECKED : BST_UNCHECKED, 0);

        SendMessage(GetDlgItem(hDlg, IDC_USE_CTRL_KEY_CHECK), BM_SETCHECK, settings_.lockKeyWithControl ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(GetDlgItem(hDlg, IDC_USE_ALT_KEY_CHECK), BM_SETCHECK, settings_.lockKeyWithAlt ? BST_CHECKED : BST_UNCHECKED, 0);

        auto hKeyCB = GetDlgItem(hDlg, IDC_LOCK_KEY_COMBO);
        constexpr std::pair<BYTE, LPCTSTR> KEY_MAPS[] = { 
            {static_cast<BYTE>('A'), TEXT("A")},
            {static_cast<BYTE>('B'), TEXT("B")},
            {static_cast<BYTE>('C'), TEXT("C")},
            {static_cast<BYTE>('D'), TEXT("D")},
            {static_cast<BYTE>('E'), TEXT("E")},
            {static_cast<BYTE>('F'), TEXT("F")},
            {static_cast<BYTE>('G'), TEXT("G")},
            {static_cast<BYTE>('H'), TEXT("H")},
            {static_cast<BYTE>('I'), TEXT("I")},
            {static_cast<BYTE>('J'), TEXT("J")},
            {static_cast<BYTE>('K'), TEXT("K")},
            {static_cast<BYTE>('L'), TEXT("L")},
            {static_cast<BYTE>('M'), TEXT("M")},
            {static_cast<BYTE>('N'), TEXT("N")},
            {static_cast<BYTE>('O'), TEXT("O")},
            {static_cast<BYTE>('P'), TEXT("P")},
            {static_cast<BYTE>('Q'), TEXT("Q")},
            {static_cast<BYTE>('R'), TEXT("R")},
            {static_cast<BYTE>('S'), TEXT("S")},
            {static_cast<BYTE>('T'), TEXT("T")},
            {static_cast<BYTE>('U'), TEXT("U")},
            {static_cast<BYTE>('V'), TEXT("V")},
            {static_cast<BYTE>('W'), TEXT("W")},
            {static_cast<BYTE>('X'), TEXT("X")},
            {static_cast<BYTE>('Y'), TEXT("Y")},
            {static_cast<BYTE>('Z'), TEXT("Z")},
            {static_cast<BYTE>('0'), TEXT("0")},
            {static_cast<BYTE>('1'), TEXT("1")},
            {static_cast<BYTE>('2'), TEXT("2")},
            {static_cast<BYTE>('3'), TEXT("3")},
            {static_cast<BYTE>('4'), TEXT("4")},
            {static_cast<BYTE>('5'), TEXT("5")},
            {static_cast<BYTE>('6'), TEXT("6")},
            {static_cast<BYTE>('7'), TEXT("7")},
            {static_cast<BYTE>('8'), TEXT("8")},
            {static_cast<BYTE>('9'), TEXT("9")},
            {static_cast<BYTE>(VK_F1), TEXT("F1")},
            {static_cast<BYTE>(VK_F2), TEXT("F2")},
            {static_cast<BYTE>(VK_F3), TEXT("F3")},
            {static_cast<BYTE>(VK_F4), TEXT("F4")},
            {static_cast<BYTE>(VK_F5), TEXT("F5")},
            {static_cast<BYTE>(VK_F6), TEXT("F6")},
            {static_cast<BYTE>(VK_F7), TEXT("F7")},
            {static_cast<BYTE>(VK_F8), TEXT("F8")},
            {static_cast<BYTE>(VK_F9), TEXT("F9")},
            {static_cast<BYTE>(VK_F10), TEXT("F10")},
            {static_cast<BYTE>(VK_F11), TEXT("F11")},
            {static_cast<BYTE>(VK_F12), TEXT("F12")},
            {static_cast<BYTE>(VK_SHIFT), TEXT("Shift")},
            {static_cast<BYTE>(VK_TAB), TEXT("Tab")},
            {static_cast<BYTE>(VK_SNAPSHOT), TEXT("PrintScreen")},
            {static_cast<BYTE>(VK_SCROLL), TEXT("ScrollLock")},
            {static_cast<BYTE>(VK_PAUSE), TEXT("Pause")},
            {static_cast<BYTE>(VK_INSERT), TEXT("Insert")},
            {static_cast<BYTE>(VK_HOME), TEXT("Home")},
            {static_cast<BYTE>(VK_PRIOR), TEXT("PageUp")},
            {static_cast<BYTE>(VK_NEXT), TEXT("PageDown")},
            {static_cast<BYTE>(VK_END), TEXT("End")},
            {static_cast<BYTE>(VK_SPACE), TEXT("Space")},
            {static_cast<BYTE>(VK_RETURN), TEXT("Enter")},
            {static_cast<BYTE>(VK_ESCAPE), TEXT("Escape")},
        };

        for (int i = 0; i < _countof(KEY_MAPS); ++i)
        {
            SendMessage(hKeyCB, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(KEY_MAPS[i].second));
            SendMessage(hKeyCB, CB_SETITEMDATA, i, static_cast<LPARAM>(KEY_MAPS[i].first));
            if(KEY_MAPS[i].first == settings_.lockKey)
                SendMessage(hKeyCB, CB_SETCURSEL, i, 0);
        }

        RECT rc;
        GetClientRect(hKeyCB, &rc);
        SetWindowPos(hKeyCB, HWND_TOP, 0, 0, rc.right, (int)SendMessage(hKeyCB, CB_GETITEMHEIGHT, -1, 0) + (int)SendMessage(hKeyCB, CB_GETITEMHEIGHT, 0, 0) * 16, SWP_NOMOVE | SWP_NOZORDER);
      
        TCHAR buf[128] = {};
        _itot_s(static_cast<int>(settings_.autoLockInterval / 1000), buf, _countof(buf), 10);
        SetWindowText(GetDlgItem(hDlg, IDC_AUTO_LOCK_TIME_EDIT), buf);
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            if(SendMessage(GetDlgItem(hDlg, IDC_AUTO_LOCK_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED)
                settings_.enableAutoLock = true;
            else
                settings_.enableAutoLock = false;

            if (SendMessage(GetDlgItem(hDlg, IDC_USE_CTRL_KEY_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED)
                settings_.lockKeyWithControl = true;
            else
                settings_.lockKeyWithControl = false;

            if (SendMessage(GetDlgItem(hDlg, IDC_USE_ALT_KEY_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED)
                settings_.lockKeyWithAlt = true;
            else
                settings_.lockKeyWithAlt = false;

            auto curKeyIndex = SendMessage(GetDlgItem(hDlg, IDC_LOCK_KEY_COMBO), CB_GETCURSEL, 0, 0);
            settings_.lockKey = static_cast<BYTE>(SendMessage(GetDlgItem(hDlg, IDC_LOCK_KEY_COMBO), CB_GETITEMDATA, curKeyIndex, 0));

            TCHAR buf[128] = {};
            GetWindowText(GetDlgItem(hDlg, IDC_AUTO_LOCK_TIME_EDIT), buf, _countof(buf));
            settings_.autoLockInterval = static_cast<DWORD>(_ttoi(buf)) * 1000;

            SaveSettings();
            ResetAutoLockTimer();
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
