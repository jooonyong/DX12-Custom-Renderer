#pragma once
#include "WindowsWindow.h"
#include "D3D12Device.h"
#include "Renderer.h"
#include "Camera.h"

class Application
{
public:
	Application() = default;

	bool Initialize(HINSTANCE Instance, int ShowCommand);
	void Run();

	void ShutDown();

private:
	void ProcessMessages();
	void Update(float DeltaTime);

private:
	bool bRunning = false;
	WindowsWindow Wnd;
	Renderer Renderer;
	Camera MainCamera;

	LARGE_INTEGER Frequency;
	LARGE_INTEGER PreviousTime;
};