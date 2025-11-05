#include "LitWavesApp.h"

#include "../Common/Waves.h"

LitWavesApp::LitWavesApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

LitWavesApp::~LitWavesApp()
{
    if (md3dDevice != nullptr)
    {
        FlushCommandQueue();
    }
}

bool LitWavesApp::Initialize()
{
    if (!D3DApp::Initialize())
    {
        return false;
    }

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mWaves = std::make_unique<Waves>(128, 128, 1.f, 0.03f, 4.f, 0.2f);

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildLandGeometry();
    BuildWavesGeometryBuffers();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    FlushCommandQueue();    
    
    return true;
}

void LitWavesApp::OnResize()
{
    D3DApp::OnResize();

    DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH((float)0.25*MathHelper::Pi, AspectRatio(), 1.f, 1000.f);
    DirectX::XMStoreFloat4x4(&mProj, P);    
}

void LitWavesApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
    UpdateCamera(gt);

    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateMainPassCB(gt);
    UpdateWaves(gt);
}

void LitWavesApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    mCommandList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to command queue to set new fence point.
    // Because we are on GPU timeline, new fence point won't be set
    // until GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void LitWavesApp::OnKeyboardInput(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
    {
        mSunTheta -= 1.f*dt;
    }
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
    {
        mSunTheta += 1.f*dt;
    }
    if (GetAsyncKeyState(VK_UP) & 0x8000)
    {
        mSunPhi -= 1.f*dt;
    }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
    {
        mSunPhi += 1.f*dt;
    }

    mSunPhi = MathHelper::Clamp(mSunPhi, 0.1f, DirectX::XM_PIDIV2);
}

void LitWavesApp::UpdateCamera(const GameTimer& gt)
{
    // Convert Spherical to Cartesian coordinates.
    mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
    mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
    mEyePos.y = mRadius*cosf(mPhi);

    // Build the view matrix.
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
    DirectX::XMVECTOR target = DirectX::XMVectorZero();
    //DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);
}

void LitWavesApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        if (e->NumFramesDirty > 0)
        {
            DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&e->World);
            DirectX::XMMATRIX texTransform = DirectX::XMLoadFloat4x4(&e->TexTransfrom);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

            currObjectCB->CopyData(e->ObjCBIndex, objConstants);

            e->NumFramesDirty--;
        }
    }
}

void LitWavesApp::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    for (auto& e : mMaterials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            DirectX::XMMATRIX matTransform = DirectX::XMLoadFloat4x4(&mat->MatTransform);

            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;

            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

            mat->NumFramesDirty--;
        }
    }
}

void LitWavesApp::UpdateMainPassCB(const GameTimer& gt)
{
}

void LitWavesApp::UpdateWaves(const GameTimer& gt)
{
}

void LitWavesApp::BuildRootSignature()
{
}

void LitWavesApp::BuildShadersAndInputLayout()
{
}

void LitWavesApp::BuildLandGeometry()
{
}

void LitWavesApp::BuildWavesGeometry()
{
}

void LitWavesApp::BuildWavesGeometryBuffers()
{
}

void LitWavesApp::BuildPSOs()
{
}

void LitWavesApp::BuildFrameResources()
{
}

void LitWavesApp::BuildMaterials()
{
}

void LitWavesApp::BuildRenderItems()
{
}

void LitWavesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& items)
{
}

float LitWavesApp::GetHillsHeight(float x, float z) const
{
    return 0.f;
}

DirectX::XMFLOAT3 LitWavesApp::GetHillsNormal(float x, float z) const
{
    return DirectX::XMFLOAT3(0.f, 0.f, 1.f);
}
