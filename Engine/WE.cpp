#pragma once

#include "./D3D12/D3D12App.h"
#include "Common/MathHelper.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int ShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    
}