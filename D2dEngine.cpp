#include "D2dEngine.h"

#include "Box.h"

D2dEngine::D2dEngine() : WindowHandle(nullptr), D2DFactory(nullptr), WindowHandleRenderTarget(nullptr), LightSlateGrayBrush(nullptr),CornFlowerBlueBrush(nullptr)
{
	UBox* PlayerBox = new UBox(10.f, 10.f, 10.f, 10.f);
	AddObject(PlayerBox);
}

D2dEngine::~D2dEngine()
{
	SafeRelease(&D2DFactory);
	SafeRelease(&WindowHandleRenderTarget);
	SafeRelease(&LightSlateGrayBrush);
	SafeRelease(&CornFlowerBlueBrush);
	
	Objects.clear();
}

HRESULT D2dEngine::Initialize()
{
	HRESULT hr = CreateDeviceIndependentResources();

	if (SUCCEEDED(hr))
	{
		WNDCLASSEX wcex = {sizeof(WNDCLASSEX)};
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = D2dEngine::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = sizeof(LONG_PTR);
		wcex.hInstance = HINST_THISCOMPONENT;
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDI_APPLICATION);
		wcex.lpszClassName = L"D2dEngine";

		RegisterClassEx(&wcex);

		WindowHandle = CreateWindow(
			L"D2dEngine",
			L"WE",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			nullptr,
			nullptr,
			HINST_THISCOMPONENT,
			this);

		if (WindowHandle)
		{
			float dpi = static_cast<float>(GetDpiForWindow(WindowHandle));

			SetWindowPos(
				WindowHandle,
				NULL,
				NULL,
				NULL,
				static_cast<int>(ceil(640.f * dpi / 96.f)),
				static_cast<int>(ceil(480.f * dpi / 96.f)),
				SWP_NOMOVE);
			
			ShowWindow(WindowHandle, SW_SHOWNORMAL);
			UpdateWindow(WindowHandle);
		}
	}
	return hr;
}

void D2dEngine::RunMessageLoop()
{
	MSG msg;

	while (GetMessage(&msg, nullptr, 0,0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

HRESULT D2dEngine::CreateDeviceIndependentResources()
{
	HRESULT hr = S_OK;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2DFactory);
	return hr;	
}

HRESULT D2dEngine::CreateDeviceResources()
{
	HRESULT hr = S_OK;

	if (!WindowHandleRenderTarget)
	{
		RECT rc;
		GetClientRect(WindowHandle, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
		
		// Create Direct2D render Target
		hr = D2DFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(WindowHandle, size),
			&WindowHandleRenderTarget);
		
		if (SUCCEEDED(hr))
		{
			hr = WindowHandleRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::LightSlateGray),
				&LightSlateGrayBrush);

			hr = WindowHandleRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::CornflowerBlue),
				&CornFlowerBlueBrush);
		}
	}
	return hr;
}

void D2dEngine::DiscardDeviceResources()
{
	SafeRelease(&WindowHandleRenderTarget);
	SafeRelease(&LightSlateGrayBrush);
	SafeRelease(&CornFlowerBlueBrush);
}

HRESULT D2dEngine::OnRender()
{
	HRESULT hr = S_OK;
	hr = CreateDeviceResources();

	if (SUCCEEDED(hr))
	{
		WindowHandleRenderTarget->BeginDraw();
		WindowHandleRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
		WindowHandleRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
		
		// Draw Grid
		float GridWidth = WindowHandleRenderTarget->GetSize().width;
		float GridHeight = WindowHandleRenderTarget->GetSize().height;
		float GridSize = Grid::CellSize;
		
		for (float x = 0; x <= GridWidth; x += GridSize)
		{
			WindowHandleRenderTarget->DrawLine(
				D2D1::Point2F(x, 0.0f),
				D2D1::Point2F(x, GridHeight),
				LightSlateGrayBrush,
				.5f
				);
		}
		for (float y = 0; y <= GridHeight; y += GridSize)
		{
			WindowHandleRenderTarget->DrawLine(
				D2D1::Point2F(0.0f, y),
				D2D1::Point2F(GridWidth, y),
				LightSlateGrayBrush,
				.5f
				);
		}
		
		// Draw Objects
		if (!Objects.empty())
		{
			for (auto& Object : Objects)
			{
				Object->Render(WindowHandleRenderTarget, LightSlateGrayBrush);
			}
		}
		
		hr = WindowHandleRenderTarget->EndDraw();
	}
	
	if (hr == D2DERR_RECREATE_TARGET)
	{
		hr = S_OK;
		DiscardDeviceResources();
	}

	return hr;
}

void D2dEngine::OnResize(UINT width, UINT height)
{
	if (WindowHandleRenderTarget)
	{
		WindowHandleRenderTarget->Resize(D2D1::SizeU(width, height));
	}
}

LRESULT D2dEngine::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;

	if (message == WM_CREATE)
	{
		LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
		D2dEngine* D2d = (D2dEngine*)pcs->lpCreateParams;

		SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(D2d));

		result = 1;
	}
	else
	{
		D2dEngine* D2d = reinterpret_cast<D2dEngine*>((GetWindowLongPtrW(hWnd, GWLP_USERDATA)));
		bool bHandled = false;

		if (D2d)
		{
			switch (message)
			{
			case WM_KEYDOWN:
				{
					constexpr float MoveSpeed = 10.f;
					if ((D2d->Objects[0] == nullptr)) break;
						
					switch (wParam)
					{
					case 'W':
						dynamic_cast<UBox*>(D2d->Objects[0])->Move(0, -MoveSpeed);
						break;
					case 'A':
						dynamic_cast<UBox*>(D2d->Objects[0])->Move(-MoveSpeed, 0);
						break;
					case 'S':
						dynamic_cast<UBox*>(D2d->Objects[0])->Move(0, MoveSpeed);
						break;
					case 'D':
						dynamic_cast<UBox*>(D2d->Objects[0])->Move(MoveSpeed, 0);
						break;						
					}
					InvalidateRect(hWnd, nullptr, FALSE);
				}
				break;
			case WM_SIZE:
				{
					UINT width = LOWORD(lParam);
					UINT height = HIWORD(lParam);
					D2d->OnResize(width, height);
				}
				result = 0;
				bHandled = true;
				break;
			case WM_DISPLAYCHANGE:
				{
					InvalidateRect(hWnd, nullptr, FALSE);
				}
				result = 0;
				bHandled = true;
				break;
			case WM_PAINT:
				{
					D2d->OnRender();
					ValidateRect(hWnd, nullptr);
				}
				result = 0;
				bHandled = true;
				break;
			case WM_DESTROY:
				{
					PostQuitMessage(0);
				}
				result = 0;
				bHandled = true;
				break;
			}
		}

		if (!bHandled)
		{
			result = DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	return result;
}
