#pragma once

#include "Box.h"
#include <d2d1helper.h>

UBox::UBox(float InPositionX, float InPositionY, float InWidth, float InHeight)
{
    Rect = D2D1::RectF(InPositionX, InPositionY, InPositionX + InWidth, InPositionY+InHeight);
}

void UBox::Render(ID2D1RenderTarget* RenderTarget, ID2D1SolidColorBrush* Brush)
{
    if (Brush)
    {
        RenderTarget->FillRectangle(&Rect, Brush);    
    }
}

void UBox::Move(float DeltaX, float DeltaY)
{
    Rect.left += DeltaX;
    Rect.right += DeltaX;
    Rect.top += DeltaY;
    Rect.bottom += DeltaY;    
}
