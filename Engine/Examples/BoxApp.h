#pragma once

#pragma once

#include "../D3D12/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"

namespace Box
{
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
        float gTime;
    };
}
class BoxApp : public D3DApp
{
public:
    BoxApp(HINSTANCE hInstance);
    BoxApp(const BoxApp& rhs) = delete;
    BoxApp& operator=(const BoxApp& rhs) = delete;
    
    virtual ~BoxApp() override;
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

    std::unique_ptr<UploadBuffer<Box::ObjectConstants>> mObjectCB = nullptr;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;
};

