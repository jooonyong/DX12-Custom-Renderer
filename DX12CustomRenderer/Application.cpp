#include "Application.h"

bool Application::Initialize(HINSTANCE Instance, int ShowCommand)
{
	if (!Wnd.Initialize(Instance, 1920, 1080, L"Custom Renderer"))
	{
		return false;
	}
	Wnd.Show(ShowCommand);

	bRunning = true;
	return true;
}

void Application::Run()
{
	while (bRunning)
	{
		ProcessMessages();
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
