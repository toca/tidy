#pragma once

#include "resource.h"
#include "string"

ATOM                MyRegisterClass(HINSTANCE hInstance);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

BOOL Init(HINSTANCE, int, LPWSTR);
void Close();

void RegisterIcon(HWND hWindow, const std::wstring& commandLine);
void UnregisterIcon();
void OnIconClicked(HWND hWindow);
void OnCommand(HWND hWindows, WPARAM wParam);


DWORD CreateAppProcess(LPWSTR commandLine);
void ShowAppWindow(DWORD threadId);
void HideAppWindow(DWORD threadId);
