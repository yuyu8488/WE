#pragma once

#include "./Engine/WEngine.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    if (SUCCEEDED(CoInitialize(nullptr)))
    {
        WEngine* Engine = nullptr;
        Engine = new WEngine();
            
        if (SUCCEEDED(Engine->Initialize()))
        {                
            Engine->RunMessageLoop();
        }
        
        CoUninitialize();
    }
    
    return 0;
}

