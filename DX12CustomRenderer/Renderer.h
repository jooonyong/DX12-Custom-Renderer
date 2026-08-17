#pragma once

#include "D3D12CommandQueue.h"
#include "D3D12CommandContext.h"
#include "D3D12SwapChain.h"
#include "dxgi1_6.h"
#include "dxcapi.h"

class IDxcBlob;

struct Vertex
{
	float Position[3];
	float Color[4];
};

class Renderer
{
public:
	~Renderer();

	bool Initialize(HWND Hwnd, UINT Width, UINT Height);

	void RenderFrame();

	bool CreateRootSignature();
	bool CreateShaders();
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const wchar_t* FilePath, const wchar_t* EntryPoint, const wchar_t* TargetProfile);

	bool CreatePipelineState();

	bool CreateVertexBuffer();
	bool CreateIndexBuffer();

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

	ID3D12Resource* VertexUploadBuffer = nullptr;
	ID3D12Resource* VertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW VBView;

	ID3D12Resource* IndexUploadBuffer = nullptr;
	ID3D12Resource* IndexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW IBView;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	UINT Width;
	UINT Height;
};