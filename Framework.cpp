#include "Framework.h"

#include "Engine/Utility/ComUtils.h"
#include "Object/Box.h"

Framework::Framework() : WindowHandle(nullptr), D2DFactory(nullptr), RenderTarget(nullptr), LightSlateGrayBrush(nullptr),CornFlowerBlueBrush(nullptr)
{
	UBox* PlayerBox = new UBox(10.f, 10.f, 10.f, 10.f);
	AddObject(PlayerBox);
}

Framework::~Framework()
{
	SafeRelease(&D2DFactory);
	SafeRelease(&RenderTarget);
	SafeRelease(&LightSlateGrayBrush);
	SafeRelease(&CornFlowerBlueBrush);
	
	Objects.clear();
}

HRESULT Framework::Initialize()
{
	HRESULT hr = CreateDeviceIndependentResources();

	if (SUCCEEDED(hr))
	{
		WNDCLASSEX wcex = {sizeof(WNDCLASSEX)};
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = Framework::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = sizeof(LONG_PTR);
		wcex.hInstance = HINST_THISCOMPONENT;
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDI_APPLICATION);
		wcex.lpszClassName = L"Framework";

		RegisterClassEx(&wcex);

		WindowHandle = CreateWindow(
			L"Framework",
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
			
			RECT rc;
			GetClientRect(WindowHandle, &rc);
			hr = D3D11.Initialize(WindowHandle, rc.right - rc.left, rc.bottom - rc.top);
			if (FAILED(hr))
			{
				return hr;
			}
			
			ShowWindow(WindowHandle, SW_SHOWNORMAL);
			UpdateWindow(WindowHandle);
		}
	}
	return hr;
}

void Framework::RunMessageLoop()
{
	MSG msg;

	while (GetMessage(&msg, nullptr, 0,0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

HRESULT Framework::CreateDeviceIndependentResources()
{
	HRESULT hr = S_OK;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2DFactory);
	return hr;	
}

HRESULT Framework::CreateDeviceResources(int Width, int Height)
{
	HRESULT hr = S_OK;

	if (!RenderTarget)
	{
		IDXGISurface* BackBufferSurface = D3D11.GetBackBufferSurface();
		if (BackBufferSurface)
		{
			float Dpi = static_cast<float>(GetDpiForWindow(WindowHandle));

			D2D1_RENDER_TARGET_PROPERTIES RenderTargetProperties = D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
				Dpi,
				Dpi);

			hr = D2DFactory->CreateDxgiSurfaceRenderTarget(
				BackBufferSurface,
				&RenderTargetProperties,
				&RenderTarget);

			if (SUCCEEDED(hr))
			{
				hr = RenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightSlateGray), &LightSlateGrayBrush);
				hr = RenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::CornflowerBlue), &CornFlowerBlueBrush);
			}
		}
	}
	return hr;
}

void Framework::DiscardDeviceResources()
{
	SafeRelease(&RenderTarget);
	SafeRelease(&LightSlateGrayBrush);
	SafeRelease(&CornFlowerBlueBrush);
}

HRESULT Framework::OnRender()
{
	RECT rc;
	GetClientRect(WindowHandle, &rc);
	HRESULT hr = CreateDeviceResources(rc.right - rc.left, rc.bottom - rc.top);
	
	if (SUCCEEDED(hr))
	{
		D3D11.BeginRender();
		RenderTarget->BeginDraw();
		RenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
		
		// Draw Grid
		float GridWidth = RenderTarget->GetSize().width;
		float GridHeight = RenderTarget->GetSize().height;
		float GridSize = Grid::CellSize;
		
		for (float x = 0; x <= GridWidth; x += GridSize)
		{
			RenderTarget->DrawLine(
				D2D1::Point2F(x, 0.0f),
				D2D1::Point2F(x, GridHeight),
				LightSlateGrayBrush,
				.5f
				);
		}
		for (float y = 0; y <= GridHeight; y += GridSize)
		{
			RenderTarget->DrawLine(
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
				Object->Render(RenderTarget, LightSlateGrayBrush);
			}
		}
		
		hr = RenderTarget->EndDraw();
		D3D11.EndRender();
	}
	
	if (hr == D2DERR_RECREATE_TARGET)
	{
		hr = S_OK;
		DiscardDeviceResources();
	}

	return hr;
}

void Framework::OnResize(UINT width, UINT height)
{
	if (RenderTarget)
	{
		//WindowHandleRenderTarget->Resize(D2D1::SizeU(width, height));
	}
}

LRESULT Framework::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;

	if (message == WM_CREATE)
	{
		LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
		Framework* D2d = (Framework*)pcs->lpCreateParams;

		SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(D2d));

		result = 1;
	}
	else
	{
		Framework* D2d = reinterpret_cast<Framework*>((GetWindowLongPtrW(hWnd, GWLP_USERDATA)));
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
