#pragma once
#include <array>

namespace WE
{
    
class Vector2
{
public:
    Vector2() = default;
    __forceinline Vector2(int InX, int InY) : X((float)InX), Y((float)InY) {}
    __forceinline Vector2(float InX, float InY) : X(InX), Y(InY) {}
    
    struct 
    {
        float X = 0;
        float Y = 0;
    };
};
    
}

