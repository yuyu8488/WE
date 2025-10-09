#pragma once
#include "BufferBase.h"

class IndexBuffer : public BufferBase
{
public:
    HRESULT Initialize(ID3D11Device* Device, void* Data, UINT Count) override
    {
        Release();

        IndexCount = Count;
        D3D11_BUFFER_DESC BufferDesc = {};
        BufferDesc.Usage = D3D11_USAGE_DEFAULT;
        BufferDesc.ByteWidth = sizeof(DWORD) * IndexCount;
        BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        BufferDesc.CPUAccessFlags = 0;
        BufferDesc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA InitialData = {};
        InitialData.pSysMem = Data;

        return Device->CreateBuffer(&BufferDesc, &InitialData, &Buffer);
    }

    void Bind(ID3D11DeviceContext* Context, UINT StartSlot = 0, UINT Offset = 0) override
    {
        Context->IASetIndexBuffer(Buffer, DXGI_FORMAT_R32_UINT, Offset);
    }

    UINT GetIndexCount() const { return IndexCount; }

private:
    UINT IndexCount = 0;
};