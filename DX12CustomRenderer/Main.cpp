#include "WindowsWindow.h"
#include "Application.h"

using namespace std;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	Application App;
	if (!App.Initialize(hInstance, nCmdShow))
	{
		return 0;
	}
	App.Run();

	return 0;
}
