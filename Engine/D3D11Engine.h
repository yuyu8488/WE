#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

template <class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease != NULL)
    {
        (*ppInterfaceToRelease)->Release();
        (*ppInterfaceToRelease) = NULL;
    }
}

class D3D11Engine
{
public:
    D3D11Engine();
    ~D3D11Engine();

    HRESULT Initialize(HWND WindowHandle, int Width, int Height);
    
    void Cleanup();

    void BeginRender();

    void EndRender();

    ID3D11Device* GetDevice() const {return D3DDevice;}
    ID3D11DeviceContext* GetDeviceContext() const {return D3DDeviceContext;}
    IDXGISurface* GetBackBufferSurface();

private:
    ID3D11Device* D3DDevice = nullptr;
    ID3D11DeviceContext* D3DDeviceContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    ID3D11RenderTargetView* RenderTargetView = nullptr;
    ID3D11Texture2D* DepthStencilBuffer = nullptr;
    ID3D11DepthStencilView* DepthStencilView = nullptr;
};
