#include <wgut/Win32Window.h>

int main(int argc, char **argv)
{
    wgut::Win32Window window(L"CLASS_NAME");

    auto hwnd = window.Create(L"WINDOW_NAME");
    if (!hwnd)
    {
        // error
        return 1;
    }

    window.Show();
    wgut::ScreenState state;
    while (window.TryGetInput(&state))
    {
        // do something
    }

    // window closed

    return 0;
}
