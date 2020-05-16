#pragma once
#include <wgut/ImGuiImplScreenState.h>
#include <imgui.h>
#include <Windows.h>

// HWND g_hWnd = nullptr;
// INT64 g_Time = 0;
// INT64 g_TicksPerSecond = 0;
ImGuiMouseCursor g_LastMouseCursor = ImGuiMouseCursor_COUNT;

bool ImGui_Impl_ScreenState_Init()
{
    // ImGui_ImplWin32_Init(hwnd);
    // if (!::QueryPerformanceFrequency((LARGE_INTEGER *)&g_TicksPerSecond))
    //     return false;
    // if (!::QueryPerformanceCounter((LARGE_INTEGER *)&g_Time))
    //     return false;

    // Setup back-end capabilities flags
    // g_hWnd = (HWND)hwnd;
    ImGuiIO &io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;  // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendPlatformName = "imgui_impl_win32";
    // io.ImeWindowHandle = hwnd;

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array that we will update during the application lifetime.
    // io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    // io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    // io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    // io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    // io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    // io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    // io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    // io.KeyMap[ImGuiKey_Home] = VK_HOME;
    // io.KeyMap[ImGuiKey_End] = VK_END;
    // io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
    // io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    // io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    // io.KeyMap[ImGuiKey_Space] = VK_SPACE;
    // io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    // io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    // io.KeyMap[ImGuiKey_KeyPadEnter] = VK_RETURN;
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    return true;
}

wgut::ScreenState g_lastState{};

void ImGui_Impl_ScreenState_NewFrame(const wgut::ScreenState &state)
{
    // ImGui_ImplWin32_NewFrame();
    ImGuiIO &io = ImGui::GetIO();
    IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

    io.MousePos = ImVec2(state.MouseX, state.MouseY);
    io.MouseDown[0] = state.Has(wgut::MouseButtonFlags::LeftDown);
    io.MouseDown[1] = state.Has(wgut::MouseButtonFlags::RightDown);
    io.MouseDown[2] = state.Has(wgut::MouseButtonFlags::MiddleDown);
    if (state.Has(wgut::MouseButtonFlags::WheelPlus))
    {
        io.MouseWheel = 1;
    }
    else if (state.Has(wgut::MouseButtonFlags::WheelMinus))
    {
        io.MouseWheel = -1;
    }
    else
    {
        io.MouseWheel = 0;
    }

    // Setup display size (every frame to accommodate for window resizing)
    io.DisplaySize = ImVec2((float)state.Width, (float)state.Height);

    // Setup time step
    // INT64 current_time;
    // ::QueryPerformanceCounter((LARGE_INTEGER *)&current_time);
    // io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
    // g_Time = current_time;

    io.DeltaTime =  state.DeltaSeconds();
    g_lastState = state;

    // Read keyboard modifiers inputs
    // io.KeyCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    // io.KeyShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
    // io.KeyAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
    io.KeySuper = false;
    // io.KeysDown[], io.MousePos, io.MouseDown[], io.MouseWheel: filled by the WndProc handler below.

    // Update OS mouse position
    // Set OS mouse position if requested (rarely used, only when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
    // if (io.WantSetMousePos)
    // {
    //     POINT pos = {(int)io.MousePos.x, (int)io.MousePos.y};
    //     ::ClientToScreen(g_hWnd, &pos);
    //     ::SetCursorPos(pos.x, pos.y);
    // }

    // Update game controllers (if enabled and available)
    // ImGui_ImplWin32_UpdateGamepads();
}

void ImGui_Impl_Win32_UpdateMouseCursor()
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
    {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return;
    }

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        ::SetCursor(NULL);
        return;
    }

    // Show OS mouse cursor
    LPTSTR win32_cursor = IDC_ARROW;
    switch (imgui_cursor)
    {
    case ImGuiMouseCursor_Arrow:
        win32_cursor = IDC_ARROW;
        break;
    case ImGuiMouseCursor_TextInput:
        win32_cursor = IDC_IBEAM;
        break;
    case ImGuiMouseCursor_ResizeAll:
        win32_cursor = IDC_SIZEALL;
        break;
    case ImGuiMouseCursor_ResizeEW:
        win32_cursor = IDC_SIZEWE;
        break;
    case ImGuiMouseCursor_ResizeNS:
        win32_cursor = IDC_SIZENS;
        break;
    case ImGuiMouseCursor_ResizeNESW:
        win32_cursor = IDC_SIZENESW;
        break;
    case ImGuiMouseCursor_ResizeNWSE:
        win32_cursor = IDC_SIZENWSE;
        break;
    case ImGuiMouseCursor_Hand:
        win32_cursor = IDC_HAND;
        break;
    case ImGuiMouseCursor_NotAllowed:
        win32_cursor = IDC_NO;
        break;
    }
    ::SetCursor(::LoadCursor(NULL, win32_cursor));
}
