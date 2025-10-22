#pragma once

#include "../Engine/D3D12/D3D12App.h"
#include "../Engine/Common/MathHelper.h"
#include "../Engine/Common/UploadBuffer.h"


struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

struct VPosData
{
    DirectX::XMFLOAT3 pos;
};

struct VColorData
{
    DirectX::XMFLOAT4 Color;
};

struct VertexEx
{
	DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Tangent;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 Tex0;
    DirectX::XMFLOAT2 Tex1;
    DirectX::PackedVector::XMCOLOR Color;
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 WorldViewProjection = MathHelper::Identity4x4();
};

class Box : public D3D12App
{
public:
    Box(HINSTANCE hInstance);
    Box(const Box& rhs) = delete;
    Box& operator=(const Box& rhs) = delete;
    
    virtual ~Box() override;
    virtual bool Initialize() override;


private:
    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;

    DirectX::XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * DirectX::XM_PI;
    float mPhi = DirectX::XM_PIDIV4;
    float mRadius = 5.f;

    POINT mLastMousePos;    
};
