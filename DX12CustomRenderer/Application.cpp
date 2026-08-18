#include "Application.h"

bool Application::Initialize(HINSTANCE Instance, int ShowCommand)
{
	if (!Wnd.Initialize(Instance, 1080, 720, L"Custom Renderer"))
	{
		return false;
	}
	
	Wnd.Show(ShowCommand);
	
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&PreviousTime);

	if (!Renderer.Initialize(Wnd.GetHandle(), 1080, 720))
	{
		return false;
	}
	bRunning = true;
	return true;
}

void Application::Run()
{
	while (bRunning)
	{
		LARGE_INTEGER CurrentTime;
		QueryPerformanceCounter(&CurrentTime);

		float DeltaTime = static_cast<float>((CurrentTime.QuadPart - PreviousTime.QuadPart)) / static_cast<float>(Frequency.QuadPart);
		PreviousTime = CurrentTime;

		//OS Event
		ProcessMessages();

		//Scene, Camera 상태 업데이트
		Update(DeltaTime);

		//그리기 관련 RenderCommand
		Renderer.RenderFrame(MainCamera);
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

}

void Application::Update(float DeltaTime)
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
}
