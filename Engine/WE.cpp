#pragma once

#include "./D3D12/D3D12App.h"
#include "Common/MathHelper.h"
#include "..\Shapes\Shapes.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int ShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    Shapes App(hInstance);
    if (App.Initialize())
    {
        return App.Run();
    }

    return 0;
}