#pragma once

#include "D3D12CommandQueue.h"
#include "D3D12CommandContext.h"
#include "D3D12SwapChain.h"
#include "D3D12ResourceUploader.h"
#include "D3D12DescriptorAllocator.h"
#include "Mesh.h"
#include "Texture.h"
#include "GLTFLoader.h"
#include "ImageLoader.h"
#include "dxgi1_6.h"
#include "dxcapi.h"
#include "DirectXMath.h"
#include <vector>
#include <memory>
//#include <wincodec.h>
//#include <wrl.h>
//
//#pragma comment(lib, "windowscodecs.lib")

class IDxcBlob;
class Camera;
class Material;

struct FrameResource
{
	ID3D12CommandAllocator* CommandAllocator = nullptr;
	UINT FenceValue = 0;
	ID3D12Resource* TransformConstantBuffer = nullptr;
	void* TransformConstantBufferMappedData = nullptr;

	ID3D12Resource* DirLgtConstantBuffer = nullptr;
	void* DirLgtConstantBufferMappedData = nullptr;
};

struct TransformConstant
{
	DirectX::XMFLOAT4X4 WorldMatrix;
	DirectX::XMFLOAT4X4 ViewMatrix;
	DirectX::XMFLOAT4X4 ProjectionMatrix;
	DirectX::XMFLOAT4X4 WorldInverseTranspose;

	DirectX::XMFLOAT3 CameraPosition;
	float Padding;
};

struct DirectionalLightConstant
{
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 1.0f };
	float Intensity = 1.0f;
	DirectX::XMFLOAT3 Color{ 1.0f,1.0f,1.0f };
	float AmbientIntensity = 0.1f;
};

class Renderer
{
public:
	Renderer();
	~Renderer();

	bool Initialize(HWND Hwnd, UINT Width, UINT Height);

	void RenderFrame(const Camera& MainCamera);

	bool CreateRootSignature();
	bool CreateShaders();
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const wchar_t* FilePath, const wchar_t* EntryPoint, const wchar_t* TargetProfile);

	bool CreatePipelineState();

	bool CreateDepthBuffer();

	std::shared_ptr<Texture> CreateTexture(const ImageData& Image, TextureColorSpace ColorSpace);
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
	
	D3D12ResourceUploader ResourceUploader{};
	D3D12DescriptorAllocator SRVDescriptorAllocator;

	GLTFLoader ModelLoader;
	ModelData LoadedModel;
	ImageLoader TextureImageLoader;

	std::shared_ptr<Texture> DefaultWhiteTexture;
	std::unique_ptr<Mesh> ModelMesh = nullptr;
	std::shared_ptr<Material> DefaultMaterial;
	std::vector<std::shared_ptr<Material>> ModelMaterials;

	ID3D12Resource* DepthBuffer = nullptr;
	ID3D12DescriptorHeap* DSVHeap = nullptr;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	UINT Width;
	UINT Height;
};