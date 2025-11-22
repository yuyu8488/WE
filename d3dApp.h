#pragma once

#include "d3dUtil.h"
#include "config.h"
#include "GeometryGenerator.h"
#include "FrameResource.h"
#include "MathHelper.h"
#include "Material.h"
#include "GameTimer.h"
#include "Texture.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")



struct RenderItem
{
public:
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	int NumFramesDirty = NUM_FRAME_RESOURCES;
	
	UINT ObjectCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};

class D3DApp
{
public:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();

public:
	static D3DApp* GetApp();

	HINSTANCE AppInstance() const;
	HWND MainWindow() const;
	float AspectRatio() const;

	bool Get4xMsaaState() const;
	void Set4xMsaaState(bool Value);

	bool Initialize();
	int Run();

	LRESULT CALLBACK MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
protected:
	void CreateRtvAndDsvDescriptorHeaps();

	void OnResize();
	void Update(const GameTimer& gt);
	void Draw(const GameTimer& gt);
	
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);

	bool InitMainWindow();

	bool InitDirect3D();
	void CreateCommandObjects();
	void CreateSwapChain();
	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void CalculateFrameStats();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* Adapter);
	void LogOutputDisplayModes(IDXGIOutput* Output, DXGI_FORMAT Format);

protected:
	//***************************************************************************************
	// Initialize D3D12 Member variables
	//***************************************************************************************
	static D3DApp* mApp;

	HINSTANCE mhAppInst = nullptr;
	HWND mhMainWnd = nullptr;
	bool mAppPaused = false;
	bool mMinimized = false;
	bool mMaximized = false;
	bool mResizing = false;
	bool mFullScreenState = false;
	
	bool m4xMsaaState = false;
	UINT m4xMsaaQuality = 0;

	GameTimer mTimer;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;
	
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;
	
	UINT RtvDescriptorSize = 0;
	UINT DsvDescriptorSize = 0;
	UINT CbvSrvUavDescriptorSize = 0;

	// Derived class should set these in derived constructor to customize starting values.
	std::wstring mMainWndCaption = L"d3d App";
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 800;
	int mClientHeight = 600;

private:
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShaderAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& RenderItems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	std::vector<std::unique_ptr<FrameResource>> FrameResources;
	FrameResource* CurrentFrameResource = nullptr;
	int CurrentFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature = nullptr;
	//Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CbvDescriptorHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> Geometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> Materials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> Shaders;

	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO;

	// List of all render items;
	std::vector<std::unique_ptr<RenderItem>> AllRenderItems;

	// Render items divided by PSO
	std::vector<RenderItem*> RenderItems;

	PassConstants MainPassCB;
	UINT PassCbvOffset = 0;

protected:
	DirectX::XMFLOAT3 mEyePos = { 0.f, 0.f, 0.f };
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	bool mIsWireframe = false;
	float mTheta = 1.3f * DirectX::XM_PI;
	float mPhi = 0.4f * DirectX::XM_PI;
	float mRadius = 2.5f;

	float mSunTheta = 1.25f * DirectX::XM_PI;
	float mSunPhi = DirectX::XM_PIDIV4;

	POINT mLastMousePos;
};