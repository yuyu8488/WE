#include "TextureManager.h"

#include "d3dUtil.h"
#include "DDSTextureLoader12.h"

TextureManager::TextureManager(ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCmdList)
	: Device(InDevice), CmdList(InCmdList) 
{
}

std::shared_ptr<Texture> TextureManager::LoadTexture(const std::string& InTextureName, const std::wstring& InFileName)
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

    // ¹Ø¿¡°Å ¼öÁ¤...

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(GrassTex->Resource.Get(),
        0, (UINT)(GrassTex->Subresources.size()));

    // Create GPU upload buffer.
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(GrassTex->UploadHeap.ReleaseAndGetAddressOf())));

    auto BarrierToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(GrassTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    mCommandList->ResourceBarrier(1, &BarrierToCopyDest);

    UpdateSubresources(mCommandList.Get(), GrassTex->Resource.Get(), GrassTex->UploadHeap.Get(),
        0, 0, static_cast<UINT>(GrassTex->Subresources.size()), GrassTex->Subresources.data());

    auto BarrierToPixelShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(GrassTex->Resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &BarrierToPixelShaderResource);

    Textures[GrassTex->Name] = std::move(GrassTex);

	return std::shared_ptr<Texture>();
}
