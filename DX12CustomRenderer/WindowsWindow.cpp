#include "WindowsWindow.h"

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

LRESULT WindowsWindow::WndProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
	switch (Message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(Hwnd, &ps);

		// All painting occurs here, between BeginPaint and EndPaint.

		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

		EndPaint(Hwnd, &ps);
	}
	return 0;

	}
	return DefWindowProc(Hwnd, Message, WParam, LParam);
}
