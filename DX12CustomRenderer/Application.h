#pragma once
#include "WindowsWindow.h"
#include "D3D12Device.h"
#include "Scene.h"
#include "Renderer.h"
#include "Camera.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

struct RenderFrameData
{
	Scene MainScene;
	Camera MainCamera;
	RenderMode Mode = RenderMode::E_Raster;
	uint64_t FrameNumber = 0;
};

class Application
{
public:
	Application() = default;

	bool Initialize(HINSTANCE Instance, int ShowCommand);
	void Run();

	void ShutDown();
	
	RenderFrameData BuildRenderFrameData() const;
	bool SubmitRenderFrame(RenderFrameData FrameData);

	void RenderThreadMain();

private:
	void ProcessMessages();
	void Update(float DeltaTime);

	void StartRenderThread();
	void StopRenderThread();

private:
	bool bRunning = false;
	WindowsWindow Wnd;
	Scene MainScene;
	Renderer Renderer;
	Camera MainCamera;
	RenderMode RequestedMode = RenderMode::E_Raster;

	LARGE_INTEGER Frequency;
	LARGE_INTEGER PreviousTime;

	std::thread RenderThread;
	std::mutex FrameQueueMutex;
	std::condition_variable FrameAvailable;
	std::queue<RenderFrameData> FrameQueue;

	bool bStopRenderThread = false; // FrameQueueMutex·Î º¸È£
	static constexpr size_t MaxQueuedFrames = 2;

	uint64_t NextFrameNumber = 0;
};
