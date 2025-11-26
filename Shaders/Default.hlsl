#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);

SamplerState gSamPointWrap : register(s0);
SamplerState gSamPointClamp : register(s1);
SamplerState gSamLinearWrap : register(s2);
SamplerState gSamLinearClamp : register(s3);
SamplerState gSamAnisotropicWrap : register(s4);
SamplerState gSamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
	float4 gAmbientLight;
	
	// 응용 프로그램이 안개 매개변수들을 프레임별로 변경할 숭 있게 한다.
	// 예를 들어 하루 중 특정 시간대에만 안개를 적용.
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 cbPerObjectPad2;

	Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float gRoughness;
	float4x4 gMatTransform;
};
 
struct VertexIn
{
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut
{
	float4 PosH		: SV_POSITION;
	float3 PosW		: POSITION;
	float3 NormalW	: NORMAL;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut) 0.0f;
	
    // Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;

    // 세계 행렬에 비균등 비례가 없다고 가정하고, 법선을 변환한다.
	// 비균등 비례가 없다면 역전치 행렬을 사용.
	vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);

    // Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);
	
	float4 texC = mul(float4(vin.TexC, 0.f, 1.f), gTexTransform);
	vout.TexC = mul(texC, gMatTransform).xy;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 diffuseAlbedo = gDiffuseMap.Sample(gSamLinearWrap, pin.TexC) * gDiffuseAlbedo;

#ifdef ALPHA_TEST
	// 텍스처 알파가 0.1보다 작으면 픽셀을 폐기.
	// 셰이더 안에서 이판정을 최대한 일찍하는 것이 바람직 >> 나머지 코드의 실행을 생략할수 있어서 효율적.
	clip(diffuseAlbedo.a - 0.1f);
#endif
	
    // Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
	float3 toEyeW = gEyePosW - pin.PosW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye; // Normalize;
	
	// Indirect lighting.
	float4 ambient = gAmbientLight * diffuseAlbedo;

	const float shininess = 1.0f - gRoughness;
	Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

	float4 litColor = ambient + directLight;

#ifdef FOG
	float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
	litColor = lerp(litColor, gFogColor, fogAmount);
#endif
	
    // Common convention to take alpha from diffuse material.
	litColor.a = diffuseAlbedo.a;

	return litColor;
}