#include "Box.h"

Box::Box(HINSTANCE hInstance) : D3D12App(hInstance), mLastMousePos()
{
}

Box::~Box()
{
}

bool Box::Initialize()
{
    if (!D3D12App::Initialize())
    {
        return false;
    }

    // 초기화 명령들을 준비하기 위해 명령 목록을 재설정.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

    // 초기화 명령 실행.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsList[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdsList), cmdsList);

    FlushCommandQueue();

    return true;    
}

void Box::OnResize()
{
    D3D12App::OnResize();

    DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, GetAspectRatio(), 1.f, 1000.f);
    DirectX::XMStoreFloat4x4(&mProj, P);
}

void Box::Update(const GameTimer& gt)
{
    // 구면 좌표를 직교좌표(데카르트좌표)로 변환
    float X = mRadius*sinf(mPhi)*cosf(mTheta);
    float Z = mRadius*sinf(mPhi)*sinf(mTheta);
    float Y = mRadius*cosf(mPhi);

    // 시야 행렬 
    DirectX::XMVECTOR Pos = DirectX::XMVectorSet(X,Y,Z,1.f);
    DirectX::XMVECTOR Target = DirectX::XMVectorZero();
    DirectX::XMVECTOR Up = DirectX::XMVectorSet(0.f,1.f,0.f,0.f);
    DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(Pos, Target, Up);
    DirectX::XMStoreFloat4x4(&mView, View);
    
    //DirectX::XMMATRIX World = XMLoadFloat4x4(&mWorld);
    DirectX::XMMATRIX BoxWorld = XMLoadFloat4x4(&mWorld);
	DirectX::XMFLOAT4X4 P(
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 1.f, 0.f, 1.f);
    DirectX::XMMATRIX PyramidWorld = XMLoadFloat4x4(&P);


    DirectX::XMMATRIX Proj = DirectX::XMLoadFloat4x4(&mProj);
    //DirectX::XMMATRIX WorldViewProjection = World * View * Proj;

    // Update constant buffer
    ObjectConstants ObjConstants;
    ObjConstants.gTime = mTimer.TotalTime();

	DirectX::XMStoreFloat4x4(&ObjConstants.WorldViewProjection, DirectX::XMMatrixTranspose(BoxWorld * View * Proj));
	mObjectCB->CopyData(0, ObjConstants);

    DirectX::XMStoreFloat4x4(&ObjConstants.WorldViewProjection, DirectX::XMMatrixTranspose(PyramidWorld * View * Proj));
    mObjectCB->CopyData(1, ObjConstants);
}

void Box::Draw(const GameTimer& gt)
{
    // 명령 기록에 관련된 메모리의 재활용을 위해 명령 할당자를 재설정.
    // 재설정은 GPU가 관련 명령 목록들을 모두 처리한 후에 일어남.
    ThrowIfFailed(mDirectCmdListAlloc->Reset());

    // 명령 목록을 ExecuteCommandList를 통해서 명령대기열에 추가했다면 명령 목록 재설정 가능.
    // 명령 목록을 재설정하면 메모리가 재활용됨.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // 자원 용도에 관련된 상태 전이를 Direct3D에 통지.
    CD3DX12_RESOURCE_BARRIER rb = CD3DX12_RESOURCE_BARRIER::Transition(
        GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &rb);

    mCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

    // 렌더링 결과가 기록될 렌더 대상 버퍼들을 지정.
    D3D12_CPU_DESCRIPTOR_HANDLE BackBufferView = GetCurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView = GetDepthStencilView();
    mCommandList->OMSetRenderTargets(1, &BackBufferView, true,  &DepthStencilView);

    ID3D12DescriptorHeap* DescriptorHeaps[] = {mCbvHeap.Get()};
    mCommandList->SetDescriptorHeaps(_countof(DescriptorHeaps), DescriptorHeaps);
    
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    //D3D12_VERTEX_BUFFER_VIEW Vbv = mBoxGeo->VertexBufferView();
    //mCommandList->IASetVertexBuffers(0, 1, &Vbv);
    
    D3D12_VERTEX_BUFFER_VIEW VPosView = mBoxGeo->VPosBufferView();
    mCommandList->IASetVertexBuffers(0, 1, &VPosView);

	D3D12_VERTEX_BUFFER_VIEW VColorView = mBoxGeo->VColorBufferView();
	mCommandList->IASetVertexBuffers(1, 1, &VColorView);

    D3D12_INDEX_BUFFER_VIEW Ibv = mBoxGeo->IndexBufferView();
    mCommandList->IASetIndexBuffer(&Ibv);

    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto GPUDescriptorHandleForHeapStart = mCbvHeap->GetGPUDescriptorHandleForHeapStart();
    mCommandList->SetGraphicsRootDescriptorTable(0, GPUDescriptorHandleForHeapStart);

    mCommandList->DrawIndexedInstanced(
        mBoxGeo->DrawArgs["Box"].IndexCount,
        1,
        mBoxGeo->DrawArgs["Box"].StartIndexLocation,
        mBoxGeo->DrawArgs["Box"].BaseVertexLocation,
        0);

	GPUDescriptorHandleForHeapStart = mCbvHeap->GetGPUDescriptorHandleForHeapStart();
    GPUDescriptorHandleForHeapStart.ptr += mCbvSrvUavDescriptorSize;
	mCommandList->SetGraphicsRootDescriptorTable(0, GPUDescriptorHandleForHeapStart);

    mCommandList->DrawIndexedInstanced(
        mBoxGeo->DrawArgs["Pyramid"].IndexCount,
        1,
        mBoxGeo->DrawArgs["Pyramid"].StartIndexLocation,
        mBoxGeo->DrawArgs["Pyramid"].BaseVertexLocation,
        0);

    rb = CD3DX12_RESOURCE_BARRIER::Transition(
    GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &rb);
    
    ThrowIfFailed(mCommandList->Close());

    // 명령 실행을 위해 명령 목록을 명령 대기열에 추가
    ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 후면 버퍼와 전면 버퍼 교환
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Box 예제에서는 간단함을 위해 프레임 명령들이 모두 처리되길 기다림(비효율적)
    FlushCommandQueue();
}

void Box::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc ={};
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    cbvHeapDesc.NumDescriptors = 2;
    ThrowIfFailed(mD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
}

void Box::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(mD3dDevice.Get(), 2, true);
	UINT ObjCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex*ObjCBByteSize; 
	
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = ObjCBByteSize;
	
	mD3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart()); 
	
	int PyramidBufIndex = 1;
	cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
    cbAddress += PyramidBufIndex * ObjCBByteSize;
	cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = ObjCBByteSize;

    auto HeapHandle = mCbvHeap->GetCPUDescriptorHandleForHeapStart();
    HeapHandle.ptr += mCbvSrvUavDescriptorSize;
    mD3dDevice->CreateConstantBufferView(&cbvDesc, HeapHandle);
}

void Box::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,1,0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // 상수 버퍼 하나로 구성된 서술자 구간을 가리키는 슬롯하나로 이루어진 루트서명을 생성.
    Microsoft::WRL::ComPtr<ID3DBlob> serializeRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        serializeRootSignature.GetAddressOf(),
        errorBlob.GetAddressOf());

    if (errorBlob)
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(mD3dDevice->CreateRootSignature(
        0,
        serializeRootSignature->GetBufferPointer(),
        serializeRootSignature->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));
}

void Box::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;
    
    mvsByteCode = D3DUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = D3DUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "PS", "ps_5_0");

	//mInputLayout =
	//{
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	//};

    // VertexEx
	//mInputLayout =
	//{
	//    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//    { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//    { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	//    { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 41, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	//};

    // VPosData, VColorData
    mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void Box::BuildBoxGeometry()
{
    //std::array<Vertex, 8> vertices =
    //{
    //    Vertex({ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::White) }),
    //    Vertex({ DirectX::XMFLOAT3(-1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Black) }),
    //    Vertex({ DirectX::XMFLOAT3(+1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Red) }),
    //    Vertex({ DirectX::XMFLOAT3(+1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Green) }),
    //    Vertex({ DirectX::XMFLOAT3(-1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Blue) }),
    //    Vertex({ DirectX::XMFLOAT3(-1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Yellow) }),
    //    Vertex({ DirectX::XMFLOAT3(+1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Cyan) }),
    //    Vertex({ DirectX::XMFLOAT3(+1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Magenta) })
    //};

    std::array<VPosData, 13> vertices =
    {
		// 정육면체
        VPosData({DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f)}),
        VPosData({DirectX::XMFLOAT3(-1.0f, +1.0f, -1.0f)}),
        VPosData({DirectX::XMFLOAT3(+1.0f, +1.0f, -1.0f)}),
        VPosData({DirectX::XMFLOAT3(+1.0f, -1.0f, -1.0f)}),
        VPosData({DirectX::XMFLOAT3(-1.0f, -1.0f, +1.0f)}),
        VPosData({DirectX::XMFLOAT3(-1.0f, +1.0f, +1.0f)}),
        VPosData({DirectX::XMFLOAT3(+1.0f, +1.0f, +1.0f)}),
        VPosData({DirectX::XMFLOAT3(+1.0f, -1.0f, +1.0f)}),

        // 사각뿔
        VPosData({DirectX::XMFLOAT3(-1.f, -0.f, -1.f)}),
        VPosData({DirectX::XMFLOAT3(-1.f, +0.f, +1.f)}),
        VPosData({DirectX::XMFLOAT3(+1.f, +0.f, +1.f)}),
        VPosData({DirectX::XMFLOAT3(+1.f, -0.f, -1.f)}),
        VPosData({DirectX::XMFLOAT3(+0.f,  +2.f, +0.f)}),
    };

    std::array<VColorData, 13> colors = 
    {
		VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
		VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
		VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
		VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
		VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
		VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
		VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
		VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),

        VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
        VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
        VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
        VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
        VColorData({DirectX::XMFLOAT4(DirectX::Colors::Red)}),
    };

    std::array<std::uint16_t, 54> indices =
    {
       
        // front face
        0, 1, 2,
        0, 2, 3,
        // back face
        4, 6, 5,
        4, 7, 6,
        // left face
        4, 5, 1,
        4, 1, 0,
        // right face
        3, 2, 6,
        3, 6, 7,
        // top face
        1, 5, 6,
        1, 6, 2,
        // bottom face
        4, 0, 3,
        4, 3, 7,

        0, 3, 2,
        0, 2, 1,
        0, 4, 3,
        3, 4, 2,
        2, 4, 1,
		1, 4, 0,
    };

    //const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    //const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    const UINT VPosByteSize = (UINT)vertices.size() * sizeof(VPosData);
    const UINT VColorByteSize = (UINT)colors.size() * sizeof(VColorData);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "boxGeo";

    //ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
    //CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(VPosByteSize, &mBoxGeo->VPosBufferCPU));
    CopyMemory(mBoxGeo->VPosBufferCPU->GetBufferPointer(), vertices.data(), VPosByteSize);

    ThrowIfFailed(D3DCreateBlob(VColorByteSize, &mBoxGeo->VColorBufferCPU));
    CopyMemory(mBoxGeo->VColorBufferCPU->GetBufferPointer(), colors.data(), VColorByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    //mBoxGeo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(
    //    mD3dDevice.Get(), mCommandList.Get(),
    //    vertices.data(), vbByteSize,
    //    mBoxGeo->VertexBufferUploader);
    
	mBoxGeo->VPosBufferGPU = D3DUtil::CreateDefaultBuffer(
		mD3dDevice.Get(), mCommandList.Get(),
		vertices.data(), VPosByteSize,
		mBoxGeo->VPosBufferUploader);

    mBoxGeo->VColorBufferGPU = D3DUtil::CreateDefaultBuffer(
        mD3dDevice.Get(), mCommandList.Get(),
        colors.data(), VColorByteSize,
        mBoxGeo->VColorBufferUploader);

    mBoxGeo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(
        mD3dDevice.Get(), mCommandList.Get(),
        indices.data(), ibByteSize,
        mBoxGeo->IndexBufferUploader);

    //mBoxGeo->VertexByteStride = sizeof(VPosData) + sizeof(VColorData);
    //mBoxGeo->VertexBufferByteSize = vbByteSize;
    
    mBoxGeo->VPosByteStride = sizeof(VPosData);
    mBoxGeo->VPosBufferByteSize = VPosByteSize;

    mBoxGeo->VColorByteStride = sizeof(VColorData);
    mBoxGeo->VColorBufferByteSize = VColorByteSize;

    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = ibByteSize;

    //SubmeshGeometry submesh;
    //submesh.BaseVertexLocation = 0;
    //submesh.Bounds;
    //submesh.IndexCount = (UINT)indices.size();

    //mBoxGeo->DrawArgs["box"] = submesh;

    SubmeshGeometry BoxMesh;
    BoxMesh.BaseVertexLocation = 0;
    BoxMesh.StartIndexLocation = 0;
    BoxMesh.IndexCount = 36;
    BoxMesh.Bounds;

    SubmeshGeometry PyramidMesh;
    PyramidMesh.BaseVertexLocation = 8;
    PyramidMesh.StartIndexLocation = 36;
    PyramidMesh.IndexCount = 18;
    PyramidMesh.Bounds;
    
    mBoxGeo->DrawArgs["Box"] = BoxMesh;
    mBoxGeo->DrawArgs["Pyramid"] = PyramidMesh;
}

void Box::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
    {
        static_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
        mvsByteCode->GetBufferSize()
    };
    psoDesc.PS =
    {
        static_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(mD3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

void Box::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void Box::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Box::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        mTheta += dx;
        mPhi += dy;

        mPhi = MathHelper::Clamp(mPhi, 0.1f, DirectX::XM_PI - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

        mRadius += dx - dy;

        mRadius = MathHelper::Clamp(mRadius, 3.f, 15.f);
    }
    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
