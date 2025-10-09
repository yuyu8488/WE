#pragma once
#include <d3d11.h>

#include "../Utility/ComUtils.h"


class BufferBase
{
public:
    virtual ~BufferBase()
    {
        Release();
    }

    virtual HRESULT Initialize(ID3D11Device* Device, void* Data, UINT Count) = 0;
    virtual void Bind(ID3D11DeviceContext* Context, UINT StartSlot = 0, UINT Offset = 0) = 0;
    virtual void Release()
    {
        SafeRelease(&Buffer);
    }

    ID3D11Buffer* GetBuffer() const {return Buffer;}
    ID3D11Buffer** GetBufferAddress() {return &Buffer;}
    
protected:
    ID3D11Buffer* Buffer = nullptr;
};
