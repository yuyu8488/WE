#pragma once

#include "../Engine/D3D12/D3D12.h"
#include "../Engine/Common/MathHelper.h"
#include "../Engine/Common/UploadBuffer.h"

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

struct ObjectConstants
{
    DirectX::XMFLOAT4X4 WorldViewProjection = MathHelper::Identity4x4();
};

class Box : public D3D12
{
public:
    Box(HINSTANCE hInstance);
    Box(const Box& rhs) = delete;
    Box& operator=(const Box& rhs) = delete;
    virtual ~Box();

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

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
    
};
