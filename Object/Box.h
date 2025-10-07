#pragma once
#include <d2d1.h>

#include "UObject.h"

class UBox : public UObject
{
public:
    UBox(float InPositionX, float InPositionY, float InWidth, float InHeight);
    
    virtual void Render(ID2D1RenderTarget* RenderTarget, ID2D1SolidColorBrush* SolidColorBrush) override;
    
    void Move(float DeltaX, float DeltaY);

private:
    D2D1_RECT_F Rect = {};
};
