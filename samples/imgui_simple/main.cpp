#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_shader.h>
#include <wgut/OrbitCamera.h>
#include <stdexcept>
#include <iostream>
#include <DirectXMath.h>
#include <imgui.h>
// #include <examples/imgui_impl_win32.h>
#include <examples/imgui_impl_dx11.h>
#include "wgut/ImGuiImplScreenState.h"

auto SHADER = R"(
struct VS
{
    float3 position: POSITION;
};

struct PS
{
    float4 position: SV_POSITION;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 View;
    float4x4 Projection;
};

PS vsMain(VS input)
{
    PS output;
    output.position = mul(mul(Projection, View), float4(input.position, 1));
	return output;
}

float4 psMain(PS input): SV_TARGET
{
    return float4(1, 1, 1, 1);
}
)";

static wgut::d3d11::DrawablePtr CreateDrawable(const Microsoft::WRL::ComPtr<ID3D11Device> &device)
{
    // compile shader
    auto vs = wgut::shader::CompileVS(SHADER, "vsMain");
    if (!vs.ByteCode)
    {
        std::cerr << vs.error_str() << std::endl;
        throw std::runtime_error("fail to compile vs");
    }
    auto inputLayout = wgut::shader::InputLayout::Create(vs.ByteCode);
    if (!inputLayout)
    {
        throw std::runtime_error("fail to ReflectInputLayout");
    }
    auto ps = wgut::shader::CompilePS(SHADER, "psMain");
    if (!ps.ByteCode)
    {
        std::cerr << ps.error_str() << std::endl;
        throw std::runtime_error("fail to compile ps");
    }

    // create vertex buffer
    struct float3
    {
        float x;
        float y;
        float z;
    };
    // counter clockwise ?
    float3 vertices[] = {
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
    };
    uint16_t indices[] = {
        0, 1, 2, 2, 3, 0, // z-
        1, 5, 6, 6, 2, 1, // x+
        5, 4, 7, 7, 6, 5, // z+
        4, 0, 3, 3, 7, 4, // x-
        3, 2, 6, 6, 7, 3, // y+
        0, 4, 5, 5, 1, 0, // y-
    };
    auto vb = std::make_shared<wgut::d3d11::VertexBuffer>();
    vb->Vertices(device, vs.ByteCode, inputLayout->Elements(), vertices);
    vb->Indices(device, indices);

    // create shader
    auto shader = wgut::d3d11::Shader::Create(device, vs.ByteCode, ps.ByteCode);

    auto drawable = std::make_shared<wgut::d3d11::Drawable>(vb);
    auto &submesh = drawable->AddSubmesh();
    submesh->Count = vb->IndexCount();
    submesh->Shader = shader;

    return drawable;
}

struct ImGuiControl
{
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

public:
    ImGuiControl(HWND hwnd, ID3D11Device *device, ID3D11DeviceContext *context)
    {
        // ImGui_ImplWin32_EnableDpiAwareness();

        // // Create application window
        // WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL};
        // ::RegisterClassEx(&wc);
        // HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

        // // Initialize Direct3D
        // if (!CreateDeviceD3D(hwnd))
        // {
        //     CleanupDeviceD3D();
        //     ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        //     return 1;
        // }

        // // Show the window
        // ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        // ::UpdateWindow(hwnd);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
                                                            //io.ConfigViewportsNoAutoMerge = true;
                                                            //io.ConfigViewportsNoTaskBarIcon = true;
                                                            //io.ConfigViewportsNoDefaultParent = true;
                                                            //io.ConfigDockingAlwaysTabBar = true;
                                                            //io.ConfigDockingTransparentPayload = true;
#if 1
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;     // FIXME-DPI: THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI
#endif

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();

        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
        ImGuiStyle &style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Setup Platform/Renderer bindings
        // ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(device, context);
        ImGui_Impl_ScreenState_Init();

        // Load Fonts
        // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
        // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
        // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
        // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
        // - Read 'docs/FONTS.txt' for more instructions and details.
        // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
        io.Fonts->AddFontDefault();
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
        //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
        //IM_ASSERT(font != NULL);
    }

    ~ImGuiControl()
    {
        // Cleanup
        ImGui_ImplDX11_Shutdown();
        // ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void Frame(const wgut::ScreenState &state)
    {
        ImGuiIO &io = ImGui::GetIO();

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        // ImGui_ImplWin32_NewFrame();
        // Start the Dear ImGui frame
        ImGui_Impl_ScreenState_NewFrame(state);
        if (state.Has(wgut::MouseButtonFlags::CursorUpdate))
        {
            ImGui_Impl_Win32_UpdateMouseCursor();
        }
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");          // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        // context->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        // context->ClearRenderTargetView(g_mainRenderTargetView, (float *)&clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }
};

int main(int argc, char **argv)
{
    wgut::Win32Window window(L"CLASS_NAME");
    auto hwnd = window.Create(WINDOW_NAME);
    if (!hwnd)
    {
        throw std::runtime_error("fail to create window");
    }
    window.Show();

    auto device = wgut::d3d11::CreateDeviceForHardwareAdapter();
    if (!device)
    {
        throw std::runtime_error("fail to create device");
    }
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    auto swapchain = wgut::dxgi::CreateSwapChain(device, hwnd);
    if (!swapchain)
    {
        throw std::runtime_error("fail to create swapchain");
    }

    ImGuiControl imgui(hwnd, device.Get(), context.Get());

    auto drawable = CreateDrawable(device);

    auto camera = std::make_shared<wgut::OrbitCamera>(wgut::PerspectiveTypes::D3D);
    struct SceneConstantBuffer
    {
        std::array<float, 16> View;
        std::array<float, 16> Projection;
    };

    auto b0 = wgut::d3d11::ConstantBuffer<SceneConstantBuffer>::Create(device);

    // float clearColor[4] = {0.3f, 0.2f, 0.1f, 1.0f};
    wgut::d3d11::SwapChainRenderTarget rt(swapchain);
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        {
            // update camera
            camera->Update(state);
            auto sceneData = b0->Data();
            sceneData->Projection = camera->state.projection;
            sceneData->View = camera->state.view;
            b0->Upload(context);
        }

        rt.UpdateViewport(device, state.Width, state.Height);
        rt.ClearAndSet(context, &imgui.clear_color.x);

        drawable->Draw(context, b0->Buffer());

        imgui.Frame(state);

        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
