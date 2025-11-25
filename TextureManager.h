#pragma once

#include <d3d12.h>
#include "Texture.h"
#include <unordered_map>

class TextureManager
{
public:
	TextureManager(ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCmdList);

	std::shared_ptr<Texture> LoadTexture(const std::string& InTextureName, const std::wstring& InFileName);

private:
	ID3D12Device* Device;
	ID3D12GraphicsCommandList* CmdList;
	std::unordered_map<std::string, std::shared_ptr<Texture>> Textures;
};