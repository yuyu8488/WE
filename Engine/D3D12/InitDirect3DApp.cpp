/*#include "D3d12App.h"
#include <DirectXColors.h>

class InitDirect3DApp : public D3d12App
{
public:
	InitDirect3DApp(HINSTANCE hInstance);
	virtual ~InitDirect3DApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	InitDirect3DApp App(hInstance);
	if (!App.Initialize())
	{
		return 0;
	}
	return App.Run();
}

InitDirect3DApp::InitDirect3DApp(HINSTANCE hInstance) : D3d12App(hInstance)
{
}

InitDirect3DApp::~InitDirect3DApp()
{
}

bool InitDirect3DApp::Initialize()
{
	if (!D3d12App::Initialize())
	{
		return false;
	}
	return true;
}

void InitDirect3DApp::OnResize()
{
	D3d12App::OnResize();
}

void InitDirect3DApp::Update(const GameTimer& gt)
{
}

void InitDirect3DApp::Draw(const GameTimer& gt)
{
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	auto Barrier = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), 
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ResourceBarrier(1, &Barrier);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto BackBufferView = GetCurrentBackBufferView();
	mCommandList->ClearRenderTargetView(BackBufferView, DirectX::Colors::LightSteelBlue, 0, nullptr);

	auto DepthStencilView = GetDepthStencilView();
	mCommandList->ClearDepthStencilView(DepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &BackBufferView, true, &DepthStencilView);

	Barrier = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(1, &Barrier);

	mCommandList->Close();

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}