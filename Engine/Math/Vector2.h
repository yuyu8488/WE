#pragma once
#include <array>

namespace WE
{
    
class Vector2
{
public:
    __forceinline constexpr Vector2() = default;
    __forceinline constexpr Vector2(int InX, int InY) : X((float)InX), Y((float)InY) {}
    
    struct 
    {
        float X;
        float Y;
    };
};
    
}

