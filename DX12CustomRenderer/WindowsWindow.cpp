#include "WindowsWindow.h"
#include <windowsx.h>

bool WindowsWindow::Initialize(HINSTANCE Instance, UINT32 Width, UINT32 Height, const wchar_t* Title)
{
	this->Instance = Instance;
	this->Width = Width;
	this->Height = Height;
	this->Title = Title;

	if (!RegisterWindowClass())
	{
		return false;
	}

	Hwnd = CreateWindowEx(0, Title, L"CustomRenderer", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, this->Width, this->Height,
		nullptr, nullptr, this->Instance, nullptr);

	if (!Hwnd)
	{
		return false;
	}

	SetWindowLongPtr(Hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	return true;
}

void WindowsWindow::Show(int nCmdShow)
{
	ShowWindow(Hwnd, nCmdShow);
}

HWND WindowsWindow::GetHandle() const
{
	return this->Hwnd;
}

void WindowsWindow::ConsumeMouseDelta(int& DeltaX, int& DeltaY)
{
	DeltaX = MouseDeltaX;
	DeltaY = MouseDeltaY;

	MouseDeltaX = 0;
	MouseDeltaY = 0;
}

bool WindowsWindow::RegisterWindowClass()
{
	WNDCLASS WindowClass = { };

	WindowClass.lpfnWndProc = WndProc;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = Title;

	if (!RegisterClass(&WindowClass))
	{
		return false;
	}

	return true;
}

LRESULT WindowsWindow::HandleMessage(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
	switch (Message)
	{
	case WM_RBUTTONDOWN:
	{
		bRMouseDown = true;
		LastMouseX = GET_X_LPARAM(LParam);
		LastMouseY = GET_Y_LPARAM(LParam);
		SetCapture(Hwnd);

		return 0;
	}
	case WM_RBUTTONUP:
	{
		bRMouseDown = false;
		ReleaseCapture();

		return 0;
	}
	case WM_MOUSEMOVE:
	{
		const int MouseX = GET_X_LPARAM(LParam);
		const int MouseY = GET_Y_LPARAM(LParam);
		
		if (bRMouseDown)
		{
			MouseDeltaX += MouseX - LastMouseX;
			MouseDeltaY += MouseY - LastMouseY;

			LastMouseX = MouseX;
			LastMouseY = MouseY;
		}

		return 0;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProc(Hwnd, Message, WParam, LParam);
}

LRESULT WindowsWindow::WndProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
	WindowsWindow* Window = nullptr;
	Window = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(Hwnd, GWLP_USERDATA));
	if (Window)
	{
		return Window->HandleMessage(Hwnd, Message, WParam, LParam);
	}
	else
	{
		return DefWindowProc(Hwnd, Message, WParam, LParam);
	}
}
