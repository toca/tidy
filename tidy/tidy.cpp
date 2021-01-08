// entry point
#include "framework.h"

#include <memory>
#include <shellapi.h>
#include "tidy.h"
#include <string>
#include <iostream>


constexpr DWORD WM_TRAYMESSAGE = WM_APP + 1;
constexpr LPCWSTR WINDOW_CLASS_NAME = L"tidy-class";
// global var
HINSTANCE hInst;
NOTIFYICONDATA notifyIconData{};
HICON icon{};
STARTUPINFO startupInfo{};
PROCESS_INFORMATION processInfo{};
HWINEVENTHOOK winEventHook{};




// TODO
// global -> arg
// namespace?
// detect app close





int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR commandLine,
                     _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    // TODO: ここにコードを挿入してください。

    MyRegisterClass(hInstance);

    // initialize application 
    if (!Init (hInstance, nCmdShow, commandLine))
    {
        return FALSE;
    }
    

    // target process
    auto res = CreateAppProcess(commandLine);
    if (res != ERROR_SUCCESS)
    {
        auto message = std::wstring(L"Failed to CreateProcess. Err: ") + std::to_wstring(res);
        MessageBoxW(nullptr, message.c_str(), L"Error", MB_OK);
        return res;
    }


    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RESIDENT));

    MSG msg;
    // main message loop
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



// register window class
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RESIDENT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_RESIDENT);
    wcex.lpszClassName  = WINDOW_CLASS_NAME;
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
BOOL Init(HINSTANCE hInstance, int nCmdShow, LPWSTR commandLine)
{
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   HWND hWnd = CreateWindowExW(
       WS_EX_TOOLWINDOW,
       WINDOW_CLASS_NAME,
       L"tidy-window",
       WS_OVERLAPPEDWINDOW,
       CW_USEDEFAULT,   // x
       0,               // y
       0,               // width
       0,               // height
       nullptr,
       nullptr,
       hInstance,
       nullptr
   );

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   // add icon to notification area (task tray)
   RegisterIcon(hWnd, commandLine);

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
    case WM_COMMAND:
        OnCommand(hWnd, wParam);
        //return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: HDC を使用する描画コードをここに追加してください...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        Close();
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        OutputDebugStringA("WM_CLOSE");
        break;
    case WM_TRAYMESSAGE : // タスクトレイのアイコンへのメッセージ
        if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN)
        {
            OnIconClicked(hWnd);
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


void Close()
{
    UnhookWinEvent(winEventHook);
    // close target process
    //processInformation.dwProcessId
    EnumWindows([](HWND window, LPARAM lParam) -> BOOL {
        DWORD procId = 0;
        GetWindowThreadProcessId(window, &procId);
        if (procId == lParam)
        {
            SendMessageTimeout(window, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG, 30000, NULL);
        }
        return TRUE;
    }, processInfo.dwProcessId);
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    // delete notify icon
    UnregisterIcon();
}




// target application /////////////////////////////////////////////////

DWORD CreateAppProcess(LPWSTR commandLine)
{
    startupInfo.cb = sizeof(STARTUPINFO);
    
    BOOL result = CreateProcessW(
        nullptr,
        commandLine,
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startupInfo, &processInfo
    );
    if (!result)
    {
        return ::GetLastError();
    }
    // wait for process started
    ::WaitForInputIdle(processInfo.hProcess, INFINITE);
    // detect window minimize
    winEventHook = ::SetWinEventHook(
        EVENT_SYSTEM_MINIMIZESTART,
        EVENT_SYSTEM_MINIMIZESTART,
        nullptr,
        [](HWINEVENTHOOK hWinEventHook,
            DWORD event,
            HWND hwnd,
            LONG idObject,
            LONG idChild,
            DWORD dwEventThread,
            DWORD dwmsEventTime) {
                DWORD pid = 0;
                ::GetWindowThreadProcessId(hwnd, &pid);
                //if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
                if (processInfo.dwProcessId == pid)
                {
                    // hide target app
                    HideAppWindow(processInfo.dwThreadId);
                }},
        0, //processInfo.dwProcessId,
        0, //processInfo.dwThreadId,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS /*| WINEVENT_SKIPOWNTHREAD*/
    );
    if (!winEventHook)
    {
        auto le = GetLastError();
        std::clog << "SetWinEventHook failed" << std::endl;
        return 1;
    }


    return ERROR_SUCCESS;
}


void ShowAppWindow(DWORD threadId)
{
    // set window style
    EnumWindows([](HWND window, LPARAM lParam)->BOOL {
        if (GetWindowThreadProcessId(window, NULL) == lParam)
        {
            auto style = GetWindowLongPtr(window, GWL_EXSTYLE);
            SetWindowLongPtr(window, GWL_EXSTYLE, (style & ~WS_EX_TOOLWINDOW )| WS_EX_APPWINDOW);

            BOOL result = ::SetForegroundWindow(window);
            ::SendMessage(window, WM_SYSCOMMAND, SC_RESTORE, 0);
            return FALSE;
        }
        else
        {
            return TRUE;

        }
    }, processInfo.dwThreadId);
}


void HideAppWindow(DWORD threadId)
{
    // set window style
    EnumWindows([](HWND window, LPARAM lParam)->BOOL {
        if (GetWindowThreadProcessId(window, NULL) == lParam)
        {
            auto style = GetWindowLongPtr(window, GWL_EXSTYLE);
            SetWindowLongPtr(window, GWL_EXSTYLE, (style & ~WS_EX_APPWINDOW) | WS_EX_TOOLWINDOW);
            return FALSE;
        }
        else
        {
            return TRUE;

        }
    }, processInfo.dwThreadId);
}




// icon ///////////////////////////////////////////////////////////////
void RegisterIcon(HWND hWindow, const std::wstring& commandLine) 
{
    notifyIconData.cbSize = sizeof(NOTIFYICONDATA);

    ::ExtractIconExW(commandLine.c_str(), 0, nullptr, &icon, 1);
    if (icon)
    {
        ICONINFO info{};
        ::GetIconInfo(icon, &info);
        notifyIconData.hIcon = icon;
    }
    else
    {
        notifyIconData.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    notifyIconData.hWnd = hWindow;
    notifyIconData.uCallbackMessage = WM_TRAYMESSAGE;
    notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    //notifyIconData.uID = ID_TASKTRAY;
    std::wstring tip = L"tidy-" + commandLine;
    ::wcscpy_s(notifyIconData.szTip, sizeof(notifyIconData.szTip)/2, tip.c_str());
    Shell_NotifyIconW(NIM_ADD, &notifyIconData);
}


void UnregisterIcon()
{
    ::DestroyIcon(icon);
    Shell_NotifyIconW(NIM_DELETE, &notifyIconData);
}


void OnIconClicked(HWND hWindow)
{
    ::SetForegroundWindow(hWindow);
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = LoadMenu((HINSTANCE)GetWindowLongPtr(hWindow, GWLP_HINSTANCE), MAKEINTRESOURCE(IDR_MENU));
    TrackPopupMenu(GetSubMenu(menu, 0), TPM_LEFTALIGN,/* X */pt.x,/* Y */pt.y, NULL, hWindow, NULL);
    DestroyMenu(menu);
}


void OnCommand(HWND hWindow, WPARAM wParam)
{
    int wmId = LOWORD(wParam);
    // 選択されたメニューの解析:
    switch (UINT(wParam))
    {
    case ID__TERMINATE:
        DestroyWindow(hWindow);
        break;
    case ID__SHOWAPP:
        //MessageBoxA(hWindow, "show application", "", MB_OK);
        ShowAppWindow(processInfo.dwThreadId);
        break;
    }
}