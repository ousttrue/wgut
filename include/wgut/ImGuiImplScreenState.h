#pragma once
#include "ScreenState.h"

// HWND g_hWnd = nullptr;
// INT64 g_Time = 0;
// INT64 g_TicksPerSecond = 0;
// ImGuiMouseCursor g_LastMouseCursor = ImGuiMouseCursor_COUNT;

bool ImGui_Impl_ScreenState_Init();
void ImGui_Impl_ScreenState_NewFrame(const wgut::ScreenState &state);
void ImGui_Impl_Win32_UpdateMouseCursor();
