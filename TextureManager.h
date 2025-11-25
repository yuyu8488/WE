#pragma once


#include <d3d12.h>
#include <unordered_map>
#include "Texture.h"

class FTextureManager
{
public:
	FTextureManager(ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCmdList);

	void LoadTexture(const std::string& InTextureName, const std::wstring& InFileName);

	Texture* GetTexture(const std::string& Name);

private:
	ID3D12Device* Device;
	ID3D12GraphicsCommandList* CmdList;
	std::unordered_map<std::string, std::shared_ptr<Texture>> Textures;
};