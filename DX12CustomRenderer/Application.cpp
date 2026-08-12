#include "Application.h"

bool Application::Initialize(HINSTANCE Instance, int ShowCommand)
{
	if (!Wnd.Initialize(Instance, 1080, 720, L"Custom Renderer"))
	{
		return false;
	}
	Wnd.Show(ShowCommand);

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
		ProcessMessages();
		Renderer.RenderFrame();
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
