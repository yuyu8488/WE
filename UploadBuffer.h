#pragma once

#include "d3d12.h"
#include "d3dx12.h"
#include "wrl.h"

// CPU에서 GPU로 데이터를 복사하기위한 업로드 힙을 생성하고 관리하는 Template Class.
template<typename T>
class UploadBuffer
{
public:
    UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer);
    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

    virtual ~UploadBuffer();

    ID3D12Resource* Resource() const;

    void CopyData(int elementIndex, const T& data);

private:
    // 실제 GPU 리소스를 가리키는 포인터
    Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;

    // GPU 메모리를 CPU에서 접근할 수 있도록 매핑한 포인터
    BYTE* mMappedData = nullptr;

    // 각 요소의 크기(상수 버퍼일 경우 256바이트 단위로 정렬)
    UINT mElementByteSize = 0;

    // 상수 버퍼 여부 플래그
    bool mIsConstantBuffer = false;
};

template<typename T>
inline UploadBuffer<T>::UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer)
    : mIsConstantBuffer(isConstantBuffer)
{
    mElementByteSize = sizeof(T);

    if (isConstantBuffer)
    {
        mElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));
    }

    // 자원의 힙에 대한 속성
    // D3D12_HEAP_TYPE_UPLOAD: CPU가 쓰고, GPU가 읽을 수 있는 메모리 영역(Upload Heap)
    const CD3DX12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // 자원 설명 구조체
    const CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);

    // GPU에 실제 리소스(버퍼) 생성
    ThrowIfFailed(device->CreateCommittedResource(
        &HeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &ResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mUploadBuffer)));

    // GPU 메모리를 CPU 가상 메모리 공간에 매핑해서 접근 가능하게 함.
    ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));  
}

template<typename T>
inline UploadBuffer<T>::~UploadBuffer()
{
    if (mUploadBuffer != nullptr)
    {
        mUploadBuffer->Unmap(0, nullptr);
    }

    mMappedData = nullptr;
}

template<typename T>
inline ID3D12Resource* UploadBuffer<T>::Resource() const
{
    return mUploadBuffer.Get();
}

template<typename T>
inline void UploadBuffer<T>::CopyData(int elementIndex, const T& data)
{
    memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
}
