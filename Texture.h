#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>

class Texture
{
    // Unique material name for lookup.
    std::string Name;

    std::wstring Filename;

    Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};