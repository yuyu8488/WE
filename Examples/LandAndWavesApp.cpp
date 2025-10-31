#pragma once
#include "..\Engine\D3D12\d3dApp.h"
#include "..\Engine\Common\GeometryGenerator.h"
#include "../Engine/D3D12/d3dUtil.h"
using namespace DirectX;

class LandAndWavesApp : public d3dApp
{
public:
	struct Vertex
	{
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	LandAndWavesApp(HINSTANCE hInstance) : d3dApp(hInstance) {}

private:
	void BuildLandGeometry();

	float GetHillsHeight(float x, float z) const;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
};

void LandAndWavesApp::BuildLandGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.f, 160.f, 50, 50);

	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z);

		if (vertices[i].Pos.y < -10.f)
		{
			vertices[i].Color = XMFLOAT4(1.f, 0.96f, 0.62f, 1.f);
		}
		else if (vertices[i].Pos.y < 5.f)
		{
			vertices[i].Color = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.f);
		}
		else if (vertices[i].Pos.y < 12.f)
		{
			vertices[i].Color = XMFLOAT4(0.1f, 0.48f, 0.19f, 1.f);
		}
		else if (vertices[i].Pos.y < 20.f)
		{
			vertices[i].Color = XMFLOAT4(0.45f, 0.39f, 0.34f, 1.f);
		}
		else
		{
			vertices[i].Color = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
		}
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;
	
	SubmeshGeometry subMesh;
	subMesh.IndexCount = (UINT)indices.size();
	subMesh.StartIndexLocation = 0;
	subMesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = subMesh;
	mGeometries["landGeo"] = std::move(geo);
}

float LandAndWavesApp::GetHillsHeight(float x, float z) const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}
