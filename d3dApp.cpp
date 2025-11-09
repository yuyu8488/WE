#include "d3dApp.h"
#include <WindowsX.h>

D3DApp* D3DApp::mApp = nullptr;

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK MainWndProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    return D3DApp::GetApp()->MsgProc(WindowHandle, Message, WParam, LParam);
}


D3DApp::D3DApp(HINSTANCE hInstance) : mhAppInst(hInstance)
{
    assert(mApp == nullptr);
    mApp = this;
}

D3DApp::~D3DApp()
{
    if (md3dDevice != nullptr)
    {
        FlushCommandQueue();
    }
}

D3DApp* D3DApp::GetApp()
{
    return mApp;
}

HINSTANCE D3DApp::AppInstance() const
{
    return mhAppInst;
}

HWND D3DApp::MainWindow() const
{
    return mhMainWnd;
}

float D3DApp::AspectRatio() const
{
    return (float)(mClientWidth) / (float)(mClientHeight);
}

bool D3DApp::Get4xMsaaState() const
{
    return m4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool Value)
{
    if (m4xMsaaState != Value)
    {
        m4xMsaaState = Value;

        CreateSwapChain();
        OnResize();
    }
}

int D3DApp::Run()
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

bool D3DApp::Initialize()
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

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
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
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
    case WM_SIZE:
        mClientWidth = LOWORD(lParam);
        mClientHeight = HIWORD(lParam);
        if (md3dDevice)
        {
            if (wParam == SIZE_MINIMIZED)
            {
                mAppPaused = true;
                mMinimized = true;
                mMaximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                mAppPaused = false;
                mMinimized = false;
                mMaximized = true;
                OnResize();
            }
            else if (wParam == SIZE_RESTORED)
            {
                if (mMinimized)
                {
                    mAppPaused = false;
                    mMinimized = false;
                    OnResize();
                }
                else if (mMaximized)
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
        ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf()));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf()));
}

void D3DApp::OnResize()
{
    assert(md3dDevice);
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

        md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);

        rtvHeapHandle.Offset(1, mRtvDescriptorSize);
    }

    D3D12_RESOURCE_DESC DepthStencilDesc;
    DepthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    DepthStencilDesc.Alignment = 0;
    DepthStencilDesc.Width = mClientWidth;
    DepthStencilDesc.Height = mClientHeight;
    DepthStencilDesc.DepthOrArraySize = 1;
    DepthStencilDesc.MipLevels = 1;
    DepthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    DepthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    DepthStencilDesc.Format = mDepthStencilFormat;
    DepthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    DepthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.f;
    optClear.DepthStencil.Stencil = 0;
    
    CD3DX12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    md3dDevice->CreateCommittedResource(
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
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &DepthStencilViewDesc, DepthStencilView());

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

void D3DApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void D3DApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void D3DApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.2f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.2f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

bool D3DApp::InitMainWindow()
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

void D3DApp::FlushCommandQueue()
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

ID3D12Resource* D3DApp::CurrentBackBuffer() const
{
    return mSwapChainBuffer[mCurrBackBuffer].Get();
}

bool D3DApp::InitDirect3D()
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
        IID_PPV_ARGS(&md3dDevice));

    if (FAILED(ResultHandle))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> WarpAdapter;
        ThrowIfFailed(mDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&WarpAdapter)));
    
        D3D12CreateDevice(
            WarpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&md3dDevice));
    }
    
    ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    
    mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS QualityLevels;
    QualityLevels.Format = mBackBufferFormat;
    QualityLevels.SampleCount = 4;
    QualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    QualityLevels.NumQualityLevels = 0;
    ThrowIfFailed(md3dDevice->CheckFeatureSupport(
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

void D3DApp::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(md3dDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&mCommandQueue)));
   
    ThrowIfFailed(md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));
    ThrowIfFailed(md3dDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        mDirectCmdListAlloc.Get(),
        nullptr,
        IID_PPV_ARGS(mCommandList.GetAddressOf())));

    mCommandList->Close();
}

void D3DApp::CreateSwapChain()
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
    SD.BufferCount = 2;
    SD.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SD.OutputWindow = mhMainWnd;
    SD.Windowed = true;
    SD.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SD.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ThrowIfFailed(mDxgiFactory->CreateSwapChain(mCommandQueue.Get(), &SD, mSwapChain.GetAddressOf()));
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::CalculateFrameStats()
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

void D3DApp::LogAdapters()
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

void D3DApp::LogAdapterOutputs(IDXGIAdapter* Adapter)
{
}

void D3DApp::LogOutputDisplayModes(IDXGIOutput* Output, DXGI_FORMAT Format)
{
}
