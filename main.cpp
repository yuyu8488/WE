#pragma once

#include "Framework.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    if (SUCCEEDED(CoInitialize(nullptr)))
    {
        Framework* MyFramework = nullptr;
        MyFramework = new Framework();
            
        if (SUCCEEDED(MyFramework->Initialize()))
        {                
            MyFramework->RunMessageLoop();
        }
        
        CoUninitialize();
    }
    
    return 0;
}

