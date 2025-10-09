#include "D3D11.h"

#include <d3dcompiler.h>

#include "../Utility/ComUtils.h"
#include <DirectXMath.h>

#include "Buffers/VertexBuffer.h"

struct SimpleVertex
{
    DirectX::XMFLOAT3 Position;
};

//Global Variables
ID3D11VertexShader* VertexShader = nullptr;
ID3D11PixelShader* PixelShader = nullptr;
ID3D11InputLayout* VertexLayout = nullptr;

D3D11::D3D11()
{
}

D3D11::~D3D11()
{
    Cleanup();
}

HRESULT D3D11::Initialize(HWND WindowHandle, int Width, int Height)
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
    

    /*IDXGIFactory1* DxgiFactory = nullptr;
    {
        IDXGIDevice* DxgiDevice = nullptr;
        ResultHandle = D3DDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
        if (SUCCEEDED(ResultHandle))
        {
            IDXGIAdapter* DxgiAdapter = nullptr;
            ResultHandle = DxgiDevice->GetAdapter(&DxgiAdapter);
            if (SUCCEEDED(ResultHandle))
            {
                ResultHandle = DxgiAdapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&DxgiFactory));
                DxgiAdapter->Release();
            }
            DxgiDevice->Release();
        }
        if (FAILED(ResultHandle))
        {
            return ResultHandle;
        }
    }*/
    IDXGIFactory1* DxgiFactory = nullptr;
    ResultHandle = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&DxgiFactory));
    if (FAILED(ResultHandle))
    {
        return ResultHandle;
    }
    IDXGIAdapter1* Adapter = nullptr;
    ResultHandle = DxgiFactory->EnumAdapters1(0, &Adapter);
    if (FAILED(ResultHandle))
    {
        return ResultHandle;
    }
    
    D3D_FEATURE_LEVEL FeatureLevelsRequested = D3D_FEATURE_LEVEL_11_0;
    UINT NumLevelsRequested = 1;
    D3D_FEATURE_LEVEL FeatureLevelsSupported;
    ResultHandle = D3D11CreateDeviceAndSwapChain(
        Adapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &FeatureLevelsRequested,
        NumLevelsRequested,
        D3D11_SDK_VERSION,
        &SwapChainDesc,
        &SwapChain,
        &D3DDevice,
        &FeatureLevelsSupported,
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

    // Compile vertex shader
    ID3DBlob* VertexShaderBlob = nullptr;
    ResultHandle = CompileShaderFromFile(L"Engine/Graphics/Shaders/VertexShader.hlsl", "VS", "vs_5_0", &VertexShaderBlob);
    if (FAILED(ResultHandle))
    {
        MessageBox( nullptr,
             L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return ResultHandle;
    }

    // Create vertex shader
    ResultHandle = D3DDevice->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), nullptr, &VertexShader);
    if (FAILED(ResultHandle))
    {
        VertexShaderBlob->Release();
        return ResultHandle;
    }

    // Define input layout
    D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA,0},   
    };
    UINT NumElements = ARRAYSIZE(Layout);

    // Create input layout
    ResultHandle = D3DDevice->CreateInputLayout(Layout, NumElements, VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), &VertexLayout);
    VertexShaderBlob->Release();
    if (FAILED(ResultHandle)) return ResultHandle;

    // Set Input layout
    D3DDeviceContext->IASetInputLayout(VertexLayout);

    // Compile pixel shader
    ID3DBlob* PixelShaderBlob = nullptr;
    ResultHandle = CompileShaderFromFile(L"Engine/Graphics/Shaders/PixelShader.hlsl", "PS", "ps_5_0", &PixelShaderBlob);
    if (FAILED(ResultHandle))
    {
        MessageBox( nullptr,
            L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return ResultHandle;
    }

    // Create pixel shader
    ResultHandle = D3DDevice->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), nullptr, &PixelShader);
    PixelShaderBlob->Release();
    if (FAILED(ResultHandle)) return ResultHandle;

    //Create vertex buffer
    SimpleVertex vertices[] =
    {
        DirectX::XMFLOAT3( 0.0f, 0.5f, 0.5f ),
        DirectX::XMFLOAT3( 0.5f, -0.5f, 0.5f ),
        DirectX::XMFLOAT3( -0.5f, -0.5f, 0.5f ),
    };

    VertexBuffer<SimpleVertex>* SimpleVertexBuffer = nullptr;
    SimpleVertexBuffer = new VertexBuffer<SimpleVertex>();
    SimpleVertexBuffer->Initialize(D3DDevice, vertices, ARRAYSIZE(vertices));
    SimpleVertexBuffer->Bind(D3DDeviceContext);
    D3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    return S_OK;
}

HRESULT D3D11::CompileShaderFromFile(const WCHAR* FileName, LPCSTR EntryPoint, LPCSTR ShaderModel, ID3DBlob** BlobOut)
{
    HRESULT ResultHandle = S_OK;
    DWORD ShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    ShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    ShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* ErrorBlob = nullptr;
    ResultHandle = D3DCompileFromFile(FileName, nullptr, nullptr, EntryPoint, ShaderModel,
        ShaderFlags, 0, BlobOut, &ErrorBlob);
    if (FAILED(ResultHandle))
    {
        if (ErrorBlob)
        {
            OutputDebugStringA(reinterpret_cast<const char*>(ErrorBlob->GetBufferPointer()));
            ErrorBlob->Release();
        }
        return ResultHandle;
    }
    if (ErrorBlob)
    {
        ErrorBlob->Release();
    }
    return S_OK;	
}

void D3D11::Cleanup()
{
    SafeRelease(&DepthStencilView);
    SafeRelease(&DepthStencilBuffer);
    SafeRelease(&RenderTargetView);
    SafeRelease(&SwapChain);
    SafeRelease(&D3DDeviceContext);
    SafeRelease(&D3DDevice);
}

void D3D11::BeginRender()
{
    float color[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    D3DDeviceContext->ClearRenderTargetView(RenderTargetView, color);

    D3DDeviceContext->VSSetShader(VertexShader, nullptr, 0);
    D3DDeviceContext->PSSetShader(PixelShader, nullptr, 0);
    D3DDeviceContext->Draw(3, 0);
    
    
    D3DDeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void D3D11::EndRender()
{
    SwapChain->Present(1, 0);
}

IDXGISurface* D3D11::GetBackBufferSurface()
{
    IDXGISurface* pBackBuffer = nullptr;
    if (SwapChain)
    {
        SwapChain->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(&pBackBuffer));
    }
    return pBackBuffer;
}
