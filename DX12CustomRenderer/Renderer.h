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
	bool Initialize(HWND Hwnd, UINT Width, UINT Height);

	void RenderFrame();

	bool CreateRootSignature();
	bool CreateShaders();
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const wchar_t* FilePath, const wchar_t* EntryPoint, const wchar_t* TargetProfile);

	bool CreatePipelineState();

	bool CreateVertexBuffer();

	void UpdateViewport(UINT Width, UINT Height);

private:
	D3D12Device Device;
	D3D12CommandQueue CommandQueue;
	D3D12CommandContext CommandContext;
	D3D12SwapChain SwapChain;

	FrameResource Frame[BufferCount];

	ID3D12RootSignature* RootSignature;

	Microsoft::WRL::ComPtr<IDxcBlob> VertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> PixelShader;

	ID3D12PipelineState* PipelineState;

	ID3D12Resource* VertexUploadBuffer;
	ID3D12Resource* VertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW VBView;


	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	UINT Width;
	UINT Height;
};