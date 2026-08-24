#pragma once

#include "D3D12CommandQueue.h"
#include "D3D12CommandContext.h"
#include "D3D12SwapChain.h"
#include "Mesh.h"
#include "dxgi1_6.h"
#include "dxcapi.h"
#include "DirectXMath.h"
#include <vector>
#include <memory>
#include <wincodec.h>
#include <wrl.h>

#pragma comment(lib, "windowscodecs.lib")

class IDxcBlob;
class Camera;

struct TransformConstant
{
	DirectX::XMFLOAT4X4 WorldMatrix;
	DirectX::XMFLOAT4X4 ViewMatrix;
	DirectX::XMFLOAT4X4 ProjectionMatrix;
};

class Renderer
{
public:
	~Renderer();

	bool Initialize(HWND Hwnd, UINT Width, UINT Height);

	void RenderFrame(const Camera& MainCamera);

	bool CreateRootSignature();
	bool CreateShaders();
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const wchar_t* FilePath, const wchar_t* EntryPoint, const wchar_t* TargetProfile);

	bool CreatePipelineState();

	bool CreateDefaultBuffer(const void* Data, UINT64 Size, D3D12_RESOURCE_STATES FinalState, Microsoft::WRL::ComPtr<ID3D12Resource>& OutBuffer);
	bool CreateDepthBuffer();

	bool LoadImage(const wchar_t* FilePath, std::vector<uint8_t>& OutPixels, UINT& OutWidth,UINT& OutHeight);
	bool CreateTexture();
	std::unique_ptr<Mesh> CreateMesh(const MeshData& Data);

	void UpdateViewport(UINT Width, UINT Height);

private:
	D3D12Device Device;
	D3D12CommandQueue CommandQueue;
	D3D12CommandContext CommandContext;
	D3D12SwapChain SwapChain;

	FrameResource Frame[BufferCount];

	ID3D12RootSignature* RootSignature = nullptr;

	Microsoft::WRL::ComPtr<IDxcBlob> VertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> PixelShader;

	ID3D12PipelineState* PipelineState = nullptr;
	std::unique_ptr<Mesh> CubeMesh = nullptr;

	ID3D12Resource* DepthBuffer = nullptr;
	ID3D12DescriptorHeap* DSVHeap = nullptr;

	ID3D12Resource* Texture = nullptr;
	ID3D12DescriptorHeap* SRVHeap = nullptr;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	UINT Width;
	UINT Height;
};