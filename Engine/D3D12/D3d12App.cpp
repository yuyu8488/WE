#include "D3d12App.h"
#include <WindowsX.h>

D3d12App* D3d12App::mApp = nullptr;

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK MainWndProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    return D3d12App::GetApp()->MsgProc(WindowHandle, Message, WParam, LParam);
}


D3d12App::D3d12App(HINSTANCE hInstance) : mhAppInst(hInstance)
{
    assert(mApp == nullptr);
    mApp = this;
}

D3d12App::~D3d12App()
{
    if (mD3dDevice != nullptr)
    {
        FlushCommandQueue();
    }
}

D3d12App* D3d12App::GetApp()
{
    return mApp;
}

HINSTANCE D3d12App::GetAppInstance() const
{
    return mhAppInst;
}

HWND D3d12App::GetMainWindow() const
{
    return mhMainWnd;
}

float D3d12App::GetAspectRatio() const
{
    return (float)(mClientWidth) / (float)(mClientHeight);
}

bool D3d12App::Get4xMsaaState() const
{
    return m4xMsaaState;
}

void D3d12App::Set4xMsaaState(bool Value)
{
    if (m4xMsaaState != Value)
    {
        m4xMsaaState = Value;

        CreateSwapChain();
        OnResize();
    }
}

int D3d12App::Run()
{
    MSG Msg = {nullptr};

    mTimer.Reset();

    while (Msg.message != WM_QUIT)
    {
        if (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }
        // Do animation/game stuff.
        else
        {
            mTimer.Tick();

            if (!mAppPaused)
            {
                CalculateFrameStats();
                Update(mTimer);
                Draw(mTimer);
            }
            else
            {
                Sleep(100);
            }
        }
    }
    return (int)Msg.wParam;
}

bool D3d12App::Initialize()
{
    if (!InitMainWindow())
    {
        return false;
    }

    if (!InitDirect3D())
    {
        return false;
    }
    
    OnResize();

    return true;
}

LRESULT D3d12App::MsgProc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    switch (Msg)
    {
    case WM_ACTIVATE:
        if (LOWORD(WParam) == WA_INACTIVE)
        {
            mAppPaused = true;
            mTimer.Stop();
        }
        else
        {
            mAppPaused = false;
            mTimer.Start();
        }
        break;

    case WM_SIZE:
        mClientWidth = LOWORD(LParam);
        mClientHeight = HIWORD(LParam);
        if (mD3dDevice)
        {
            if (WParam == SIZE_MINIMIZED)
            {
                mAppPaused = true;
                mMinimized = true;
                mMaximized = false;
            }
            else if (WParam == SIZE_MAXIMIZED)
            {
                mAppPaused = false;
                mMinimized = false;
                mMaximized = true;
                OnResize();
            }
            else if (WParam == SIZE_RESTORED)
            {
                if (mMinimized)
                {
                    mAppPaused = false;
                    mMinimized = false;
                    OnResize();
                }
                else if (mMinimized)
                {
                    mAppPaused = false;
                    mMaximized = false;
                    OnResize();
                }
                else if(mResizing)
                {

                }
                else
                {
                    OnResize();
                }
            }
        }
        break;
    case WM_ENTERSIZEMOVE:
        mAppPaused = true;
        mResizing = true;
        mTimer.Stop();
        break;
    case WM_EXITSIZEMOVE:
        mAppPaused = false;
        mResizing = false;
        mTimer.Start();
        OnResize();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)LParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)LParam)->ptMinTrackSize.y = 200;
        break;
    }

    return DefWindowProc(Hwnd, Msg, WParam, LParam);
}

void D3d12App::Update(const GameTimer& gt)
{
}

void D3d12App::Draw(const GameTimer& gt)
{
}

void D3d12App::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    mD3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf()));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    mD3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf()));
}

void D3d12App::OnResize()
{
    assert(mD3dDevice);
    assert(mSwapChain);
    assert(mDirectCmdListAlloc);

    FlushCommandQueue();

    mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

    for (int i = 0; i < SwapChainBufferCount; i++)
    {
        mSwapChainBuffer[i].Reset();
    }
    mDepthStencilBuffer.Reset();

    mSwapChain->ResizeBuffers(
        SwapChainBufferCount,
        mClientWidth, mClientHeight,
        mBackBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

    mCurrBackBuffer = 0;
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i]));

        mD3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);

        rtvHeapHandle.Offset(1, mRtvDescriptorSize);
    }

    D3D12_RESOURCE_DESC DepthStencilDesc;
    DepthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    DepthStencilDesc.Alignment = 0;
    DepthStencilDesc.Width = mClientWidth;
    DepthStencilDesc.Height = mClientHeight;
    DepthStencilDesc.DepthOrArraySize = 1;
    DepthStencilDesc.MipLevels = 1;
    DepthStencilDesc.MipLevels = 1;
    DepthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    DepthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    DepthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    DepthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.f;
    optClear.DepthStencil.Stencil = 0;
    
    CD3DX12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    mD3dDevice->CreateCommittedResource(
        &HeapProps,
        D3D12_HEAP_FLAG_NONE,
        &DepthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf()));


    D3D12_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDesc;
    DepthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;
    DepthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    DepthStencilViewDesc.Format = mDepthStencilFormat;
    DepthStencilViewDesc.Texture2D.MipSlice = 0;
    mD3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &DepthStencilViewDesc, GetDepthStencilView());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), 
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &barrier);

    mCommandList->Close();
    ID3D12CommandList* CommandList[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(CommandList), CommandList);

    FlushCommandQueue();

    mScreenViewport.TopLeftX = 0;
    mScreenViewport.TopLeftY = 0;
    mScreenViewport.Width = static_cast<float>(mClientWidth);
    mScreenViewport.Height = static_cast<float>(mClientHeight);
    mScreenViewport.MinDepth = 0.f;
    mScreenViewport.MaxDepth = 1.f;

    mScissorRect = { 0,0,mClientWidth,mClientHeight };
}

bool D3d12App::InitMainWindow()
{
    WNDCLASS WC;
    WC.style = CS_HREDRAW | CS_VREDRAW;
    WC.lpfnWndProc = MainWndProc;
    WC.cbClsExtra = 0;
    WC.cbWndExtra = 0;
    WC.hInstance = mhAppInst;
    WC.hIcon = LoadIcon(0, IDI_APPLICATION);
    WC.hCursor = LoadCursor(0, IDC_ARROW);
    WC.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    WC.lpszMenuName = 0;
    WC.lpszClassName = L"MainWindow";

    if (!RegisterClass(&WC))
    {
        MessageBox(0, L"RegisterClass Failed.", 0, 0);
        return false;
    }

    RECT Rect = { 0,0,mClientWidth, mClientHeight };
    AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, false);
    int Width = Rect.right - Rect.left;
    int Height = Rect.bottom - Rect.top;

    mhMainWnd = CreateWindow(L"MainWindow", mMainWndCaption.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, Width, Height, 0, 0, mhAppInst, 0);

    if (!mhMainWnd)
    {
        MessageBox(0, L"CreateWindow Failed", 0, 0);
        return false;
    }

    ShowWindow(mhMainWnd, SW_SHOW);
    UpdateWindow(mhMainWnd);

    return true;
}

void D3d12App::FlushCommandQueue()
{
    mCurrentFence++;

    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE EventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        mFence->SetEventOnCompletion(mCurrentFence, EventHandle);
        
        if (EventHandle)
        {
			WaitForSingleObject(EventHandle, INFINITE);
			CloseHandle(EventHandle);
        }
    }
}

ID3D12Resource* D3d12App::GetCurrentBackBuffer() const
{
    return NULL;
}

bool D3d12App::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG)
{
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
}
#endif

    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mDxgiFactory)));

    HRESULT ResultHandle = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&mD3dDevice));

    if (FAILED(ResultHandle))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> WarpAdapter;
        ThrowIfFailed(mDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&WarpAdapter)));
    
        D3D12CreateDevice(
            WarpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&mD3dDevice));
    }
    
    ThrowIfFailed(mD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    
    mDsvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mRtvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mCbvSrvUavDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS QualityLevels;
    QualityLevels.Format = mBackBufferFormat;
    QualityLevels.SampleCount = 4;
    QualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    QualityLevels.NumQualityLevels = 0;
    ThrowIfFailed(mD3dDevice->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &QualityLevels,
        sizeof(QualityLevels)));
    
    m4xMsaaQuality = QualityLevels.NumQualityLevels;
    assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level");
    
#ifdef _DEBUG
    LogAdapters();
#endif

    CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();
    
    return true;
}

void D3d12App::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(mD3dDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&mCommandQueue)));
   
    ThrowIfFailed(mD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));
    ThrowIfFailed(mD3dDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        mDirectCmdListAlloc.Get(),
        nullptr,
        IID_PPV_ARGS(mCommandList.GetAddressOf())));

    mCommandList->Close();
}

void D3d12App::CreateSwapChain()
{
    mSwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC SD;
    SD.BufferDesc.Width = mClientWidth;
    SD.BufferDesc.Height = mClientHeight;
    SD.BufferDesc.RefreshRate.Numerator = 60;
    SD.BufferDesc.RefreshRate.Denominator = 1;
    SD.BufferDesc.Format = mBackBufferFormat;
    SD.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    SD.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    SD.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    SD.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    SD.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SD.OutputWindow = mhMainWnd;
    SD.Windowed = true;
    SD.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SD.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ThrowIfFailed(mDxgiFactory->CreateSwapChain(mCommandQueue.Get(), &SD, mSwapChain.GetAddressOf()));
}

D3D12_CPU_DESCRIPTOR_HANDLE D3d12App::GetCurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3d12App::GetDepthStencilView() const
{
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3d12App::CalculateFrameStats()
{
    static int FrameCount = 0;
    static float TimeElapsed = 0.f;

    FrameCount++;

    if ((mTimer.TotalTime() - TimeElapsed) >= 1.f)
    {
        float Fps = (float)FrameCount;
        float MillisecondPerFrame = 1000.f / Fps;

        wstring FpsStr = to_wstring(Fps);
        wstring MsPerFrameStr = to_wstring(MillisecondPerFrame);

        wstring WindowText = mMainWndCaption + L"   fps: " + FpsStr + L"   ms pf: " + MsPerFrameStr;
        SetWindowText(mhMainWnd, WindowText.c_str());

        FrameCount = 0;
        TimeElapsed += 1.f;
    }
}

void D3d12App::LogAdapters()
{
    UINT i = 0;
    IDXGIAdapter* Adapter = nullptr;
    std::vector<IDXGIAdapter*> AdapterList;
    while (mDxgiFactory->EnumAdapters(i, &Adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC Desc;
        Adapter->GetDesc(&Desc);

        std::wstring Text = L"***Adapter: ";
        Text += Desc.Description;
        Text += L"\n";

        OutputDebugString(Text.c_str());

        AdapterList.push_back(Adapter);

        ++i;
    }

    for (size_t i = 0; i < AdapterList.size(); i++)
    {
        LogAdapterOutputs(AdapterList[i]);
        ReleaseCom(AdapterList[i]);
    }
}

void D3d12App::LogAdapterOutputs(IDXGIAdapter* Adapter)
{
}

void D3d12App::LogOutputDisplayModes(IDXGIOutput* Output, DXGI_FORMAT Format)
{
}
