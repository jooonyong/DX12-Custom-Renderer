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
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator = nullptr;
	UINT FenceValue = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> TransformConstantBuffer = nullptr;
	void* TransformConstantBufferMappedData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> DirLgtConstantBuffer = nullptr;
	void* DirLgtConstantBufferMappedData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> ShadowPassConstantBuffer;
	void* ShadowPassMappedData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> ShadowObjectConstantBuffer;
	void* ShadowObjectMappedData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> DeferredPassConstantBuffer;
	void* DeferredPassMappedData = nullptr;
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

struct DeferredPassConstant
{
	DirectX::XMFLOAT4X4 InverseViewMatrix;
	DirectX::XMFLOAT3 CameraPosition;
	float Padding;
};

struct DirectionalLightConstant
{
	DirectX::XMFLOAT3 Direction = { 1.0f, -0.5f, 1.0f };
	float Intensity = 1.0f;
	DirectX::XMFLOAT3 Color{ 1.0f,1.0f,1.0f };
	float AmbientIntensity = 0.1f;
};

struct ShadowObjectConstant
{
	DirectX::XMFLOAT4X4 WorldMatrix;
};

struct ShadowPassConstant
{
	DirectX::XMFLOAT4X4 LightViewProjectionMatrix;
};

class Renderer
{
public:
	Renderer();
	~Renderer();

	bool Initialize(HWND Hwnd, UINT Width, UINT Height);

	void RenderGBufferPass(FrameResource& Frame, UINT FrameIndex);
	void RenderShadowPass(FrameResource& Frame);
	void RenderDeferredLightingPass(FrameResource& Frame);
	void RenderFrame(const Camera& MainCamera);

	bool CreateMainRootSignature();
	bool CreateMainPipelineState();

	bool CreateShadowRootSignature();
	bool CreateShadowPipelineState();

	bool CreateGBufferRootSignature();
	bool CreateGBufferPipelineState();

	bool CreateDeferredLightingRootSignature();
	bool CreateDeferredLightingPipelineState();

	bool CreateShaders();
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const wchar_t* FilePath, const wchar_t* EntryPoint, const wchar_t* TargetProfile);

	bool CreateDepthBuffer();
	bool CreateShadowMap();
	bool CreateGBuffers(uint32_t Width, uint32_t Height);

	bool CreateMappedConstantBuffer(uint64_t DataSize, Microsoft::WRL::ComPtr<ID3D12Resource>& OutResource, void** OutMappedData);

	std::shared_ptr<Texture> CreateTexture(const ImageData& Image, TextureColorSpace ColorSpace);
	std::unique_ptr<Mesh> CreateMesh(const MeshData& Data);

	void UpdateShadowObjectConstant(FrameResource& Frame, const DirectX::XMMATRIX& WorldMatrix);
	void UpdateShadowPassConstant(FrameResource& Frame);

	void UpdateShadowViewport(UINT Width, UINT Height);
	void UpdateViewport(UINT Width, UINT Height);

private:
	D3D12Device Device;
	D3D12CommandQueue CommandQueue;
	D3D12CommandContext CommandContext;
	D3D12SwapChain SwapChain;

	FrameResource Frame[BufferCount];

	Microsoft::WRL::ComPtr<ID3D12RootSignature> MainRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> ShadowRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GBufferRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> DeferredLightingRootSignature = nullptr;

	Microsoft::WRL::ComPtr<IDxcBlob> MainVertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> MainPixelShader;

	Microsoft::WRL::ComPtr<IDxcBlob> ShadowVertexShader;

	Microsoft::WRL::ComPtr<IDxcBlob> GBufferVertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> GBufferPixelShader;

	Microsoft::WRL::ComPtr<IDxcBlob> DeferredLightingVertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> DeferredLightingPixelShader;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> MainPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ShadowPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GBufferPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> DeferredLightingPipelineState = nullptr;
	DirectionalLightConstant DirLgtData{};

	D3D12ResourceUploader ResourceUploader{};
	D3D12DescriptorAllocator SRVDescriptorAllocator;

	GLTFLoader ModelLoader;
	ModelData LoadedModel;
	ImageLoader TextureImageLoader;

	std::shared_ptr<Texture> DefaultWhiteTexture;
	std::shared_ptr<Texture> DefaultNormalTexture;
	std::unique_ptr<Mesh> ModelMesh = nullptr;
	std::shared_ptr<Material> DefaultMaterial;
	std::vector<std::shared_ptr<Material>> ModelMaterials;

	Microsoft::WRL::ComPtr<ID3D12Resource> DepthBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DSVHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE DSV;
	D3D12DescriptorHandle DepthSRV;

	Microsoft::WRL::ComPtr<ID3D12Resource> ShadowDepthTexture;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ShadowDSVHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE ShadowDSV;
	D3D12DescriptorHandle ShadowSRV;
	
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GBufferRTVDescriptorHeap;

	Microsoft::WRL::ComPtr<ID3D12Resource> GBufferA; //BaseColor
	Microsoft::WRL::ComPtr<ID3D12Resource> GBufferB; //Normal
	Microsoft::WRL::ComPtr<ID3D12Resource> GBufferC; //MRTexture

	D3D12_CPU_DESCRIPTOR_HANDLE GBufferARTV;
	D3D12_CPU_DESCRIPTOR_HANDLE GBufferBRTV;
	D3D12_CPU_DESCRIPTOR_HANDLE GBufferCRTV;

	D3D12DescriptorHandle GBufferASRV;
	D3D12DescriptorHandle GBufferBSRV;
	D3D12DescriptorHandle GBufferCSRV;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	D3D12_VIEWPORT ShadowViewport;
	D3D12_RECT ShadowScissorRect;

	UINT Width;
	UINT Height;
};