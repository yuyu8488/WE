#include "D2dEngine.h"

D2dEngine::D2dEngine() : m_hwnd(nullptr), m_pD2dFactory(nullptr), m_pRenderTarget(nullptr), m_pLightSlateGrayBrush(nullptr),m_pCornFlowerBlueBrush(nullptr)
{
}

D2dEngine::~D2dEngine()
{
	SafeRelease(&m_pD2dFactory);
	SafeRelease(&m_pRenderTarget);
	SafeRelease(&m_pLightSlateGrayBrush);
	SafeRelease(&m_pCornFlowerBlueBrush);
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

		m_hwnd = CreateWindow(
			L"D2dEngine",
			L"Direct2D Engine",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			nullptr,
			nullptr,
			HINST_THISCOMPONENT,
			this);

		if (m_hwnd)
		{
			float dpi = GetDpiForWindow(m_hwnd);

			SetWindowPos(
				m_hwnd,
				NULL,
				NULL,
				NULL,
				static_cast<int>(ceil(640.f * dpi / 96.f)),
				static_cast<int>(ceil(480.f * dpi / 96.f)),
				SWP_NOMOVE);

			ShowWindow(m_hwnd, SW_SHOWNORMAL);
			UpdateWindow(m_hwnd);
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
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2dFactory);
	return hr;	
}

HRESULT D2dEngine::CreateDeviceResources()
{
	HRESULT hr = S_OK;

	if (!m_pRenderTarget)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
	
	
		// Create Direct2D render Target
		hr = m_pD2dFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(m_hwnd, size),
			&m_pRenderTarget);

		if (SUCCEEDED(hr))
		{
			hr = m_pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::LightSlateGray),
				&m_pLightSlateGrayBrush);
		}
		if (SUCCEEDED(hr))
		{
			hr = m_pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::CornflowerBlue),
				&m_pCornFlowerBlueBrush);
		}
	}
	return hr;
}

void D2dEngine::DiscardDeviceResources()
{
	SafeRelease(&m_pRenderTarget);
	SafeRelease(&m_pLightSlateGrayBrush);
	SafeRelease(&m_pCornFlowerBlueBrush);
}

HRESULT D2dEngine::OnRender()
{
	HRESULT hr = S_OK;
	hr = CreateDeviceResources();

	if (SUCCEEDED(hr))
	{
		m_pRenderTarget->BeginDraw();
		m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
		m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

		
		// Draw a grid background.
		D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();
		int width = static_cast<int>(rtSize.width);
		int height = static_cast<int>(rtSize.height);

		for (int x = 0; x < width; x += 10)
		{
			m_pRenderTarget->DrawLine(
				D2D1::Point2F(static_cast<FLOAT>(x), 0.0f),
				D2D1::Point2F(static_cast<FLOAT>(x), rtSize.height),
				m_pLightSlateGrayBrush,
				0.5f
				);
		}
		for (int y = 0; y < height; y += 10)
		{
			m_pRenderTarget->DrawLine(
				D2D1::Point2F(0.0f, static_cast<FLOAT>(y)),
				D2D1::Point2F(rtSize.width, static_cast<FLOAT>(y)),
				m_pLightSlateGrayBrush,
				0.5f
				);
		}

		// Draw two rectangles.
		D2D1_RECT_F rectangle1 = D2D1::RectF(
			rtSize.width/2 - 50.0f,
			rtSize.height/2 - 50.0f,
			rtSize.width/2 + 50.0f,
			rtSize.height/2 + 50.0f
			);

		D2D1_RECT_F rectangle2 = D2D1::RectF(
			rtSize.width/2 - 100.0f,
			rtSize.height/2 - 100.0f,
			rtSize.width/2 + 100.0f,
			rtSize.height/2 + 100.0f
			);
		
		m_pRenderTarget->FillRectangle(&rectangle1, m_pLightSlateGrayBrush);
		m_pRenderTarget->DrawRectangle(&rectangle2, m_pCornFlowerBlueBrush);

		hr = m_pRenderTarget->EndDraw();
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
	if (m_pRenderTarget)
	{
		m_pRenderTarget->Resize(D2D1::SizeU(width, height));
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
		D2dEngine* D2d = reinterpret_cast<D2dEngine*>(static_cast<LONG_PTR>(GetWindowLongPtrW(hWnd, GWLP_USERDATA)));
		bool bHandled = false;

		if (D2d)
		{
			switch (message)
			{
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
