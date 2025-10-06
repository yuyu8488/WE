#pragma once

#include "D2dEngine.h"



int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    if (SUCCEEDED(CoInitialize(NULL)))
    {
        {
            D2dEngine D2d;
            if (SUCCEEDED(D2d.Initialize()))
            {
                D2d.RunMessageLoop();
            }
        }
        CoUninitialize();
    }
    
    return 0;
}
