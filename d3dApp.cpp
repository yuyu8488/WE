#pragma once

#include <WindowsX.h>
#include "d3dApp.h"
#include "d3dx12.h"
#include "GeometryGenerator.h"
#include "Material.h"
#include "UploadBuffer.h"
#include "TextureManager.h"

#include "DDSTextureLoader12.h"

D3D12* D3D12::mApp = nullptr;

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK MainWndProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    return D3D12::GetApp()->MsgProc(WindowHandle, Message, WParam, LParam);
}

D3D12::D3D12(HINSTANCE hInstance) : mhAppInst(hInstance)
{
    assert(mApp == nullptr);
    mApp = this;
}

D3D12::~D3D12()
{
    if (md3dDevice != nullptr)
    {
        FlushCommandQueue();
    }
}
 
D3D12* D3D12::GetApp()
{
    return mApp;
}

HINSTANCE D3D12::AppInstance() const
{
    return mhAppInst;
}

HWND D3D12::MainWindow() const
{
    return mhMainWnd;
}

float D3D12::AspectRatio() const
{
    return (float)(mClientWidth) / (float)(mClientHeight);
}

bool D3D12::Get4xMsaaState() const
{
    return m4xMsaaState;
}

void D3D12::Set4xMsaaState(bool Value)
{
    if (m4xMsaaState != Value)
    {
        m4xMsaaState = Value;

        CreateSwapChain();
        OnResize();
    }
}

int D3D12::Run()
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

bool D3D12::Initialize()
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

    TextureManager = std::make_unique<FTextureManager>(md3dDevice.Get(), mCommandList.Get());
    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShaderAndInputLayout();
    BuildGeometries();
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

LRESULT D3D12::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

void D3D12::CreateRtvAndDsvDescriptorHeaps()
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

void D3D12::OnResize()
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

        rtvHeapHandle.Offset(1, RtvDescriptorSize);
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

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);

    mScreenViewport.TopLeftX = 0;
    mScreenViewport.TopLeftY = 0;
    mScreenViewport.Width = static_cast<float>(mClientWidth);
    mScreenViewport.Height = static_cast<float>(mClientHeight);
    mScreenViewport.MinDepth = 0.f;
    mScreenViewport.MaxDepth = 1.f;

    mScissorRect = { 0,0,mClientWidth,mClientHeight };
}

void D3D12::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
    UpdateCamera(gt);

    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % NUM_FRAME_RESOURCES;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        if (eventHandle)
        {
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }

    //AnimateMaterials(gt); 
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateMainPassCBs(gt);
    //UpdateWaves(gt);
}

void D3D12::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    
    // 커맨드 기록에 사용된 메모리 재사용.
    // GPU가 해당 커맨드 리스트 실행을 끝난 뒤에만 Reset 가능.
    ThrowIfFailed(cmdListAlloc->Reset());

    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear back buffer, depth buffer
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

    // Specify buffers to render.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    // 셰이더에서 사용할 Descriptor Heap 바인딩(GPU리소스를 셰이더가 접근할 수 있도록 연결)
    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    // Draw opaque items
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    /* TODO: Add Others ...*/

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Close recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add command list to queue
    ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // Swap back and front buffers.
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void D3D12::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void D3D12::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void D3D12::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 1.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void D3D12::UpdateCamera(const GameTimer& gt)
{
    mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
    mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
    mEyePos.y = mRadius * cosf(mPhi);

    XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);
}

void D3D12::OnKeyboardInput(const GameTimer& gt)
{

}

bool D3D12::InitMainWindow()
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

void D3D12::FlushCommandQueue()
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

ID3D12Resource* D3D12::CurrentBackBuffer() const
{
    return mSwapChainBuffer[mCurrBackBuffer].Get();
}

bool D3D12::InitDirect3D()
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
    
    DsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    RtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
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

void D3D12::CreateCommandObjects()
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

void D3D12::CreateSwapChain()
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

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, RtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::DepthStencilView() const
{
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3D12::CalculateFrameStats()
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

void D3D12::LogAdapters()
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

void D3D12::LogAdapterOutputs(IDXGIAdapter* Adapter)
{
}

void D3D12::LogOutputDisplayModes(IDXGIOutput* Output, DXGI_FORMAT Format)
{
}

void D3D12::AnimateMaterials(const GameTimer& gt)
{
    // Scroll water material texture coordinates.
    auto WaterMat = mMaterials["Water"].get();

    if (WaterMat)
    {
        // 행렬의 translation 부분을 가져옴.
        float& tu = WaterMat->MatTransform(3, 0);
        float& tv = WaterMat->MatTransform(3, 1);

        tu += 0.1f * gt.DeltaTime();
        tv += 0.02f * gt.DeltaTime();

        if (tu >= 1.f)
        {
            tu -= 1.f;
        }
        if (tv >= 1.f)
        {
            tv -= 1.f;
        }

        WaterMat->MatTransform(3, 0) = tu;
        WaterMat->MatTransform(3, 1) = tv;

        // Material changed so need to update cbuffer;
        WaterMat->NumFramesDirty = NUM_FRAME_RESOURCES;
    }
}

void D3D12::UpdateMainPassCBs(const GameTimer& gt)
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

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(View));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(Proj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(ViewProj));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(InvView));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(InvProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(InvViewProj));
    mMainPassCB.EyePosW = mEyePos;
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.f / (float)mClientWidth, 1.f / (float)mClientHeight);
    mMainPassCB.NearZ = 1.f;
    mMainPassCB.FarZ = 1000.f;
    mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.f };
    mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.8f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

    auto CurrentPassCB = mCurrFrameResource->PassCB.get();
    CurrentPassCB->CopyData(0, mMainPassCB);
}

void D3D12::UpdateObjectCBs(const GameTimer& gt)
{
    auto CurrentObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& R : mAllRitems)
    {
        if (R->NumFramesDirty > 0)
        {
            XMMATRIX World = XMLoadFloat4x4(&R->World);
            XMMATRIX TexTransform = XMLoadFloat4x4(&R->TexTransform);

            ObjectConstants ObjConstants;
            XMStoreFloat4x4(&ObjConstants.World, XMMatrixTranspose(World));
            XMStoreFloat4x4(&ObjConstants.TexTransform, XMMatrixTranspose(TexTransform));

            CurrentObjectCB->CopyData(R->ObjectCBIndex, ObjConstants);

            R->NumFramesDirty--;
        }
    }
}

void D3D12::UpdateMaterialCBs(const GameTimer& gt)
{
    auto CurrentMaterialCB = mCurrFrameResource->MaterialCB.get();

    for (auto& e : mMaterials)
    {
        Material* Mat = e.second.get();
        if (Mat && Mat->NumFramesDirty > 0)
        {
            XMMATRIX MaterialTransform = XMLoadFloat4x4(&Mat->MatTransform);

            MaterialConstants MatConstants;
            MatConstants.DiffuseAlbedo = Mat->DiffuseAlbedo;
            MatConstants.FresnelR0 = Mat->FresnelR0;
            MatConstants.Roughness = Mat->Roughness;
            XMStoreFloat4x4(&MatConstants.MatTransform, XMMatrixTranspose(MaterialTransform));

            CurrentMaterialCB->CopyData(Mat->MatCBIndex, MatConstants);

            Mat->NumFramesDirty--;
        }
    }
}

void D3D12::UpdateWaves(const GameTimer& gt)
{
    // Every quarter second, generate random wave.
    static float t_base = 0.0f;
    if ((mTimer.TotalTime() - t_base) >= 0.25f)
    {
        t_base += 0.25f;

        int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
        int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

        float r = MathHelper::RandF(0.2f, 0.5f);

        mWaves->Disturb(i, j, r);
    }

    mWaves->Update(gt.DeltaTime());

    // Update the wave vertex buffer with the new solution.
    if (auto CurrWavesVB = mCurrFrameResource->WavesVB.get())
    {
        for (int i = 0; i < mWaves->VertexCount(); ++i)
        {
            Vertex v;
            v.Pos = mWaves->Position(i);
            v.Normal = mWaves->Normal(i);

            // 정점의 위치를 이용해서 텍스처 조표를 계산
            // mapping [-w/2,w/2] --> [0,1]
            v.TexC.x = 0.5f + (v.Pos.x / mWaves->Width());
            v.TexC.y = 0.5f + (v.Pos.z / mWaves->Depth());

            CurrWavesVB->CopyData(i, v);
        }
        // Set dynamic VB of Wave renderItem to current frame VB.
        WavesRenderItem->Geo->VertexBufferGPU = CurrWavesVB->Resource();
    }
}

void D3D12::UpdateReflectedPassCB(const GameTimer& gt)
{

}

void D3D12::LoadTextures()
{
    TextureManager->LoadTexture("bricksTex", L"Textures/bricks3.dds");
    TextureManager->LoadTexture("checkboardTex", L"Textures/checkboard.dds");
    TextureManager->LoadTexture("iceTex", L"Textures/ice.dds");
    TextureManager->LoadTexture("white1x1Tex", L"Textures/white1x1.dds");
}

void D3D12::BuildRootSignature()
{
    // CD3DX12_DESCRIPTOR_RANGE: Root Signature 정의 헬퍼 클래스.
    // Descripotr Range를 지정해서 셰이더가 접근할수 있는 리소스 집합을 정의.
    // 셰이더 코드에서 Texture2D Tex: register(t0) 슬롯에 있는 텍스처 리소스를 읽을 수 있도록 설정.
    CD3DX12_DESCRIPTOR_RANGE TextureTable;
    TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    
    CD3DX12_ROOT_PARAMETER SlotRootParameter[4];

    // Perfomance TIP: Order from most frequent to least frequent.
    SlotRootParameter[0].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL); 
    SlotRootParameter[1].InitAsConstantBufferView(0);
    SlotRootParameter[2].InitAsConstantBufferView(1);
    SlotRootParameter[3].InitAsConstantBufferView(2);

    auto StaticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC RootSigDesc(4, SlotRootParameter,
        (UINT)StaticSamplers.size(), StaticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> SerializedRootSig = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&RootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        SerializedRootSig.GetAddressOf(), ErrorBlob.GetAddressOf());

    if (ErrorBlob != nullptr)
    {
        OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0, SerializedRootSig->GetBufferPointer(), SerializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void D3D12::BuildDescriptorHeaps()
{
    // Create SRV heap.
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 4;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    // Fill out heap with actual descriptors.
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    auto bricksTex = TextureManager->GetTexture("bricksTex")->Resource;
    auto checkboardTex = TextureManager->GetTexture("checkboardTex")->Resource;
    auto iceTex = TextureManager->GetTexture("iceTex")->Resource;
    auto white1x1Tex = TextureManager->GetTexture("white1x1Tex")->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; //SRV가 셰이더에서 RGBA 채널을 기본 방식으로 읽도록 설정.
    srvDesc.Format = bricksTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = -1;
    md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);
    
    /*next descriptor*/
    hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

    srvDesc.Format = checkboardTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(checkboardTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

    srvDesc.Format = iceTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, CbvSrvUavDescriptorSize);
    
    srvDesc.Format = white1x1Tex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(white1x1Tex.Get(), &srvDesc, hDescriptor);
}

void D3D12::BuildShaderAndInputLayout()
{
    const D3D_SHADER_MACRO defines[] =
    {
        "FOG", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "FOG", "1",
        "ALPHA_TEST", "1",
        NULL, NULL,
    };
    
    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_0");
    //mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", alphaTestDefines, "PS", "ps_5_0");

    mInputLayout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
}

void D3D12::BuildGeometries()
{
    BuildSkullGeometry();
}

void D3D12::BuildPSOs()
{
    /*typedef struct D3D12_GRAPHICS_PIPELINE_STATE_DESC
    {
    ID3D12RootSignature *pRootSignature;
    D3D12_SHADER_BYTECODE VS;
    D3D12_SHADER_BYTECODE PS;
    D3D12_SHADER_BYTECODE DS;
    D3D12_SHADER_BYTECODE HS;
    D3D12_SHADER_BYTECODE GS;
    D3D12_STREAM_OUTPUT_DESC StreamOutput;
    D3D12_BLEND_DESC BlendState;
    UINT SampleMask;
    D3D12_RASTERIZER_DESC RasterizerState;
    D3D12_DEPTH_STENCIL_DESC DepthStencilState;
    D3D12_INPUT_LAYOUT_DESC InputLayout;
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
    UINT NumRenderTargets;
    DXGI_FORMAT RTVFormats[ 8 ];
    DXGI_FORMAT DSVFormat;
    DXGI_SAMPLE_DESC SampleDesc;
    UINT NodeMask;
    D3D12_CACHED_PIPELINE_STATE CachedPSO;
    D3D12_PIPELINE_STATE_FLAGS Flags;
    } 	D3D12_GRAPHICS_PIPELINE_STATE_DESC;
    */

    // PSO for opaque objects.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    //memset 
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();

    /*typedef struct D3D12_SHADER_BYTECODE
    {
        _Field_size_bytes_full_(BytecodeLength)  const void* pShaderBytecode;
        SIZE_T BytecodeLength;
    } 	D3D12_SHADER_BYTECODE;
    */
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));
}

void D3D12::BuildFrameResources()
{
    for (int i = 0; i < NUM_FRAME_RESOURCES; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 2, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), 0));
    }
}

void D3D12::BuildMaterials()
{
    auto bricks = std::make_unique<Material>();
    bricks->Name = "bricks";
    bricks->MatCBIndex = 0;
    bricks->DiffuseSrvHeapIndex = 0;
    bricks->DiffuseAlbedo = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
    bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    bricks->Roughness = 0.25f;

    auto checkertile = std::make_unique<Material>();
    checkertile->Name = "checkertile";
    checkertile->MatCBIndex = 1;
    checkertile->DiffuseSrvHeapIndex = 1;
    checkertile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    checkertile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
    checkertile->Roughness = 0.3f;

    auto icemirror = std::make_unique<Material>();
    icemirror->Name = "icemirror";
    icemirror->MatCBIndex = 2;
    icemirror->DiffuseSrvHeapIndex = 2;
    icemirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
    icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    icemirror->Roughness = 0.5f;

    auto skullMat = std::make_unique<Material>();
    skullMat->Name = "skullMat";
    skullMat->MatCBIndex = 3;
    skullMat->DiffuseSrvHeapIndex = 3;
    skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    skullMat->Roughness = 0.3f;

    auto shadowMat = std::make_unique<Material>();
    shadowMat->Name = "shadowMat";
    shadowMat->MatCBIndex = 4;
    shadowMat->DiffuseSrvHeapIndex = 3;
    shadowMat->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
    shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
    shadowMat->Roughness = 0.0f;

    mMaterials["bricks"] = std::move(bricks);
    mMaterials["checkertile"] = std::move(checkertile);
    mMaterials["icemirror"] = std::move(icemirror);
    mMaterials["skullMat"] = std::move(skullMat);
    mMaterials["shadowMat"] = std::move(shadowMat);
}

void D3D12::BuildRenderItems()
{
    auto skullRitem = std::make_unique<RenderItem>();
    skullRitem->World = MathHelper::Identity4x4();
    skullRitem->TexTransform = MathHelper::Identity4x4();
    skullRitem->ObjectCBIndex = 0;
    skullRitem->Mat = mMaterials["skullMat"].get();
    skullRitem->Geo = mGeometries["skullGeo"].get();
    skullRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
    skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
    skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
    mSkullRitem = skullRitem.get();
    mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());

    mAllRitems.push_back(std::move(skullRitem));   
}

void D3D12::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& RenderItems)
{
    UINT ObjCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT MatCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto ObjectCB = mCurrFrameResource->ObjectCB->Resource();
    auto MatCB = mCurrFrameResource->MaterialCB->Resource();

    for (size_t i = 0; i < RenderItems.size(); ++i)
    {
        auto RenderItem = RenderItems[i];

        cmdList->IASetVertexBuffers(0, 1, &RenderItem->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&RenderItem->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(RenderItem->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE Texture(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        Texture.Offset(RenderItem->Mat->DiffuseSrvHeapIndex, CbvSrvUavDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS ObjCBAddress = ObjectCB->GetGPUVirtualAddress() + RenderItem->ObjectCBIndex * ObjCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS MatCBAddress = MatCB->GetGPUVirtualAddress() + RenderItem->Mat->MatCBIndex * MatCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0, Texture);
        cmdList->SetGraphicsRootConstantBufferView(1, ObjCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, MatCBAddress);

        cmdList->DrawIndexedInstanced(RenderItem->IndexCount, 1, RenderItem->StartIndexLocation, RenderItem->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> D3D12::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC PointWrap(
        0,
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    const CD3DX12_STATIC_SAMPLER_DESC PointClamp(
        1,
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    const CD3DX12_STATIC_SAMPLER_DESC LinearWrap(
        2,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    const CD3DX12_STATIC_SAMPLER_DESC LinearClamp(
        3,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    const CD3DX12_STATIC_SAMPLER_DESC AnisotropicWrap(
        4,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.f,
        8);

    const CD3DX12_STATIC_SAMPLER_DESC AnisotropicClamp(
        5,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.f,
        8);

    return { PointWrap, PointClamp, LinearWrap, LinearClamp, AnisotropicWrap, AnisotropicClamp };
}

void D3D12::BuildLandGeometry()
{
    GeometryGenerator GeoGen;
    GeometryGenerator::MeshData Grid = GeoGen.CreateGrid(200, 200, 10, 10);

    std::vector<Vertex> Vertices(Grid.Vertices.size());
    for (size_t i = 0; i < Grid.Vertices.size(); ++i)
    {
        auto& P = Grid.Vertices[i].Position;
        Vertices[i].Pos = P;
        Vertices[i].Pos.y = GeometryGenerator::GetHillsHeight(P.x, P.z);
        Vertices[i].Normal = GeometryGenerator::GetHillsNormal(P.x, P.z);
        Vertices[i].TexC = Grid.Vertices[i].TexC;
    }

    const UINT VbByteSize = (UINT)Vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> Indices = Grid.GetIndices16();
    const UINT IbByteSize = (UINT)Indices.size() * sizeof(std::uint16_t);

    auto Geo = std::make_unique<MeshGeometry>();
    Geo->Name = "LandGeo";

    ThrowIfFailed(D3DCreateBlob(VbByteSize, &Geo->VertexBufferCPU));
    CopyMemory(Geo->VertexBufferCPU->GetBufferPointer(), Vertices.data(), VbByteSize);

    ThrowIfFailed(D3DCreateBlob(IbByteSize, &Geo->IndexBufferCPU));
    CopyMemory(Geo->IndexBufferCPU->GetBufferPointer(), Indices.data(), IbByteSize);

    Geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), Vertices.data(), VbByteSize, Geo->VertexBufferUploader);

    Geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), Indices.data(), IbByteSize, Geo->IndexBufferUploader);

    Geo->VertexByteStride = sizeof(Vertex);
    Geo->VertexBufferByteSize = VbByteSize;
    Geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    Geo->IndexBufferByteSize = IbByteSize;

    SubmeshGeometry LandSubMesh;
    LandSubMesh.IndexCount = (UINT)Indices.size();
    LandSubMesh.StartIndexLocation = 0;
    LandSubMesh.BaseVertexLocation = 0;
    Geo->DrawArgs["Grid"] = LandSubMesh;
    mGeometries["LandGeo"] = std::move(Geo);
}

void D3D12::BuildBoxGeometry()
{
    GeometryGenerator GeoGen;
    GeometryGenerator::MeshData Box = GeoGen.CreateBox(8.0f, 8.f, 8.f, 3);

    std::vector<Vertex> Vertices(Box.Vertices.size());
    for (size_t i = 0; i < Box.Vertices.size(); ++i)
    {
        auto& P = Box.Vertices[i].Position;
        Vertices[i].Pos = P;
        Vertices[i].Normal = Box.Vertices[i].Normal;
        Vertices[i].TexC = Box.Vertices[i].TexC;
    }

    const UINT VbByteSize = (UINT)Vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> Indices = Box.GetIndices16();
    const UINT IbByteSize = (UINT)Indices.size() * sizeof(uint16_t);

    auto BoxGeo = std::make_unique<MeshGeometry>();
    BoxGeo->Name = "BoxGeo";

    ThrowIfFailed(D3DCreateBlob(VbByteSize, &BoxGeo->VertexBufferCPU));
    CopyMemory(BoxGeo->VertexBufferCPU->GetBufferPointer(), Vertices.data(), VbByteSize);

    ThrowIfFailed(D3DCreateBlob(IbByteSize, &BoxGeo->IndexBufferCPU));
    CopyMemory(BoxGeo->IndexBufferCPU->GetBufferPointer(), Indices.data(), IbByteSize);

    BoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), Vertices.data(), VbByteSize, BoxGeo->VertexBufferUploader);

    BoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), Indices.data(), IbByteSize, BoxGeo->IndexBufferUploader);

    BoxGeo->VertexByteStride = sizeof(Vertex);
    BoxGeo->VertexBufferByteSize = VbByteSize;
    BoxGeo->IndexBufferByteSize = IbByteSize;
    BoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;

    SubmeshGeometry BoxSubMesh;
    BoxSubMesh.IndexCount = (UINT)Indices.size();
    BoxSubMesh.StartIndexLocation = 0;
    BoxSubMesh.BaseVertexLocation = 0;

    BoxGeo->DrawArgs["Box"] = BoxSubMesh;
    mGeometries["BoxGeo"] = std::move(BoxGeo);
}

void D3D12::BuildWaveGeometry()
{
    // 3 indices per face
    std::vector<std::uint16_t> Indices(3 * mWaves->TriangleCount()); 
    assert(mWaves->VertexCount() < 0x0000ffff);

    int M = mWaves->RowCount();
    int N = mWaves->ColumnCount();
    int K = 0;
    for (int i = 0; i < M - 1; ++i)
    {
        for (int j = 0; j < N - 1; ++j)
        {
            Indices[K] = i * N + j;
            Indices[K + 1] = i * N + j + 1;
            Indices[K + 2] = (i + 1) * N + j;

            Indices[K + 3] = (i + 1) * N + j;
            Indices[K + 4] = i * N + j + 1;
            Indices[K + 5] = (i + 1) * N + j + 1;
            
            K += 6; // Next quad
        }
    }

    // Set dynamically
    UINT VbByteSize = mWaves->VertexCount() * sizeof(Vertex);
    UINT IbByteSize = (UINT)Indices.size() * sizeof(std::uint16_t);

    auto Geo = std::make_unique<MeshGeometry>();
    Geo->Name = "WaterGeo";

    Geo->VertexBufferCPU = nullptr;
    Geo->VertexBufferGPU = nullptr;

    ThrowIfFailed(D3DCreateBlob(IbByteSize, &Geo->IndexBufferCPU));
    CopyMemory(Geo->IndexBufferCPU->GetBufferPointer(), Indices.data(), IbByteSize);

    Geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), Indices.data(), IbByteSize, Geo->IndexBufferUploader);

    Geo->VertexByteStride = sizeof(Vertex);
    Geo->VertexBufferByteSize = VbByteSize;
    Geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    Geo->IndexBufferByteSize = IbByteSize;

    SubmeshGeometry SubMesh;
    SubMesh.IndexCount = (UINT)Indices.size();
    SubMesh.StartIndexLocation = 0;
    SubMesh.BaseVertexLocation = 0;

    Geo->DrawArgs["Grid"] = SubMesh;
    mGeometries["WaterGeo"] = std::move(Geo);
}

void D3D12::BuildRoomGeometry()
{
    // Create and specify geometry.  For this sample we draw a floor
// and a wall with a mirror on it.  We put the floor, wall, and
// mirror geometry in one vertex buffer.
//
//   |--------------|
//   |              |
//   |----|----|----|
//   |Wall|Mirr|Wall|
//   |    | or |    |
//   /--------------/
//  /   Floor      /
// /--------------/

    std::array<Vertex, 20> vertices =
    {
        // Floor: Observe we tile texture coordinates.
        Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
        Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
        Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
        Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

        // Wall: Observe we tile texture coordinates, and that we
        // leave a gap in the middle for the mirror.
        Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
        Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
        Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

        Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
        Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
        Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

        Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
        Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
        Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

        // Mirror
        Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
        Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
        Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
    };

    std::array<std::int16_t, 30> indices =
    {
        // Floor
        0, 1, 2,
        0, 2, 3,

        // Walls
        4, 5, 6,
        4, 6, 7,

        8, 9, 10,
        8, 10, 11,

        12, 13, 14,
        12, 14, 15,

        // Mirror
        16, 17, 18,
        16, 18, 19
    };

    SubmeshGeometry floorSubmesh;
    floorSubmesh.IndexCount = 6;
    floorSubmesh.StartIndexLocation = 0;
    floorSubmesh.BaseVertexLocation = 0;

    SubmeshGeometry wallSubmesh;
    wallSubmesh.IndexCount = 18;
    wallSubmesh.StartIndexLocation = 6;
    wallSubmesh.BaseVertexLocation = 0;

    SubmeshGeometry mirrorSubmesh;
    mirrorSubmesh.IndexCount = 6;
    mirrorSubmesh.StartIndexLocation = 24;
    mirrorSubmesh.BaseVertexLocation = 0;

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "roomGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["floor"] = floorSubmesh;
    geo->DrawArgs["wall"] = wallSubmesh;
    geo->DrawArgs["mirror"] = mirrorSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void D3D12::BuildSkullGeometry()
{
    // 파일을 읽기 모드로 연다.
    std::ifstream fin("Models/skull.txt");

    if (!fin)
    {
        MessageBox(mhMainWnd, L"Modles/skull.tex not found", 0, 0);
        return;
    }

    UINT vcount = 0;
    UINT tcount = 0;
    std::string ignore;

    // 공백 앞까지가 하나의 토큰. 공백은 포함되지 않음.
    fin >> ignore >> vcount; // ignore("VertexCount:"), vcount(31076)
    fin >> ignore >> tcount; // ignore("TriangleCount"), tcount(60339)
    fin >> ignore >> ignore >> ignore >> ignore; // ignore("VertexList", "(pos,", "normal)", "{") 결과적으로 "{" 가 마지막으로 남음.

    std::vector<Vertex> vertices(vcount);
    for (UINT i = 0; i < vcount; ++i)
    {
        //0.592978 1.92413 - 2.62486 0.572276 0.816877 0.0721907
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z; 
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

        // Skull model 이 texture coordinates가 없어서 0으로 설정.
        vertices[i].TexC = { 0.f, 0.f };
    }

    //3줄 더 버려야함
    //}
    //TriangleList
    //{
    fin >> ignore;
    fin >> ignore;
    fin >> ignore;
    
    std::vector<std::int32_t> indices(3 * tcount);
    for (UINT i = 0; i < tcount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }

    fin.close();

    // Pack indices into one index buffer.
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skullGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    memcpy(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry subMesh;
    subMesh.BaseVertexLocation = 0;
    subMesh.StartIndexLocation = 0;
    subMesh.IndexCount = (UINT)indices.size();

    geo->DrawArgs["skull"] = subMesh;
    mGeometries[geo->Name] = std::move(geo);
}
