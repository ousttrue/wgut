#include <wgut/Win32Window.h>
#include <stdexcept>

int main(int argc, char **argv)
{
    wgut::Win32Window window(L"CLASS_NAME");

    auto hwnd = window.Create(L"WINDOW_NAME");
    if (!hwnd)
    {
        // error
        throw std::runtime_error("fail to create window");
    }

    window.Show();
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        // do something
    }

    // window closed

    return 0;
}
