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
		Renderer.RenderFrame(MainScene, MainCamera);
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

	if (GetAsyncKeyState('1') & 0x8000)
	{
		Renderer.SetRenderMode(RenderMode::E_Raster);
	}
	if (GetAsyncKeyState('2') & 0x8000)
	{
		Renderer.SetRenderMode(RenderMode::E_Raytracing);
	}
	if (GetAsyncKeyState('3') & 0x8000)
	{
		Renderer.SetRenderMode(RenderMode::E_Hybrid);
	}
}
