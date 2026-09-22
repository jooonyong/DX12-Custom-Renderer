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
#include "RenderModel.h"
#include "RenderObject.h"
#include "Scene.h"

#include "dxgi1_6.h"
#include "dxcapi.h"
#include "DirectXMath.h"
#include <vector>
#include <memory>
//#include <wincodec.h>
//#include <wrl.h>
//
//#pragma comment(lib, "windowscodecs.lib")

static constexpr UINT MaxRenderObjects = 1024;

class IDxcBlob;
class Camera;
class Material;

struct FrameResource
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator = nullptr;
	UINT FenceValue = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> ObjectConstantBuffer = nullptr;
	uint8_t* ObjectConstantBufferMappedData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> SceneConstantBuffer = nullptr;
	void* SceneConstantBufferMappedData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> DirLgtConstantBuffer = nullptr;
	void* DirLgtConstantBufferMappedData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> ShadowPassConstantBuffer;
	void* ShadowPassMappedData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> DeferredPassConstantBuffer;
	void* DeferredPassMappedData = nullptr;
};

struct ObjectConstant
{
	DirectX::XMFLOAT4X4 WorldMatrix;
	DirectX::XMFLOAT4X4 WorldInverseTranspose;
};

struct SceneConstant
{
	DirectX::XMFLOAT4X4 ViewMatrix;
	DirectX::XMFLOAT4X4 ProjectionMatrix;

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
	float AmbientIntensity = 0.5f;
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

	void RenderGBufferPass(const Scene& Scene, FrameResource& Frame, UINT FrameIndex);
	void RenderShadowPass(const Scene& MainScene, FrameResource& Frame);
	void RenderDeferredLightingPass(FrameResource& Frame);
	void RenderToneMapping(FrameResource& Frame);

	void RenderFrame(const Scene& Scene, const Camera& MainCamera);

	bool CreateMainRootSignature();
	bool CreateMainPipelineState();

	bool CreateShadowRootSignature();
	bool CreateShadowPipelineState();

	bool CreateGBufferRootSignature();
	bool CreateGBufferPipelineState();

	bool CreateDeferredLightingRootSignature();
	bool CreateDeferredLightingPipelineState();

	bool CreateToneMappingRootSignature();
	bool CreateToneMappingPipelineState();

	bool CreateShaders();
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const wchar_t* FilePath, const wchar_t* EntryPoint, const wchar_t* TargetProfile);

	bool CreateDepthBuffer();
	bool CreateShadowMap();
	bool CreateGBuffers(uint32_t Width, uint32_t Height);
	bool CreateSceneColor(uint32_t Width, uint32_t Height);

	bool CreateMappedConstantBuffer(uint64_t DataSize, Microsoft::WRL::ComPtr<ID3D12Resource>& OutResource, void** OutMappedData);

	std::shared_ptr<Texture> CreateTexture(const ImageData& Image, TextureColorSpace ColorSpace);
	std::unique_ptr<Mesh> CreateMesh(const MeshData& Data);

	void UpdateShadowPassConstant(FrameResource& Frame);

	void UpdateShadowViewport(UINT Width, UINT Height);
	void UpdateViewport(UINT Width, UINT Height);

	//Application 호출용 임시 함수
	std::shared_ptr<RenderModel> CreateRenderModel(const std::string& FilePath);

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
	Microsoft::WRL::ComPtr<ID3D12RootSignature> ToneMappingRootSignature = nullptr;

	Microsoft::WRL::ComPtr<IDxcBlob> MainVertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> MainPixelShader;

	Microsoft::WRL::ComPtr<IDxcBlob> ShadowVertexShader;

	Microsoft::WRL::ComPtr<IDxcBlob> GBufferVertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> GBufferPixelShader;

	Microsoft::WRL::ComPtr<IDxcBlob> DeferredLightingVertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> DeferredLightingPixelShader;

	Microsoft::WRL::ComPtr<IDxcBlob> ToneMappingVertexShader;
	Microsoft::WRL::ComPtr<IDxcBlob> ToneMappingPixelShader;
	
	Microsoft::WRL::ComPtr<ID3D12PipelineState> MainPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ShadowPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GBufferPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> DeferredLightingPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ToneMappingPipelineState = nullptr;

	DirectionalLightConstant DirLgtData{};

	D3D12ResourceUploader ResourceUploader{};
	D3D12DescriptorAllocator SRVDescriptorAllocator;

	GLTFLoader ModelLoader;
	ImageLoader TextureImageLoader;

	std::shared_ptr<Texture> DefaultWhiteTexture;
	std::shared_ptr<Texture> DefaultNormalTexture;
	std::shared_ptr<Material> DefaultMaterial;

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

	Microsoft::WRL::ComPtr<ID3D12Resource> SceneColor;
	D3D12_CPU_DESCRIPTOR_HANDLE SceneColorRTV;
	D3D12DescriptorHandle SceneColorSRV;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SceneColorRTVHeap;
	float Exposure = 1.0f;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	D3D12_VIEWPORT ShadowViewport;
	D3D12_RECT ShadowScissorRect;

	UINT Width;
	UINT Height;
};