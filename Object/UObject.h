#pragma once
#include <d2d1.h>

class UObject
{
public:
    UObject() = default;
    virtual ~UObject() = default;

    UObject(const UObject&) = delete;
    UObject& operator=(const UObject&) = delete;
    UObject(const UObject&&) = delete;
    UObject& operator=(const UObject&&) = delete;

    virtual void Render(ID2D1RenderTarget* RenderTarget, ID2D1SolidColorBrush* SolidColorBrush);
};
