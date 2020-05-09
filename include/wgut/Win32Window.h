#pragma once
#include "ScreenState.h"
#include <Windows.h>
#include <string>
#include <functional>

namespace wgut
{
class Win32Window
{
    HWND m_hwnd = NULL;
    ScreenState m_state{};
    std::wstring m_className;
    HINSTANCE m_hInstance;
    bool m_enableSetCursor = true;

    LARGE_INTEGER m_freq;
    float m_freqInv;
    LARGE_INTEGER m_lastTime{};
    LARGE_INTEGER m_startTime{};

public:
    Win32Window(const wchar_t *className);
    ~Win32Window();
    HWND Create(const wchar_t *titleName, int width = 0, int height = 0);
    void Show(int nCmdShow = SW_SHOW);
    bool TryGetState(ScreenState *pState);
    void SetEnableSetCursor(bool enable) { m_enableSetCursor = enable; }
    std::function<void()> OnDestroy;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};
} // namespace screenstate
