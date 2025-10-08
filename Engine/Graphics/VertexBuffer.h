#pragma once
#include <intsafe.h>

#include "BufferBase.h"

template<typename T>
class VertexBuffer : public BufferBase
{
public:
    HRESULT Initialize(ID3D11Device* Device, void* Data, UINT Count) override
    {
        Release();
    
        Stride = sizeof(T);
        VertexCount = Count;
    
        D3D11_BUFFER_DESC BufferDesc ={};
        BufferDesc.Usage = D3D11_USAGE_DEFAULT;
        BufferDesc.ByteWidth = Stride * VertexCount;
        BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        BufferDesc.CPUAccessFlags = 0;
        BufferDesc.MiscFlags = 0;
        
        D3D11_SUBRESOURCE_DATA InitialData = {};
        InitialData.pSysMem = Data;
    
        return Device->CreateBuffer(&BufferDesc, &InitialData, &Buffer);
    }

    void Bind(ID3D11DeviceContext* Context, UINT StartSlot = 0, UINT Offset = 0) override
    {
        Context->IASetVertexBuffers(StartSlot, 1, &Buffer, &Stride, &Offset);
    }
    
    UINT GetVertexCount() const { return VertexCount; }

private:
    UINT Stride = 0;
    UINT VertexCount = 0;
};

