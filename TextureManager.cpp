#include "TextureManager.h"

#include "d3dUtil.h"
#include "DDSTextureLoader12.h"

FTextureManager::FTextureManager(ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCmdList)
	: Device(InDevice), CmdList(InCmdList) 
{
}

void FTextureManager::LoadTexture(const std::string& InTextureName, const std::wstring& InFileName)
{
	auto NewTexture = std::make_shared<Texture>();
	NewTexture->Name = InTextureName;
	NewTexture->Filename = InFileName;

	ThrowIfFailed(DirectX::LoadDDSTextureFromFile(
		Device,
		NewTexture->Filename.c_str(),
		NewTexture->Resource.ReleaseAndGetAddressOf(),
		NewTexture->DdsData,
		NewTexture->Subresources));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(NewTexture->Resource.Get(),
        0, (UINT)(NewTexture->Subresources.size()));

    // Create GPU upload buffer.
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ThrowIfFailed(Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(NewTexture->UploadHeap.ReleaseAndGetAddressOf())));

    auto BarrierToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(NewTexture->Resource.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    CmdList->ResourceBarrier(1, &BarrierToCopyDest);

    UpdateSubresources(CmdList, NewTexture->Resource.Get(), NewTexture->UploadHeap.Get(),
        0, 0, static_cast<UINT>(NewTexture->Subresources.size()), NewTexture->Subresources.data());

    auto BarrierToPixelShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(NewTexture->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    CmdList->ResourceBarrier(1, &BarrierToPixelShaderResource);

    Textures[NewTexture->Name] = std::move(NewTexture);
}

Texture* FTextureManager::GetTexture(const std::string& Name)
{
    return Textures[Name].get();
}
