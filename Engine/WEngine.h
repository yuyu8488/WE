#pragma once
#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <wincodec.h>

#include "../Engine/Graphics/D3D11.h"

#pragma comment(lib, "d2d1.lib")

namespace Grid
{
    constexpr float CellSize = 10.f;
}

// My Custom Class Header
#include "../Object/UObject.h"

#ifndef Assert
#if defined( DEBUG ) || defined( _DEBUG )
#define Assert(b) do {if (!(b)) {OutputDebugStringA("Assert: " #b "\n");}} while(0)
#else
#define Assert(b)
#endif 
#endif

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

class WEngine
{
public:
    WEngine();
    ~WEngine();

    HRESULT Initialize();
    void RunMessageLoop();

    HWND GetWindowHandle() const {return WindowHandle;} 
    ID2D1RenderTarget* GetRenderTarget() const { return RenderTarget;}
    ID2D1SolidColorBrush* GetLightSlateGrayBrush() const {return LightSlateGrayBrush;}

    std::vector<UObject*> GetObjects() const {return Objects;}
    FORCEINLINE void AddObject(UObject* InObject)
    {
        Objects.push_back(InObject);
    }

private:
    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources(int Width, int Height);
 
    void DiscardDeviceResources();
	
    HRESULT OnRender();
    void OnResize(UINT width, UINT height);

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    D3D11 D3D11;
    
    HWND WindowHandle = nullptr;
    ID2D1Factory* D2DFactory = nullptr;
    ID2D1RenderTarget* RenderTarget = nullptr;
    ID2D1SolidColorBrush* LightSlateGrayBrush = nullptr;
    ID2D1SolidColorBrush* CornFlowerBlueBrush = nullptr;
    
    // My Object
    std::vector<UObject*> Objects;
};
