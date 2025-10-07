#include "D3D11Engine.h"

D3D11Engine::D3D11Engine()
{
}

D3D11Engine::~D3D11Engine()
{
    Cleanup();
}

HRESULT D3D11Engine::Initialize(HWND WindowHandle, int Width, int Height)
{
    HRESULT ResultHandle = S_OK;
    
    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    ZeroMemory(&SwapChainDesc, sizeof(SwapChainDesc));

    SwapChainDesc.BufferCount = 1;
    SwapChainDesc.BufferDesc.Width = Width;
    SwapChainDesc.BufferDesc.Height = Height;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.OutputWindow = WindowHandle;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.Windowed = TRUE;

    D3D_FEATURE_LEVEL FeatureLevels = D3D_FEATURE_LEVEL_11_0;
    ResultHandle = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &SwapChainDesc,
        &SwapChain,
        &D3DDevice,
        &FeatureLevels,
        &D3DDeviceContext);
    if (FAILED(ResultHandle))
    {
        return ResultHandle;
    }

    ID3D11Texture2D* BackBuffer = nullptr;
    ResultHandle = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&BackBuffer));
    if (FAILED(ResultHandle))
    {
        return ResultHandle;
    }
    ResultHandle = D3DDevice->CreateRenderTargetView(BackBuffer, nullptr, &RenderTargetView);
    BackBuffer->Release();
    if (FAILED(ResultHandle))
    {
        return ResultHandle;
    }

    D3D11_TEXTURE2D_DESC DepthStencilDesc = {};
    DepthStencilDesc.Width = Width;
    DepthStencilDesc.Height = Height;
    DepthStencilDesc.MipLevels = 1;
    DepthStencilDesc.ArraySize = 1;
    DepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthStencilDesc.SampleDesc.Count = 1;
    DepthStencilDesc.SampleDesc.Quality = 0;
    DepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ResultHandle = D3DDevice->CreateTexture2D(&DepthStencilDesc, nullptr, &DepthStencilBuffer);
    if (FAILED(ResultHandle))
    {
        return ResultHandle;
    }
    ResultHandle = D3DDevice->CreateDepthStencilView(DepthStencilBuffer, nullptr, &DepthStencilView);
    if (FAILED(ResultHandle))
    {
        return ResultHandle;
    }

    D3DDeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);

    D3D11_VIEWPORT Viewport ={};
    Viewport.Width = (float)Width;
    Viewport.Height = (float)Height;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;
    D3DDeviceContext->RSSetViewports(1, &Viewport);

    return S_OK;
}

void D3D11Engine::Cleanup()
{
    SafeRelease(&DepthStencilView);
    SafeRelease(&DepthStencilBuffer);
    SafeRelease(&RenderTargetView);
    SafeRelease(&SwapChain);
    SafeRelease(&D3DDeviceContext);
    SafeRelease(&D3DDevice);
}

void D3D11Engine::BeginRender()
{
    float color[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    D3DDeviceContext->ClearRenderTargetView(RenderTargetView, color);
    D3DDeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void D3D11Engine::EndRender()
{
    SwapChain->Present(1, 0);
}

IDXGISurface* D3D11Engine::GetBackBufferSurface()
{
    IDXGISurface* pBackBuffer = nullptr;
    if (SwapChain)
    {
        SwapChain->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(&pBackBuffer));
    }
    return pBackBuffer;
}
