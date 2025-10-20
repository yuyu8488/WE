#pragma once

#include "./D3D12/D3D12App.h"
#include "../Box/Box.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int ShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

try
{
    Box NewBox(hInstance);
    if (!NewBox.Initialize())
    {
        return 0;
    }

    return NewBox.Run();
}
catch (DxException& e)
{
    MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
    return 0;
}
}