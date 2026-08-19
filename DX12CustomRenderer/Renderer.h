#pragma once

#include "D3D12CommandQueue.h"
#include "D3D12CommandContext.h"
#include "D3D12SwapChain.h"
#include "dxgi1_6.h"
#include "dxcapi.h"
#include "DirectXMath.h"

class IDxcBlob;
class Camera;

struct Vertex
{
	float Position[3];
	float Color[4];
	float UV[2];
};

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

	bool CreateVertexBuffer();
	bool CreateIndexBuffer();
	bool CreateDefaultBuffer(const void* Data, UINT64 Size, D3D12_RESOURCE_STATES FinalState, ID3D12Resource*& OutBuffer);
	bool CreateDepthBuffer();
	bool CreateTexture();

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

	ID3D12Resource* VertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW VBView;

	ID3D12Resource* IndexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW IBView;

	ID3D12Resource* DepthBuffer = nullptr;
	ID3D12DescriptorHeap* DSVHeap = nullptr;

	ID3D12Resource* Texture = nullptr;
	ID3D12DescriptorHeap* SRVHeap = nullptr;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	UINT Width;
	UINT Height;
};