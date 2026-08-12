#pragma once
#include "WindowsWindow.h"
#include "D3D12Device.h"

class Application
{
public:
	Application() = default;

	bool Initialize(HINSTANCE Instance, int ShowCommand);
	void Run();

	void ShutDown();

private:
	void ProcessMessages();

private:
	bool bRunning = false;
	WindowsWindow Wnd;
};