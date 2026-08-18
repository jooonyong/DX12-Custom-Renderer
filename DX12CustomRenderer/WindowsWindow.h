#pragma once
#include "Windows.h"

class WindowsWindow
{
public:
	WindowsWindow() = default;

	bool Initialize(HINSTANCE Instance, UINT32 Width, UINT32 Height, const wchar_t* Title);

	void Show(int nCmdShow);
	HWND GetHandle() const;

	static LRESULT CALLBACK WndProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam);

	void ConsumeMouseDelta(int& DeltaX, int& DeltaY);
private:
	bool RegisterWindowClass();
	LRESULT HandleMessage(HWND Hwnd, UINT Message, WPARAM WParam,LPARAM LParam);

private:
	HINSTANCE Instance = nullptr;
	HWND Hwnd = nullptr;

	UINT32 Width = 0;
	UINT32 Height = 0;

	int LastMouseX = 0;
	int LastMouseY = 0;

	int MouseDeltaX = 0;
	int MouseDeltaY = 0;

	bool bRMouseDown = false;
	const wchar_t* Title = L"DX12RendererWindow";
};