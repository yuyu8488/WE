#pragma once
#include "BufferBase.h"

template<typename T>
class ConstantBuffer : public BufferBase
{
public:
    HRESULT Initialize(ID3D11Device* Device, void* Data, UINT Count) override
    {
        Release();

        D3D11_BUFFER_DESC BufferDesc = {};
        BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        BufferDesc.ByteWidth = sizeof(T) + + (16 - (sizeof(T) % 16)) % 16; // 16바이트 정렬
        BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA InitData = {};
        InitData.pSysMem = Data;

        return Device->CreateBuffer(&BufferDesc, Data ? &InitData : nullptr, &Buffer);
    }

    void Update(ID3D11DeviceContext* Context, const T& Data)
    {
        D3D11_MAPPED_SUBRESOURCE MappedResource = {};
        Context->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
        memcpy(MappedResource.pData, &Data, sizeof(T));
        Context->Unmap(Buffer, 0);
    }

    void Bind(ID3D11DeviceContext* Context, UINT StartSlot = 0, UINT Offset = 0) override
    {
        Context->VSSetConstantBuffers(StartSlot, 1, &Buffer);
    }

    void BindPS(ID3D11DeviceContext* Context, UINT Slot = 0)
    {
        Context->PSSetConstantBuffers(Slot, 1, &Buffer);
    }
};
