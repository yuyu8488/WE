#pragma once

#include "d3dApp.h"
#include "d3dx12.h"
#include <WindowsX.h>
#include "GeometryGenerator.h"
#include "Material.h"

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

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildRootSignature();
    BuildShaderAndInputLayout();
    BuildShapeGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

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

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);

    mScreenViewport.TopLeftX = 0;
    mScreenViewport.TopLeftY = 0;
    mScreenViewport.Width = static_cast<float>(mClientWidth);
    mScreenViewport.Height = static_cast<float>(mClientHeight);
    mScreenViewport.MinDepth = 0.f;
    mScreenViewport.MaxDepth = 1.f;

    mScissorRect = { 0,0,mClientWidth,mClientHeight };
}

void D3DApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
    UpdateCamera(gt);

    CurrentFrameResourceIndex = (CurrentFrameResourceIndex + 1) % NUM_FRAME_RESOURCES;
    CurrentFrameResource = FrameResources[CurrentFrameResourceIndex].get();

    if (CurrentFrameResource->Fence != 0 && mFence->GetCompletedValue() < CurrentFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(CurrentFrameResource->Fence, eventHandle));
        if (eventHandle)
        {
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }

    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateMainPassCBs(gt);
}

void D3DApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = CurrentFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());

    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), PSOs["Render"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(),
        DirectX::Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

    auto BackBufferView = CurrentBackBufferView();
    auto DSV = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &BackBufferView, true, &DSV);

    mCommandList->SetGraphicsRootSignature(RootSignature.Get());

    auto PassCB = CurrentFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(2, PassCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), RenderItems);

    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier2);

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));

    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
    CurrentFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);

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

void D3DApp::UpdateCamera(const GameTimer& gt)
{
    mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
    mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
    mEyePos.y = mRadius * cosf(mPhi);

    XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);
}

void D3DApp::OnKeyboardInput(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        mSunTheta -= 1.0f * dt;

    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        mSunTheta += 1.0f * dt;

    if (GetAsyncKeyState(VK_UP) & 0x8000)
        mSunPhi -= 1.0f * dt;

    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        mSunPhi += 1.0f * dt;

    mSunPhi = MathHelper::Clamp(mSunPhi, 0.1f, XM_PIDIV2);
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
    
        ThrowIfFailed(D3D12CreateDevice(
            WarpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&md3dDevice)));
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

void D3DApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto CurrentObjectCB = CurrentFrameResource->ObjectCB.get();
    for (auto& R : AllRenderItems)
    {
        if (R->NumFramesDirty > 0)
        {
            XMMATRIX World = XMLoadFloat4x4(&R->World);

            ObjectConstants ObjConstants;
            XMStoreFloat4x4(&ObjConstants.World, XMMatrixTranspose(World));

            CurrentObjectCB->CopyData(R->ObjectCBIndex, ObjConstants);

            R->NumFramesDirty--;
        }
    }
}

void D3DApp::UpdateMainPassCBs(const GameTimer& gt)
{
    XMMATRIX View = XMLoadFloat4x4(&mView);
    XMMATRIX Proj = XMLoadFloat4x4(&mProj);
    XMMATRIX ViewProj = XMMatrixMultiply(View, Proj);

    XMVECTOR DetView = XMMatrixDeterminant(View);
    XMVECTOR DetProj = XMMatrixDeterminant(Proj);
    XMVECTOR DetViewProj = XMMatrixDeterminant(ViewProj);

    XMMATRIX InvView = XMMatrixInverse(&DetView, View);
    XMMATRIX InvProj = XMMatrixInverse(&DetProj, Proj);
    XMMATRIX InvViewProj = XMMatrixInverse(&DetViewProj, ViewProj);

    XMStoreFloat4x4(&MainPassCB.View, XMMatrixTranspose(View));
    XMStoreFloat4x4(&MainPassCB.Proj, XMMatrixTranspose(Proj));
    XMStoreFloat4x4(&MainPassCB.ViewProj, XMMatrixTranspose(ViewProj));
    XMStoreFloat4x4(&MainPassCB.InvView, XMMatrixTranspose(InvView));
    XMStoreFloat4x4(&MainPassCB.InvProj, XMMatrixTranspose(InvProj));
    XMStoreFloat4x4(&MainPassCB.InvViewProj, XMMatrixTranspose(InvViewProj));

    MainPassCB.EyePosW = mEyePos;
    MainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    MainPassCB.InvRenderTargetSize = XMFLOAT2(1.f / (float)mClientWidth, 1.f / (float)mClientHeight);
    MainPassCB.NearZ = 1.f;
    MainPassCB.FarZ = 1000.f;
    MainPassCB.DeltaTime = gt.DeltaTime();
    MainPassCB.TotalTime = gt.TotalTime();
    MainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.f };
    MainPassCB.cbPerObjectPad1;

    XMVECTOR LightDir = MathHelper::SphericalToCartesian(1.f, mSunTheta, mSunPhi);

    XMStoreFloat3(&MainPassCB.Lights[0].Direction, LightDir);
    MainPassCB.Lights[0].Strength = { 1.f, 1.f, 1.f };

    auto CurrentPassCB = CurrentFrameResource->PassCB.get();
    CurrentPassCB->CopyData(0, MainPassCB);
}

void D3DApp::UpdateMaterialCBs(const GameTimer& gt)
{
    auto CurrentMaterialCB = CurrentFrameResource->MaterialCB.get();

    for (auto& e : Materials)
    {
        Material* Mat = e.second.get();
        if (Mat->NumFramesDirty > 0)
        {
            XMMATRIX MaterialTransform = XMLoadFloat4x4(&Mat->MatTransform);

            MaterialConstants MatConstants;
            MatConstants.DiffuseAlbedo = Mat->DiffuseAlbedo;
            MatConstants.FresnelR0 = Mat->FresnelR0;
            MatConstants.Roughness = Mat->Roughness;

            CurrentMaterialCB->CopyData(Mat->MatCBIndex, MatConstants);
            
            Mat->NumFramesDirty--;
        }
    }
}

void D3DApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& RenderItems)
{
    UINT ObjCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT MatCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto ObjectCB = CurrentFrameResource->ObjectCB->Resource();
    auto MatCB = CurrentFrameResource->MaterialCB->Resource();

    for (size_t i = 0; i < RenderItems.size(); ++i)
    {
        auto RenderItem = RenderItems[i];

        cmdList->IASetVertexBuffers(0, 1, &RenderItem->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&RenderItem->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(RenderItem->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS ObjCBAddress = ObjectCB->GetGPUVirtualAddress() + RenderItem->ObjectCBIndex * ObjCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS MatCBAddress = MatCB->GetGPUVirtualAddress() + RenderItem->Mat->MatCBIndex * MatCBByteSize;
   

        cmdList->SetGraphicsRootConstantBufferView(0, ObjCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(1, MatCBAddress);
        cmdList->DrawIndexedInstanced(RenderItem->IndexCount, 1, RenderItem->StartIndexLocation, RenderItem->BaseVertexLocation, 0);
    }
}

void D3DApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER RootParameter[3];

    RootParameter[0].InitAsConstantBufferView(0);
    RootParameter[1].InitAsConstantBufferView(1);
    RootParameter[2].InitAsConstantBufferView(2);

    CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc(3, RootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> SerializedRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> Error = nullptr;	
    HRESULT hr = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        SerializedRootSignature.GetAddressOf(), Error.GetAddressOf());

    if (Error != nullptr)
    {
        OutputDebugStringA((const char*)Error->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        SerializedRootSignature->GetBufferPointer(),
        SerializedRootSignature->GetBufferSize(),
        IID_PPV_ARGS(RootSignature.GetAddressOf())));
}

void D3DApp::BuildShaderAndInputLayout()
{
    Shaders["VS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_0");
    Shaders["PS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_0");

    InputLayout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
}

void D3DApp::BuildShapeGeometry()
{
    GeometryGenerator GeoGen;
    
    GeometryGenerator::MeshData Box = GeoGen.CreateBox(5.5f, 5.5f, 5.5f, 3);
    UINT BoxVertexOffset = 0;
    UINT BoxIndexOffset = 0;

    SubmeshGeometry BoxSubMesh;
    BoxSubMesh.IndexCount = (UINT)Box.Indices32.size();
    BoxSubMesh.StartIndexLocation = BoxIndexOffset;
    BoxSubMesh.BaseVertexLocation = BoxVertexOffset;

    auto TotalVertexCount = Box.Vertices.size();
    std::vector<Vertex> Vertices(TotalVertexCount);
    
    UINT k = 0;
    for (size_t i = 0; i < Box.Vertices.size(); i++, ++k)
    {
        Vertices[i].Pos = Box.Vertices[i].Position;
        Vertices[i].Normal = Box.Vertices[i].Normal;
    }

    std::vector<std::uint16_t> Indices;
    Indices.insert(Indices.end(), std::begin(Box.GetIndices16()), std::end(Box.GetIndices16()));

    const UINT VertexBufferByteSize = (UINT)Vertices.size() * sizeof(Vertex);
    const UINT IndexBufferByteSize = (UINT)Indices.size() * sizeof(std::uint16_t);

    auto Geo = make_unique<MeshGeometry>();
    Geo->Name = "ShapeGeo";

    ThrowIfFailed(D3DCreateBlob(VertexBufferByteSize, &Geo->VertexBufferCPU));
    CopyMemory(Geo->VertexBufferCPU->GetBufferPointer(), Vertices.data(), VertexBufferByteSize);

    ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &Geo->IndexBufferCPU));
    CopyMemory(Geo->IndexBufferCPU->GetBufferPointer(), Indices.data(), IndexBufferByteSize);

    Geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(),
        mCommandList.Get(),
        Vertices.data(),
        VertexBufferByteSize,
        Geo->VertexBufferUploader);

    Geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(),
        mCommandList.Get(),
        Indices.data(),
        IndexBufferByteSize,
        Geo->IndexBufferUploader);

    Geo->VertexByteStride = sizeof(Vertex);
    Geo->VertexBufferByteSize = VertexBufferByteSize;
    Geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    Geo->IndexBufferByteSize = IndexBufferByteSize;

    Geo->DrawArgs["Box"] = BoxSubMesh;

    Geometries[Geo->Name] = std::move(Geo);
}

void D3DApp::BuildMaterials()
{
    auto RedMaterial = std::make_unique<Material>();
    RedMaterial->Name = "M_Red";
    RedMaterial->MatCBIndex = 0;
    RedMaterial->DiffuseAlbedo = XMFLOAT4(1.f, 0.f, 0.f, 1.f);
    RedMaterial->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
    RedMaterial->Roughness = 0.325f;

    Materials[RedMaterial->Name] = std::move(RedMaterial);
}

void D3DApp::BuildRenderItems()
{
    auto BoxRenderItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&BoxRenderItem->World, XMMatrixScaling(3.f, 3.f, 3.f) * XMMatrixTranslation(0.f, 0.f, 0.f));
    BoxRenderItem->ObjectCBIndex = 0;
    BoxRenderItem->Geo = Geometries["ShapeGeo"].get();
    BoxRenderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    BoxRenderItem->IndexCount = BoxRenderItem->Geo->DrawArgs["Box"].IndexCount;
    BoxRenderItem->StartIndexLocation = BoxRenderItem->Geo->DrawArgs["Box"].StartIndexLocation;
    BoxRenderItem->BaseVertexLocation = BoxRenderItem->Geo->DrawArgs["Box"].BaseVertexLocation;
    BoxRenderItem->Mat = Materials["M_Red"].get();
    AllRenderItems.push_back(std::move(BoxRenderItem));

    for (auto& e : AllRenderItems)
    {
        RenderItems.push_back(e.get());
    }
}

void D3DApp::BuildFrameResources()
{
    for (int i = 0; i < NUM_FRAME_RESOURCES; ++i)
    {
        FrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)AllRenderItems.size(), (UINT)Materials.size(), 0));
    }
}

void D3DApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc;
    ZeroMemory(&PsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    PsoDesc.pRootSignature = RootSignature.Get();
    PsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(Shaders["VS"]->GetBufferPointer()),
        Shaders["VS"]->GetBufferSize()
    };
    PsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(Shaders["PS"]->GetBufferPointer()),
        Shaders["PS"]->GetBufferSize()
    };
    PsoDesc.DS;
    PsoDesc.HS;
    PsoDesc.GS;
    PsoDesc.StreamOutput = D3D12_STREAM_OUTPUT_DESC();
    PsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    PsoDesc.SampleMask = UINT_MAX;
    PsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    PsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    PsoDesc.InputLayout = { InputLayout.data(), (UINT)InputLayout.size() };
    PsoDesc.IBStripCutValue;
    PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PsoDesc.NumRenderTargets = 1;
    PsoDesc.RTVFormats[0] = mBackBufferFormat;
    PsoDesc.DSVFormat = mDepthStencilFormat;
    PsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    PsoDesc.SampleDesc.Quality = m4xMsaaState ? m4xMsaaQuality - 1 : 0;
    PsoDesc.NodeMask;
    PsoDesc.CachedPSO;
    PsoDesc.Flags;
   
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&PSOs["Render"])));
}
