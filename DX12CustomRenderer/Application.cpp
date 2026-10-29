#include "Application.h"
#include "GeometryGenerator.h"


bool Application::Initialize(HINSTANCE Instance, int ShowCommand)
{
	if (!Wnd.Initialize(Instance, 1920, 1080, L"Custom Renderer"))
	{
		return false;
	}
	
	Wnd.Show(ShowCommand);
	
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&PreviousTime);

	if (!Renderer.Initialize(Wnd.GetHandle(), 1920, 1080))
	{
		return false;
	}

	auto Model = Renderer.CreateRenderModel("Assets/AK/ak12.gltf");
	if (!Model)
	{
		return false;
	}

	ModelData CubeData{};
	CubeData.Mesh = GeometryGenerator::CreateCube();
	
	SubMeshData CubeSubmesh{};
	CubeSubmesh.IndexStart = 0;
	CubeSubmesh.IndexCount = static_cast<uint32_t>(CubeData.Mesh.Indices.size());
	CubeSubmesh.MaterialIndex = 0;
	CubeData.SubMeshes.push_back(CubeSubmesh);

	MaterialData CubeMaterial{};
	CubeMaterial.BaseColor = { 0.5f, 0.5f, 0.5f, 1.0f };
	CubeMaterial.Roughness = 0.1f;
	CubeMaterial.Metallic = 0.0f;
	CubeData.Materials.push_back(CubeMaterial);

	auto CubeModel = Renderer.CreateRenderModel(CubeData);

	RenderObject Object1;
	Object1.Model = Model;
	DirectX::XMStoreFloat4x4(&Object1.World, DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f) * DirectX::XMMatrixTranslation(-5.0f, 3.0f, -2.0f));

	RenderObject Object2;
	Object2.Model = Model;
	DirectX::XMStoreFloat4x4(&Object2.World, DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f) * DirectX::XMMatrixTranslation(-2.0f, 3.0f, 5.0f));
	
	RenderObject Object3;
	Object3.Model = CubeModel;
	DirectX::XMStoreFloat4x4(&Object3.World, DirectX::XMMatrixScaling(32.0f, 0.1f, 32.0f) * DirectX::XMMatrixTranslation(0.0f, -0.05f, 0.0f));

	RenderObject Object4;
	Object4.Model = CubeModel;
	DirectX::XMStoreFloat4x4(&Object4.World, DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f) * DirectX::XMMatrixTranslation(0.0f, 1.0, 0.0f));

	MainScene.AddRenderObject(Object1);
	MainScene.AddRenderObject(Object2);
	MainScene.AddRenderObject(Object3);
	MainScene.AddRenderObject(Object4);

	bRunning = true;
	return true;
}

void Application::Run()
{
	StartRenderThread();
	
	try{
		QueryPerformanceCounter(&PreviousTime);
		while (bRunning)
		{
			LARGE_INTEGER CurrentTime;
			QueryPerformanceCounter(&CurrentTime);
			float DeltaTime = static_cast<float>((CurrentTime.QuadPart - PreviousTime.QuadPart)) / static_cast<float>(Frequency.QuadPart);
			PreviousTime = CurrentTime;

			//OS Event
			ProcessMessages();
			if (!bRunning)
			{
				break;
			}
			//Scene, Camera 상태 업데이트
			Update(DeltaTime);

			UpdateWindowTitle(CurrentTime);

			RenderFrameData RenderThreadData = BuildRenderFrameData();
			bool bSubmitted = SubmitRenderFrame(std::move(RenderThreadData));
			if (!bSubmitted)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			NextFrameNumber++;
			//그리기 관련 RenderCommand
			//Renderer.RenderFrame(RenderThreadData.MainScene, RenderThreadData.MainCamera);
		}
	}
	catch (const std::exception& Error) {
		StopRenderThread();
		throw;
	}
	StopRenderThread();
	DestroyWindow(Wnd.GetHandle());
}

RenderFrameData Application::BuildRenderFrameData() const
{
	RenderFrameData RenderThreadData{};
	RenderThreadData.MainCamera = this->MainCamera;
	RenderThreadData.MainScene = this->MainScene;
	RenderThreadData.Mode = this->RequestedMode;
	RenderThreadData.FrameNumber = NextFrameNumber;

	return RenderThreadData;
}

bool Application::SubmitRenderFrame(RenderFrameData FrameData)
{
	{
		std::unique_lock<std::mutex> Lock(FrameQueueMutex);

		if (bStopRenderThread || FrameQueue.size() >= MaxQueuedFrames)
		{
			return false;
		}

		FrameQueue.push(std::move(FrameData));
	} // Lock 소멸 → mutex 잠금 해제

	FrameAvailable.notify_one();

	return true;
}

void Application::RenderThreadMain()
{
	while (true)
	{
		RenderFrameData FrameData{};
		{
			std::unique_lock<std::mutex> Lock(FrameQueueMutex);

			FrameAvailable.wait(Lock, [this]
				{
					return bStopRenderThread || !FrameQueue.empty();
				});

			if (bStopRenderThread)
			{
				return;
			}
			FrameData = std::move(FrameQueue.front());
			FrameQueue.pop();
		} // mutex 잠금 해제

		// FrameData의 Scene과 Camera로 렌더링
		Renderer.SetRenderMode(FrameData.Mode);
		Renderer.RenderFrame(FrameData.MainScene, FrameData.MainCamera);
	}
}

void Application::ShutDown()
{
	return;
}


void Application::ProcessMessages()
{
	MSG Message = {};

	while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
	{
		if (Message.message == WM_QUIT)
		{
			bRunning = false;
		}

		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	if (Wnd.IsCloseRequested())
	{
		bRunning = false;
	}
}

void Application::Update(float& DeltaTime)
{
	const float MoveSpeed = 3.0f;

	Angle += MoveSpeed * DeltaTime;
	if (GetAsyncKeyState('W') & 0x8000)
	{
		MainCamera.MoveForward(MoveSpeed * DeltaTime);
	}
	if (GetAsyncKeyState('S') & 0x8000)
	{
		MainCamera.MoveForward(-MoveSpeed * DeltaTime);
	}
	if (GetAsyncKeyState('D') & 0x8000)
	{
		MainCamera.MoveRight(MoveSpeed * DeltaTime);
	}
	if (GetAsyncKeyState('A') & 0x8000)
	{
		MainCamera.MoveRight(-MoveSpeed * DeltaTime);
	}
	if (GetAsyncKeyState('Q') & 0x8000)
	{
		MainCamera.MoveUp(-MoveSpeed * DeltaTime);
	}
	if (GetAsyncKeyState('E') & 0x8000)
	{
		MainCamera.MoveUp(MoveSpeed * DeltaTime);
	}

	int DeltaX;
	int DeltaY;
	Wnd.ConsumeMouseDelta(DeltaX, DeltaY);

	const float MouseSensitivity = DirectX::XMConvertToRadians(0.1f);
	MainCamera.AddRotation(DeltaX * MouseSensitivity, -DeltaY * MouseSensitivity);

	if (GetAsyncKeyState('1') & 0x8000)
	{
		RequestedMode = RenderMode::E_Raster;
	}
	if (GetAsyncKeyState('2') & 0x8000)
	{
		RequestedMode = RenderMode::E_Raytracing;
	}
	if (GetAsyncKeyState('3') & 0x8000)
	{
		RequestedMode = RenderMode::E_Hybrid;
	}
}

void Application::UpdateWindowTitle(LARGE_INTEGER& CurrentTime)
{
	float FPSDelta = static_cast<float>((CurrentTime.QuadPart - PreviousTimeForFrameCount.QuadPart)) / static_cast<float>(Frequency.QuadPart);
	if (FPSDelta < 0.1f)
	{
		return;
	}
   
	uint64_t CurrentCount = Renderer.GetPresentedFrameCount();
	float FPS = (CurrentCount - PreviousPresentedFrameCount) / FPSDelta;

	wchar_t TitleBuffer[128]{};
	swprintf_s(TitleBuffer, _countof(TitleBuffer), L"CustomRenderer | FPS: %.1f", FPS);
	Wnd.SetTitle(TitleBuffer);

	PreviousPresentedFrameCount = CurrentCount;
	QueryPerformanceCounter(&PreviousTimeForFrameCount);
}

void Application::StartRenderThread()
{
	//RenderThread.joinable()이면 바로 반환
	if (RenderThread.joinable())
	{
		return;
	}

	{
		std::lock_guard<std::mutex> Lock(FrameQueueMutex);

		//종료 요청 플래그를 false로 설정
		bStopRenderThread = false;
	}
	// 새 std::thread를 생성해서 RenderThread에 이동 대입
	// 새 스레드는 이 Application의 RenderThreadMain() 실행
	RenderThread = std::move(std::thread([this] {
		RenderThreadMain();
		})
	);
}

void Application::StopRenderThread()
{
	if (!RenderThread.joinable())
	{
		return;
	}

	{
		std::lock_guard<std::mutex> Lock(FrameQueueMutex);

		// 종료 요청 플래그를 true로 설정
		bStopRenderThread = true;
	}

	// FrameAvailable로 대기 중인 렌더 스레드를 깨우기
	FrameAvailable.notify_one();

	// Windows / MSVC용: 스레드 종료와 창 메시지를 함께 기다림
	HANDLE ThreadHandle = RenderThread.native_handle();

	while (true)
	{
		DWORD Result = MsgWaitForMultipleObjectsEx(
			1,
			&ThreadHandle,
			INFINITE,
			QS_ALLINPUT,
			MWMO_INPUTAVAILABLE);

		if (Result == WAIT_OBJECT_0)
		{
			break; // 렌더 스레드 실행 종료
		}

		if (Result == WAIT_OBJECT_0 + 1)
		{
			ProcessMessages();
			continue;
		}

		OutputDebugStringA("Render thread exit wait failed.\n");
		std::terminate();
	}

	// TODO: RenderThread.join() 호출
	RenderThread.join();
}
