#pragma once
#include <DirectXMath.h>
#include <string>
#include "MathHelper.h"
#include "config.h"

// Simple struct to represent a material for our demos.  A production 3D engine
// would likely create a class hierarchy of Materials.
struct Material
{
public:
    Material() {};

    std::string Name;

    int MatCBIndex = -1;
    int DiffuseSrvHeapIndex = -1;
    int NormalSrvHeapIndex = -1;

    // Dirty flag indicating the material has changed and we need to update the constant buffer.
    // Because we have a material constant buffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify a material we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = NUM_FRAME_RESOURCES;
    

    // Material constant buffer data used for shading.
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = .25f;
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

};