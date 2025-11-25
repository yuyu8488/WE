#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <memory>
#include <vector>

struct Texture
{
    // Unique material name for lookup.
    std::string Name;

    std::wstring Filename;

    // GPU에 올라가는 실제 텍스처 리소스
    Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;

    // GPU로 복사할때 사용하는 업로드용 힙
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;

    // DDS 원본 바이트 데이터를 담아두는 버퍼(Subresources가 참조하는 대상)
    std::unique_ptr<uint8_t[]> DdsData;

    // DDS 파일에서 추출된 각 서브리소스 데이터 (mip level, array slice 등)
    std::vector<D3D12_SUBRESOURCE_DATA> Subresources; 
};