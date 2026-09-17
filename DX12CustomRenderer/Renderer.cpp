#include "Renderer.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.h"
#include "GeometryGenerator.h"

Renderer::Renderer() = default;
Renderer::~Renderer()
{
	if (CommandQueue.GetNativeCommandQueue())
	{
		CommandQueue.WaitForIdle();
	}

	for (int i = 0; i < BufferCount; i++)
	{
		if (Frame[i].TransformConstantBuffer)
		{
			Frame[i].TransformConstantBuffer->Unmap(0, nullptr);
			Frame[i].TransformConstantBufferMappedData = nullptr;
		}
		if (Frame[i].DirLgtConstantBuffer)
		{
			Frame[i].DirLgtConstantBuffer->Unmap(0, nullptr);
			Frame[i].DirLgtConstantBufferMappedData = nullptr;
		}
		if (Frame[i].ShadowObjectConstantBuffer)
		{
			Frame[i].ShadowObjectConstantBuffer->Unmap(0, nullptr);
			Frame[i].ShadowObjectMappedData = nullptr;
		}
		if (Frame[i].ShadowPassConstantBuffer)
		{
			Frame[i].ShadowPassConstantBuffer->Unmap(0, nullptr);
			Frame[i].ShadowPassMappedData = nullptr;
		}
	}
	if (DepthBuffer)
	{
		DepthBuffer = nullptr;
	}
	if (DSVHeap)
	{
		DSVHeap = nullptr;
	}
}

bool Renderer::Initialize(HWND Hwnd, UINT Width, UINT Height)
{
	if (!Device.Initialize())
	{
		return false;
	}
	if (!CommandQueue.Initialize(&Device))
	{
		return false;
	}
	for (int i = 0; i < BufferCount; i++)
	{
		if (FAILED(Device.GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Frame[i].CommandAllocator))))
		{
			return false;
		}
		//TRansform ConstantBuffer용 UploadHeap
		CreateMappedConstantBuffer(sizeof(TransformConstant), Frame[i].TransformConstantBuffer, &Frame[i].TransformConstantBufferMappedData);
		//DirectionalLight ConstantBuffer용 UploadHeap
		CreateMappedConstantBuffer(sizeof(DirectionalLightConstant), Frame[i].DirLgtConstantBuffer, &Frame[i].DirLgtConstantBufferMappedData);
		//ShadowObject ConstantBuffer용 UploadHeap
		CreateMappedConstantBuffer(sizeof(ShadowObjectConstant), Frame[i].ShadowObjectConstantBuffer, &Frame[i].ShadowObjectMappedData);
		//ShadowPassConstant ConstantBuffer용 UploadHeap
		CreateMappedConstantBuffer(sizeof(ShadowPassConstant), Frame[i].ShadowPassConstantBuffer, &Frame[i].ShadowPassMappedData);
		//DeferredLighting ConstantBuffer
		CreateMappedConstantBuffer(sizeof(DeferredPassConstant), Frame[i].DeferredPassConstantBuffer, &Frame[i].DeferredPassMappedData);
	}
	//CommandList는 1개만 있어도 됨
	if (!CommandContext.Initialize(&Device, Frame[0].CommandAllocator.Get()))
	{
		return false;
	}
	if (!SwapChain.Initialize(&Device, &CommandQueue, Hwnd, Width, Height))
	{
		return false;
	}
	if (!ResourceUploader.Initialize(&Device, &CommandQueue, &CommandContext, Frame[0].CommandAllocator.Get()))
	{
		return false;
	}
	if (!SRVDescriptorAllocator.Initialize(&Device, 256))
	{
		return false;
	}

	UpdateShadowViewport(2048, 2048);
	UpdateViewport(Width, Height);

	if (!CreateGBuffers(Width, Height))
	{
		return false;
	}
	if (!CreateDepthBuffer())
	{
		return false;
	}
	if (!CreateShadowMap())
	{
		return false;
	}
	if (!CreateShaders())
	{
		return false;
	}
	if (!CreateGBufferRootSignature())
	{
		return false;
	}
	if (!CreateGBufferPipelineState())
	{
		return false;
	}
	if (!CreateShadowRootSignature())
	{
		return false;
	}
	if (!CreateShadowPipelineState())
	{
		return false;
	}

	if (!CreateDeferredLightingRootSignature())
	{
		return false;
	}
	if (!CreateDeferredLightingPipelineState())
	{
		return false;
	}
	if (!CreateMainRootSignature())
	{
		return false;
	}

	if (!CreateMainPipelineState())
	{
		return false;
	}
	if (!ResourceUploader.Begin())
	{
		return false;
	}

	ImageData WhiteImage;
	WhiteImage.Width = 1;
	WhiteImage.Height = 1;
	WhiteImage.Pixels = { 255, 255, 255, 255 };

	DefaultWhiteTexture = CreateTexture(WhiteImage, TextureColorSpace::SRGB);
	if (!DefaultWhiteTexture)
	{
		return false;
	}

	ImageData BlueImage;
	BlueImage.Width = 1;
	BlueImage.Height = 1;
	BlueImage.Pixels = { 128,128,255,255 };

	DefaultNormalTexture = CreateTexture(BlueImage, TextureColorSpace::Linear);
	if (!DefaultNormalTexture)
	{
		return false;
	}

	if (!ModelLoader.Load("Assets/AK/ak12.gltf", LoadedModel))
	{
		return false;
	}
	ModelMesh = CreateMesh(LoadedModel.Mesh);
	if (!ModelMesh)
	{
		return false;
	}

	DirectX::XMFLOAT4 BaseColor = { 1.0f,1.0f,1.0f,1.0f };
	DefaultMaterial = std::make_shared<Material>(DefaultWhiteTexture, DefaultWhiteTexture, DefaultNormalTexture, BaseColor, 0.3f, 0.0f);
	if (DefaultMaterial)
	{
		DefaultMaterial->InitializeGPU(Device.GetDevice(), BufferCount);
	}

	ModelMaterials.reserve(LoadedModel.Materials.size());
	for (const MaterialData& MaterialData : LoadedModel.Materials)
	{
		std::shared_ptr<Texture> ModelTexture = DefaultWhiteTexture;
		std::shared_ptr<Texture> MRTexture = DefaultWhiteTexture;
		std::shared_ptr<Texture> NormalTexture = DefaultNormalTexture;

		if (MaterialData.BaseColorImage.has_value())
		{
			ModelTexture = CreateTexture(MaterialData.BaseColorImage.value(), TextureColorSpace::SRGB);
			if (!ModelTexture)
			{
				return false;
			}
		}
		if (MaterialData.MetallicRoughnessImage.has_value())
		{
			MRTexture = CreateTexture(MaterialData.MetallicRoughnessImage.value(), TextureColorSpace::Linear);
			if (!MRTexture)
			{
				return false;
			}
		}
		if (MaterialData.NormalMapImage.has_value())
		{
			NormalTexture = CreateTexture(MaterialData.NormalMapImage.value(), TextureColorSpace::Linear);
			if (!NormalTexture)
			{
				return false;
			}
		}
		auto ModelMaterial = std::make_shared<Material>(ModelTexture, MRTexture, NormalTexture, MaterialData.BaseColor, MaterialData.Roughness, MaterialData.Metallic);
		if (!ModelMaterial)
		{
			return false;
		}
		if (!ModelMaterial->InitializeGPU(Device.GetDevice(), BufferCount))
		{
			return false;
		}

		ModelMaterials.push_back(ModelMaterial);
	}

	if (!ResourceUploader.End())
	{
		return false;
	}

	return true;

}

void Renderer::RenderGBufferPass(FrameResource& Frame, UINT FrameIndex)
{
	ID3D12GraphicsCommandList* CommandList = CommandContext.GetCommandList();

	CommandList->SetPipelineState(GBufferPipelineState.Get());
	CommandList->SetGraphicsRootSignature(GBufferRootSignature.Get());

	ID3D12DescriptorHeap* DescriptorHeaps[] = { SRVDescriptorAllocator.GetHeap() };
	CommandList->SetDescriptorHeaps(1, DescriptorHeaps);

	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	DSV = DSVHeap->GetCPUDescriptorHandleForHeapStart();
	CommandList->OMSetRenderTargets(3, &GBufferARTV, TRUE, &DSV);

	CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	CommandList->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	CommandList->ClearRenderTargetView(GBufferARTV, ClearColor, 0, nullptr);
	CommandList->ClearRenderTargetView(GBufferBRTV, ClearColor, 0, nullptr);
	CommandList->ClearRenderTargetView(GBufferCRTV, ClearColor, 0, nullptr);

	CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const D3D12_VERTEX_BUFFER_VIEW& VBView = ModelMesh->GetVertexBufferView();
	const D3D12_INDEX_BUFFER_VIEW& IBView = ModelMesh->GetIndexBufferView();

	CommandList->IASetVertexBuffers(0, 1, &VBView);
	CommandList->IASetIndexBuffer(&IBView);

	CommandList->SetGraphicsRootConstantBufferView(0, Frame.TransformConstantBuffer->GetGPUVirtualAddress()); //b0
	for (const SubMeshData& SubMesh : LoadedModel.SubMeshes)
	{
		std::shared_ptr<Material> Material = DefaultMaterial;
		if (SubMesh.MaterialIndex != InvalidMaterialIndex && SubMesh.MaterialIndex < ModelMaterials.size())
		{
			Material = ModelMaterials[SubMesh.MaterialIndex];
		}
		CommandList->SetGraphicsRootDescriptorTable(1, Material->GetAlbedoTexture()->GetSRV().GPU); //t0
		CommandList->SetGraphicsRootDescriptorTable(3, Material->GetMetallicRoughnessTexture()->GetSRV().GPU); //t1
		CommandList->SetGraphicsRootDescriptorTable(4, Material->GetNormalTexture()->GetSRV().GPU); //t2
		CommandList->SetGraphicsRootConstantBufferView(2, Material->GetConstantBufferGPUAddress(FrameIndex)); //b1

		CommandList->DrawIndexedInstanced(SubMesh.IndexCount, 1, SubMesh.IndexStart, 0, 0);
	}

	D3D12_RESOURCE_BARRIER ResourceBarrier{};
	ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ResourceBarrier.Transition.pResource = GBufferA.Get();
	ResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	ResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	ResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandList->ResourceBarrier(1, &ResourceBarrier);
	ResourceBarrier.Transition.pResource = GBufferB.Get();

	CommandList->ResourceBarrier(1, &ResourceBarrier);
	ResourceBarrier.Transition.pResource = GBufferC.Get();

	CommandList->ResourceBarrier(1, &ResourceBarrier);
}

void Renderer::RenderShadowPass(FrameResource& Frame)
{
	ID3D12GraphicsCommandList* CommandList = CommandContext.GetCommandList();

	CommandList->SetPipelineState(ShadowPipelineState.Get());
	CommandList->SetGraphicsRootSignature(ShadowRootSignature.Get());

	CommandList->RSSetViewports(1, &ShadowViewport);
	CommandList->RSSetScissorRects(1, &ShadowScissorRect);

	//RenderTarget를 사용하지 않고 DepthBuffer만 사용하기 때문에 RenderTarget은 nullptr로 설정
	CommandList->OMSetRenderTargets(0, nullptr, FALSE, &ShadowDSV);
	CommandList->ClearDepthStencilView(ShadowDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const D3D12_VERTEX_BUFFER_VIEW& VBView = ModelMesh->GetVertexBufferView();
	const D3D12_INDEX_BUFFER_VIEW& IBView = ModelMesh->GetIndexBufferView();

	CommandList->IASetVertexBuffers(0, 1, &VBView);
	CommandList->IASetIndexBuffer(&IBView);

	CommandList->SetGraphicsRootConstantBufferView(0, Frame.ShadowObjectConstantBuffer->GetGPUVirtualAddress());
	CommandList->SetGraphicsRootConstantBufferView(1, Frame.ShadowPassConstantBuffer->GetGPUVirtualAddress());

	for (const SubMeshData& SubMesh : LoadedModel.SubMeshes)
	{
		CommandList->DrawIndexedInstanced(SubMesh.IndexCount, 1, SubMesh.IndexStart, 0, 0);
	}

	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = ShadowDepthTexture.Get();
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandList->ResourceBarrier(1, &Barrier);
}

void Renderer::RenderDeferredLightingPass(FrameResource& Frame)
{
	ID3D12GraphicsCommandList* CommandList = CommandContext.GetCommandList();
	ID3D12Resource* CurrentBackBuffer = SwapChain.GetCurrentBackBuffer();
	UINT32 CurrentIndex = SwapChain.GetBackBufferIndex();

	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = CurrentBackBuffer;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandList->ResourceBarrier(1, &Barrier);

	float ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
	CommandList->ClearRenderTargetView(SwapChain.GetCurrentRTV(), ClearColor, 0, nullptr);

	//CommandList->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	D3D12_RESOURCE_BARRIER DepthResourceBarrier{};
	DepthResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	DepthResourceBarrier.Transition.pResource = DepthBuffer.Get();
	DepthResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	DepthResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	DepthResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandList->ResourceBarrier(1, &DepthResourceBarrier);

	CommandList->SetPipelineState(DeferredLightingPipelineState.Get());
	CommandList->SetGraphicsRootSignature(DeferredLightingRootSignature.Get());

	ID3D12DescriptorHeap* DescriptorHeaps[] = { SRVDescriptorAllocator.GetHeap() };
	CommandList->SetDescriptorHeaps(1, DescriptorHeaps);

	CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE RTV = SwapChain.GetCurrentRTV();
	CommandList->OMSetRenderTargets(1, &RTV, FALSE, nullptr);

	CommandList->SetGraphicsRootDescriptorTable(0, GBufferASRV.GPU); //t0(GBufferA), t1(GBufferB), t2, t3(Depth)
	CommandList->SetGraphicsRootConstantBufferView(1, Frame.DeferredPassConstantBuffer->GetGPUVirtualAddress()); //b0
	CommandList->SetGraphicsRootConstantBufferView(2, Frame.DirLgtConstantBuffer->GetGPUVirtualAddress()); //b1
	CommandList->SetGraphicsRootConstantBufferView(3, Frame.ShadowPassConstantBuffer->GetGPUVirtualAddress()); //b2
	CommandList->SetGraphicsRootDescriptorTable(4, ShadowSRV.GPU); //t4(ShadowTexture)

	CommandList->DrawInstanced(3, 1, 0, 0);

	D3D12_RESOURCE_BARRIER GBufferBarrier{};
	GBufferBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	GBufferBarrier.Transition.pResource = GBufferA.Get();
	GBufferBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	GBufferBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	GBufferBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandList->ResourceBarrier(1, &GBufferBarrier);
	GBufferBarrier.Transition.pResource = GBufferB.Get();

	CommandList->ResourceBarrier(1, &GBufferBarrier);
	GBufferBarrier.Transition.pResource = GBufferC.Get();

	CommandList->ResourceBarrier(1, &GBufferBarrier);

	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	CommandList->ResourceBarrier(1, &Barrier);

	D3D12_RESOURCE_BARRIER ShadowBarrier{};
	ShadowBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ShadowBarrier.Transition.pResource = ShadowDepthTexture.Get();
	ShadowBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	ShadowBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	ShadowBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandList->ResourceBarrier(1, &ShadowBarrier);
	
	DepthResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	DepthResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	
	CommandList->ResourceBarrier(1, &DepthResourceBarrier);
}

void Renderer::RenderFrame(const Camera& MainCamera)
{
	UINT32 CurrentIndex = SwapChain.GetBackBufferIndex();
	FrameResource& CurrentFrame = Frame[CurrentIndex];

	if (CurrentFrame.FenceValue != 0)
	{
		CommandQueue.WaitForFence(CurrentFrame.FenceValue);
	}

	CommandContext.Reset(CurrentFrame.CommandAllocator.Get());

	//ConstantBuffer Data세팅
	Angle += 0.001f;
	DirectX::XMMATRIX World = DirectX::XMMatrixRotationY(Angle) * DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f);
	DirectX::XMMATRIX View = MainCamera.GetViewMatrix();
	DirectX::XMMATRIX Projection = MainCamera.GetProjectionMatrix();
	DirectX::XMMATRIX WorldInverseTranspose = XMMatrixInverse(nullptr, World);

	TransformConstant ConstantData;
	DirectX::XMStoreFloat4x4(&ConstantData.WorldMatrix, DirectX::XMMatrixTranspose(World));
	DirectX::XMStoreFloat4x4(&ConstantData.ViewMatrix, DirectX::XMMatrixTranspose(View));
	DirectX::XMStoreFloat4x4(&ConstantData.ProjectionMatrix, DirectX::XMMatrixTranspose(Projection));
	DirectX::XMStoreFloat4x4(&ConstantData.WorldInverseTranspose, DirectX::XMMatrixTranspose(WorldInverseTranspose));

	ConstantData.CameraPosition = MainCamera.GetPosition();

	memcpy(CurrentFrame.TransformConstantBufferMappedData, &ConstantData, sizeof(TransformConstant));
	memcpy(CurrentFrame.DirLgtConstantBufferMappedData, &DirLgtData, sizeof(DirectionalLightConstant));

	UpdateShadowObjectConstant(CurrentFrame, World);
	UpdateShadowPassConstant(CurrentFrame);
	
	DeferredPassConstant InverseViewData;
	DirectX::XMMATRIX ViewProjection = DirectX::XMMatrixMultiply(View, Projection);
	DirectX::XMMATRIX InverseViewProjection = DirectX::XMMatrixInverse(nullptr, ViewProjection);
	DirectX::XMStoreFloat4x4(&InverseViewData.InverseViewMatrix,XMMatrixTranspose(InverseViewProjection));
	InverseViewData.CameraPosition = MainCamera.GetPosition();

	memcpy(CurrentFrame.DeferredPassMappedData, &InverseViewData, sizeof(DeferredPassConstant));

	for (auto Material : ModelMaterials)
	{
		Material->UpdateGPU(CurrentIndex);
	}
	DefaultMaterial->UpdateGPU(CurrentIndex);

	RenderGBufferPass(CurrentFrame, CurrentIndex);
	RenderShadowPass(CurrentFrame);
	RenderDeferredLightingPass(CurrentFrame);
	//RenderMainPass(CurrentFrame, MainCamera);

	CommandContext.Close();
	CommandQueue.Execute(&CommandContext);
	SwapChain.Present();

	UINT64 FenceValue = CommandQueue.Signal();
	if (FenceValue == 0)
	{
		return;
	}
	CurrentFrame.FenceValue = FenceValue;
}

bool Renderer::CreateMainRootSignature()
{
	D3D12_DESCRIPTOR_RANGE SRVRange{};
	SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	SRVRange.RegisterSpace = 0;
	SRVRange.NumDescriptors = 1;
	SRVRange.BaseShaderRegister = 0; //t0
	SRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE MRRange{};
	MRRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	MRRange.RegisterSpace = 0;
	MRRange.NumDescriptors = 1;
	MRRange.BaseShaderRegister = 1; //t1
	MRRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE NormalMapRange{};
	NormalMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	NormalMapRange.RegisterSpace = 0;
	NormalMapRange.NumDescriptors = 1;
	NormalMapRange.BaseShaderRegister = 2; //t2
	NormalMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE ShadowMapRange{};
	ShadowMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ShadowMapRange.RegisterSpace = 0;
	ShadowMapRange.NumDescriptors = 1;
	ShadowMapRange.BaseShaderRegister = 3; //t3
	ShadowMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER RootParameters[8]{};
	//Transform ConstantBuffer
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0; // b0
	RootParameters[0].Descriptor.RegisterSpace = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//AlbedoTexture DescriptorTable
	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[1].DescriptorTable.pDescriptorRanges = &SRVRange;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//Material ConstantBuffer
	RootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[2].Descriptor.ShaderRegister = 1; // b1
	RootParameters[2].Descriptor.RegisterSpace = 0;
	RootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//DirectionalLight ConstantBuffer;
	RootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[3].Descriptor.ShaderRegister = 2; // b2
	RootParameters[3].Descriptor.RegisterSpace = 0;
	RootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//MRTexture DescriptorTable
	RootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[4].DescriptorTable.pDescriptorRanges = &MRRange;
	RootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//NormalMap Texture
	RootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[5].DescriptorTable.pDescriptorRanges = &NormalMapRange;
	RootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//DirectionalLight view matrix ConstantBuffer;
	RootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[6].Descriptor.ShaderRegister = 3; // b3
	RootParameters[6].Descriptor.RegisterSpace = 0;
	RootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	//ShadowMap Texture
	RootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[7].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[7].DescriptorTable.pDescriptorRanges = &ShadowMapRange;
	RootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC SamplerDesc[2]{};
	//MainPass Sampler
	SamplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	SamplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	SamplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	SamplerDesc[0].MipLODBias = 0.0f;
	SamplerDesc[0].MaxAnisotropy = 1;
	SamplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	SamplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	SamplerDesc[0].MinLOD = 0.0f;
	SamplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
	SamplerDesc[0].ShaderRegister = 0; ///s0
	SamplerDesc[0].RegisterSpace = 0;
	SamplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//ShadowMap용 Sampler
	SamplerDesc[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	SamplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	SamplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	SamplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	SamplerDesc[1].MipLODBias = 0.0f;
	SamplerDesc[1].MaxAnisotropy = 1;
	SamplerDesc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	SamplerDesc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;  //sampling범위밖을 depth = 1로 설정해서 범위 밖은 그림자가 안생기게함
	SamplerDesc[1].MinLOD = 0.0f;	
	SamplerDesc[1].MaxLOD = D3D12_FLOAT32_MAX;	
	SamplerDesc[1].ShaderRegister = 1; //s1
	SamplerDesc[1].RegisterSpace = 0;
	SamplerDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC RootDesc;
	RootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	RootDesc.NumParameters = 8;
	RootDesc.NumStaticSamplers = 2;
	RootDesc.pParameters = RootParameters;
	RootDesc.pStaticSamplers = SamplerDesc;

	ID3DBlob* SerializedRootSignature;
	ID3DBlob* ErrorBlob;
	if (FAILED(D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &SerializedRootSignature, &ErrorBlob)))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
		}
		return false;
	}

	if (FAILED(Device.GetDevice()->CreateRootSignature(0, SerializedRootSignature->GetBufferPointer(), SerializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&MainRootSignature))))
	{
		return false;
	}

	return true;
}

bool Renderer::CreateShadowRootSignature()
{
	D3D12_ROOT_PARAMETER RootParameters[2]{};
	//ShadowObject ConstantBuffer
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0; // b0
	RootParameters[0].Descriptor.RegisterSpace = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	//ShadowPass ConstantBuffer
	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[1].Descriptor.ShaderRegister = 1; // b1
	RootParameters[1].Descriptor.RegisterSpace = 0;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_ROOT_SIGNATURE_DESC RootDesc{};
	RootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	RootDesc.NumParameters = 2;
	RootDesc.NumStaticSamplers = 0;
	RootDesc.pParameters = RootParameters;
	RootDesc.pStaticSamplers = nullptr;

	ID3DBlob* SerializedRootSignature;
	ID3DBlob* ErrorBlob;
	if (FAILED(D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &SerializedRootSignature, &ErrorBlob)))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
		}
		return false;
	}

	if (FAILED(Device.GetDevice()->CreateRootSignature(0, SerializedRootSignature->GetBufferPointer(), SerializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&ShadowRootSignature))))
	{
		return false;
	}

	return true;
}

bool Renderer::CreateShadowPipelineState()
{
	D3D12_SHADER_BYTECODE VS;
	VS.pShaderBytecode = ShadowVertexShader->GetBufferPointer();
	VS.BytecodeLength = ShadowVertexShader->GetBufferSize();

	D3D12_INPUT_ELEMENT_DESC InputLayout[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc{};
	PipelineStateDesc.pRootSignature = ShadowRootSignature.Get();
	PipelineStateDesc.VS = VS;
	PipelineStateDesc.InputLayout.NumElements = 1;
	PipelineStateDesc.InputLayout.pInputElementDescs = InputLayout;
	PipelineStateDesc.NodeMask = 0;
	PipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PipelineStateDesc.RasterizerState.MultisampleEnable = false;
	PipelineStateDesc.RasterizerState.DepthClipEnable = TRUE;
	PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	PipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	PipelineStateDesc.DepthStencilState.DepthEnable = true;
	PipelineStateDesc.DepthStencilState.StencilEnable = false;
	PipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	PipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	PipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PipelineStateDesc.NumRenderTargets = 0;
	PipelineStateDesc.SampleDesc.Count = 1;
	PipelineStateDesc.SampleDesc.Quality = 0;
	PipelineStateDesc.SampleMask = UINT_MAX;

	if (FAILED(Device.GetDevice()->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&ShadowPipelineState))))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateGBufferRootSignature()
{
	D3D12_DESCRIPTOR_RANGE SRVRange{};
	SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	SRVRange.RegisterSpace = 0;
	SRVRange.NumDescriptors = 1;
	SRVRange.BaseShaderRegister = 0; //t0
	SRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE MRRange{};
	MRRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	MRRange.RegisterSpace = 0;
	MRRange.NumDescriptors = 1;
	MRRange.BaseShaderRegister = 1; //t1
	MRRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE NormalMapRange{};
	NormalMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	NormalMapRange.RegisterSpace = 0;
	NormalMapRange.NumDescriptors = 1;
	NormalMapRange.BaseShaderRegister = 2; //t2
	NormalMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER RootParameters[5]{};
	//Transform ConstantBuffer
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0; // b0
	RootParameters[0].Descriptor.RegisterSpace = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//AlbedoTexture DescriptorTable
	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[1].DescriptorTable.pDescriptorRanges = &SRVRange;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//Material ConstantBuffer
	RootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[2].Descriptor.ShaderRegister = 1; // b1
	RootParameters[2].Descriptor.RegisterSpace = 0;
	RootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//MRTexture DescriptorTable
	RootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[3].DescriptorTable.pDescriptorRanges = &MRRange;
	RootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//NormalMap Texture
	RootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[4].DescriptorTable.pDescriptorRanges = &NormalMapRange;
	RootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC SamplerDesc{};
	//GBuffer Sampler
	SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	SamplerDesc.MipLODBias = 0.0f;
	SamplerDesc.MaxAnisotropy = 1;
	SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	SamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	SamplerDesc.MinLOD = 0.0f;
	SamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	SamplerDesc.ShaderRegister = 0; ///s0
	SamplerDesc.RegisterSpace = 0;
	SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


	D3D12_ROOT_SIGNATURE_DESC RootDesc;
	RootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	RootDesc.NumParameters = 5;
	RootDesc.NumStaticSamplers = 1;
	RootDesc.pParameters = RootParameters;
	RootDesc.pStaticSamplers = &SamplerDesc;

	ID3DBlob* SerializedRootSignature;
	ID3DBlob* ErrorBlob;
	if (FAILED(D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &SerializedRootSignature, &ErrorBlob)))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
		}
		return false;
	}

	if (FAILED(Device.GetDevice()->CreateRootSignature(0, SerializedRootSignature->GetBufferPointer(), SerializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&GBufferRootSignature))))
	{
		return false;
	}

	return true;
}

bool Renderer::CreateGBufferPipelineState()
{
	D3D12_SHADER_BYTECODE VS;
	D3D12_SHADER_BYTECODE PS;
	VS.pShaderBytecode = GBufferVertexShader->GetBufferPointer();
	VS.BytecodeLength = GBufferVertexShader->GetBufferSize();

	PS.pShaderBytecode = GBufferPixelShader->GetBufferPointer();
	PS.BytecodeLength = GBufferPixelShader->GetBufferSize();

	D3D12_INPUT_ELEMENT_DESC InputLayout[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			12,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			28,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"NORMAL",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			36,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"Tangent",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			48,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc{};
	PipelineStateDesc.pRootSignature = GBufferRootSignature.Get();
	PipelineStateDesc.VS = VS;
	PipelineStateDesc.PS = PS;
	PipelineStateDesc.InputLayout.NumElements = 5;
	PipelineStateDesc.InputLayout.pInputElementDescs = InputLayout;
	PipelineStateDesc.NodeMask = 0;
	PipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PipelineStateDesc.RasterizerState.MultisampleEnable = false;
	PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	PipelineStateDesc.RasterizerState.DepthClipEnable = TRUE;
	PipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	PipelineStateDesc.DepthStencilState.DepthEnable = true;
	PipelineStateDesc.DepthStencilState.StencilEnable = false;
	PipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	PipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	PipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	PipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	PipelineStateDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
	PipelineStateDesc.NumRenderTargets = 3;
	PipelineStateDesc.SampleDesc.Count = 1;
	PipelineStateDesc.SampleDesc.Quality = 0;
	PipelineStateDesc.SampleMask = UINT_MAX;

	if (FAILED(Device.GetDevice()->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&GBufferPipelineState))))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateDeferredLightingRootSignature()
{
	D3D12_DESCRIPTOR_RANGE SRVRange{};
	SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	SRVRange.RegisterSpace = 0;
	SRVRange.NumDescriptors = 4;
	SRVRange.BaseShaderRegister = 0; //t0
	SRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE ShadowRange{};
	ShadowRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ShadowRange.RegisterSpace = 0;
	ShadowRange.NumDescriptors = 1;
	ShadowRange.BaseShaderRegister = 4; //t4 
	ShadowRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER RootParams[5]{};
	RootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	RootParams[0].DescriptorTable.pDescriptorRanges = &SRVRange;
	RootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	
	RootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParams[1].Descriptor.RegisterSpace = 0;
	RootParams[1].Descriptor.ShaderRegister = 0; //b0
	RootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	RootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParams[2].Descriptor.RegisterSpace = 0;
	RootParams[2].Descriptor.ShaderRegister = 1; //b1
	RootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	RootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParams[3].Descriptor.RegisterSpace = 0;
	RootParams[3].Descriptor.ShaderRegister = 2; //b2
	RootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	RootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParams[4].DescriptorTable.NumDescriptorRanges = 1;
	RootParams[4].DescriptorTable.pDescriptorRanges = &ShadowRange;
	RootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC GBufferSamplerDesc{};
	//GBuffer Sampler
	GBufferSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	GBufferSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	GBufferSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	GBufferSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	GBufferSamplerDesc.MipLODBias = 0.0f;
	GBufferSamplerDesc.MaxAnisotropy = 1;
	GBufferSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	GBufferSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	GBufferSamplerDesc.MinLOD = 0.0f;
	GBufferSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	GBufferSamplerDesc.ShaderRegister = 0; ///s0
	GBufferSamplerDesc.RegisterSpace = 0;
	GBufferSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC ShadowSamplerDesc{};
	//Shadow Sampler
	ShadowSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	ShadowSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	ShadowSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	ShadowSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	ShadowSamplerDesc.MipLODBias = 0.0f;
	ShadowSamplerDesc.MaxAnisotropy = 1;
	ShadowSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	ShadowSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	ShadowSamplerDesc.MinLOD = 0.0f;
	ShadowSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	ShadowSamplerDesc.ShaderRegister = 1; ///s1
	ShadowSamplerDesc.RegisterSpace = 0;
	ShadowSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC SamplerDesc[2]{ GBufferSamplerDesc, ShadowSamplerDesc };

	D3D12_ROOT_SIGNATURE_DESC RootDesc{};
	RootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	RootDesc.NumParameters = 5;
	RootDesc.NumStaticSamplers = 2;
	RootDesc.pParameters = RootParams;
	RootDesc.pStaticSamplers = SamplerDesc;

	ID3DBlob* SerializedRootSignature;
	ID3DBlob* ErrorBlob;
	if (FAILED(D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &SerializedRootSignature, &ErrorBlob)))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
		}
		return false;
	}

	if (FAILED(Device.GetDevice()->CreateRootSignature(0, SerializedRootSignature->GetBufferPointer(), SerializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&DeferredLightingRootSignature))))
	{
		return false;
	}

	return true;
}

bool Renderer::CreateDeferredLightingPipelineState()
{
	D3D12_SHADER_BYTECODE VS;
	D3D12_SHADER_BYTECODE PS;
	VS.pShaderBytecode = DeferredLightingVertexShader->GetBufferPointer();
	VS.BytecodeLength = DeferredLightingVertexShader->GetBufferSize();

	PS.pShaderBytecode = DeferredLightingPixelShader->GetBufferPointer();
	PS.BytecodeLength = DeferredLightingPixelShader->GetBufferSize();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc{};
	PipelineStateDesc.pRootSignature = DeferredLightingRootSignature.Get();
	PipelineStateDesc.VS = VS;
	PipelineStateDesc.PS = PS;
	PipelineStateDesc.InputLayout.NumElements = 0;
	PipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
	PipelineStateDesc.NodeMask = 0;
	PipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PipelineStateDesc.RasterizerState.MultisampleEnable = false;
	PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	PipelineStateDesc.RasterizerState.DepthClipEnable = TRUE;
	PipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	PipelineStateDesc.DepthStencilState.DepthEnable = false;
	PipelineStateDesc.DepthStencilState.StencilEnable = false;
	PipelineStateDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	PipelineStateDesc.NumRenderTargets = 1;
	PipelineStateDesc.SampleDesc.Count = 1;
	PipelineStateDesc.SampleDesc.Quality = 0;
	PipelineStateDesc.SampleMask = UINT_MAX;

	if (FAILED(Device.GetDevice()->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&DeferredLightingPipelineState))))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateShaders()
{
	//MainShader
	MainVertexShader = CompileShader(L"Triangle.hlsl", L"VSMain", L"vs_6_0");
	if (!MainVertexShader)
	{
		return false;
	}

	MainPixelShader = CompileShader(L"Triangle.hlsl", L"PSMain", L"ps_6_0");
	if (!MainPixelShader)
	{
		return false;
	}

	//Shadow Shader
	ShadowVertexShader = CompileShader(L"ShadowShader.hlsl", L"VSMain", L"vs_6_0");
	if (!ShadowVertexShader)
	{
		return false;
	}

	//GBufferShader
	GBufferVertexShader = CompileShader(L"GBufferShader.hlsl", L"VSMain", L"vs_6_0");
	if (!GBufferVertexShader)
	{
		return false;
	}
	GBufferPixelShader = CompileShader(L"GBufferShader.hlsl", L"PSMain", L"ps_6_0");
	if (!GBufferPixelShader)
	{
		return false;
	}

	DeferredLightingVertexShader = CompileShader(L"DeferredLighting.hlsl", L"VSMain", L"vs_6_0");
	if (!DeferredLightingVertexShader)
	{
		return false;
	}
	DeferredLightingPixelShader = CompileShader(L"DeferredLighting.hlsl", L"PSMain", L"ps_6_0");
	if (!DeferredLightingPixelShader)
	{
		return false;
	}
	return true;
}

Microsoft::WRL::ComPtr<IDxcBlob> Renderer::CompileShader(const wchar_t* FilePath, const wchar_t* EntryPoint, const wchar_t* TargetProfile)
{
	Microsoft::WRL::ComPtr<IDxcUtils> Utils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> Compilers;
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> SourceBlob;

	if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils))))
	{
		return nullptr;
	}
	if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compilers))))
	{
		return nullptr;
	}
	if (FAILED(Utils->LoadFile(FilePath, nullptr, &SourceBlob)))
	{
		return nullptr;
	}

	DxcBuffer Source{};
	Source.Ptr = SourceBlob->GetBufferPointer();
	Source.Size = SourceBlob->GetBufferSize();
	Source.Encoding = DXC_CP_UTF8;

	LPCWSTR Arguments[] =
	{
		L"-E", EntryPoint,
		L"-T", TargetProfile,
		L"-Zi",
		L"-Qembed_debug"
	};

	Microsoft::WRL::ComPtr<IDxcIncludeHandler> IncludeHandler;

	if (FAILED(Utils->CreateDefaultIncludeHandler(&IncludeHandler)))
	{
		return nullptr;
	}

	Microsoft::WRL::ComPtr<IDxcResult> Result;
	if (FAILED(Compilers->Compile(&Source, Arguments, _countof(Arguments), IncludeHandler.Get(), IID_PPV_ARGS(&Result))))
	{
		return nullptr;
	}

	Microsoft::WRL::ComPtr<IDxcBlobUtf8> Errors;

	Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&Errors), nullptr);
	if (Errors && Errors->GetStringLength() > 0)
	{
		OutputDebugStringA(Errors->GetStringPointer());
	}

	HRESULT CompileStatus;
	Result->GetStatus(&CompileStatus);
	if (FAILED(CompileStatus))
	{
		return nullptr;
	}

	Microsoft::WRL::ComPtr<IDxcBlob> ShaderBlob;

	if (FAILED(Result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&ShaderBlob), nullptr)))
	{
		return nullptr;
	}
	return ShaderBlob;
}

bool Renderer::CreateMainPipelineState()
{
	D3D12_SHADER_BYTECODE VS;
	D3D12_SHADER_BYTECODE PS;
	VS.pShaderBytecode = MainVertexShader->GetBufferPointer();
	VS.BytecodeLength = MainVertexShader->GetBufferSize();

	PS.pShaderBytecode = MainPixelShader->GetBufferPointer();
	PS.BytecodeLength = MainPixelShader->GetBufferSize();

	D3D12_INPUT_ELEMENT_DESC InputLayout[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			12,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			28,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"NORMAL",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			36,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"Tangent",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			48,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc{};
	PipelineStateDesc.pRootSignature = MainRootSignature.Get();
	PipelineStateDesc.VS = VS;
	PipelineStateDesc.PS = PS;
	PipelineStateDesc.InputLayout.NumElements = 5;
	PipelineStateDesc.InputLayout.pInputElementDescs = InputLayout;
	PipelineStateDesc.NodeMask = 0;
	PipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PipelineStateDesc.RasterizerState.MultisampleEnable = false;
	PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	PipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	PipelineStateDesc.DepthStencilState.DepthEnable = true;
	PipelineStateDesc.DepthStencilState.StencilEnable = false;
	PipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	PipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	PipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	PipelineStateDesc.NumRenderTargets = 1;
	PipelineStateDesc.SampleDesc.Count = 1;
	PipelineStateDesc.SampleDesc.Quality = 0;
	PipelineStateDesc.SampleMask = UINT_MAX;

	if (FAILED(Device.GetDevice()->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&MainPipelineState))))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateDepthBuffer()
{
	D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc{};
	DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DSVHeapDesc.NumDescriptors = 1;
	DSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(Device.GetDevice()->CreateDescriptorHeap(&DSVHeapDesc, IID_PPV_ARGS(&DSVHeap))))
	{
		return false;
	}

	D3D12_RESOURCE_DESC DepthBufferDesc{};
	DepthBufferDesc.Width = Width;
	DepthBufferDesc.Height = Height;
	DepthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DepthBufferDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	DepthBufferDesc.MipLevels = 1;
	DepthBufferDesc.Alignment = 0;
	DepthBufferDesc.DepthOrArraySize = 1;
	DepthBufferDesc.SampleDesc.Count = 1;
	DepthBufferDesc.SampleDesc.Quality = 0;
	DepthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DepthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE ClearValue{};
	ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	ClearValue.DepthStencil.Depth = 1.0f;
	ClearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES DepthBufferHeapProperties{};
	DepthBufferHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT Result = Device.GetDevice()->CreateCommittedResource(&DepthBufferHeapProperties, D3D12_HEAP_FLAG_NONE,
		&DepthBufferDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &ClearValue, IID_PPV_ARGS(&DepthBuffer));
	if (FAILED(Result))
	{
		return false;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC DSVWriteDesc{};
	DSVWriteDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DSVWriteDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVWriteDesc.Texture2D.MipSlice = 0;
	DSVWriteDesc.Flags = D3D12_DSV_FLAG_NONE;

	DSV = DSVHeap->GetCPUDescriptorHandleForHeapStart();
	Device.GetDevice()->CreateDepthStencilView(DepthBuffer.Get(), &DSVWriteDesc, DSV);

	DepthSRV = SRVDescriptorAllocator.Allocate();

	D3D12_SHADER_RESOURCE_VIEW_DESC DepthSRVDesc{};
	DepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	DepthSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	DepthSRVDesc.Texture2D.MipLevels = 1;
	DepthSRVDesc.Texture2D.MostDetailedMip = 0;
	DepthSRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	DepthSRVDesc.Texture2D.PlaneSlice = 0;
	DepthSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	Device.GetDevice()->CreateShaderResourceView(DepthBuffer.Get(), &DepthSRVDesc, DepthSRV.CPU);

	return true;
}

bool Renderer::CreateShadowMap()
{
	D3D12_RESOURCE_DESC ShadowTextureDesc{};
	ShadowTextureDesc.Width = 2048;
	ShadowTextureDesc.Height = 2048;
	ShadowTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	ShadowTextureDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	ShadowTextureDesc.MipLevels = 1;
	ShadowTextureDesc.DepthOrArraySize = 1;
	ShadowTextureDesc.SampleDesc = { 1,0 };
	ShadowTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	ShadowTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE ClearValue{};
	ClearValue.DepthStencil.Depth = 1.0f;
	ClearValue.DepthStencil.Stencil = 0;
	ClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	D3D12_HEAP_PROPERTIES HeapProperties{};
	HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT Result = Device.GetDevice()->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&ShadowTextureDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &ClearValue, IID_PPV_ARGS(&ShadowDepthTexture));
	if (FAILED(Result))
	{
		return false;
	}
	//프로파일용 텍스쳐 이름 설정
	ShadowDepthTexture->SetName(L"ShadowDepthTexture");	

	D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc{};
	DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DSVHeapDesc.NumDescriptors = 1;
	DSVHeapDesc.NodeMask = 0;
	DSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	Result = Device.GetDevice()->CreateDescriptorHeap(&DSVHeapDesc, IID_PPV_ARGS(&ShadowDSVHeap));
	if (FAILED(Result))
	{
		return false;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc{};
	DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;
	DSVDesc.Flags = D3D12_DSV_FLAG_NONE;

	ShadowDSV = ShadowDSVHeap->GetCPUDescriptorHandleForHeapStart();
	Device.GetDevice()->CreateDepthStencilView(ShadowDepthTexture.Get(), &DSVDesc, ShadowDSV);

	ShadowSRV = SRVDescriptorAllocator.Allocate();
	
	D3D12_SHADER_RESOURCE_VIEW_DESC ShadowSRVDesc{};
	ShadowSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	ShadowSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	ShadowSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	ShadowSRVDesc.Texture2D.MostDetailedMip = 0;
	ShadowSRVDesc.Texture2D.MipLevels = 1;
	ShadowSRVDesc.Texture2D.PlaneSlice = 0;
	ShadowSRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	Device.GetDevice()->CreateShaderResourceView(ShadowDepthTexture.Get(), &ShadowSRVDesc, ShadowSRV.CPU);
	return true;
}

bool Renderer::CreateGBuffers(uint32_t Width, uint32_t Height)
{
	//BaseColor
	D3D12_RESOURCE_DESC GBufferADesc{};
	GBufferADesc.Width = Width;
	GBufferADesc.Height = Height;
	GBufferADesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	GBufferADesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	GBufferADesc.MipLevels = 1;
	GBufferADesc.DepthOrArraySize = 1;
	GBufferADesc.SampleDesc = { 1,0 };
	GBufferADesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	GBufferADesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_RESOURCE_DESC GBufferBDesc{};
	GBufferBDesc = GBufferADesc;
	GBufferBDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	D3D12_HEAP_PROPERTIES HeapProperties{};
	HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE ClearValue{};
	ClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ClearValue.Color[0] = 0.0f; // R
	ClearValue.Color[1] = 0.0f; // G
	ClearValue.Color[2] = 0.0f; // B
	ClearValue.Color[3] = 1.0f; // A

	D3D12_CLEAR_VALUE ClearValueNormal{};
	ClearValueNormal.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	ClearValueNormal.Color[0] = 0.0f; // R
	ClearValueNormal.Color[1] = 0.0f; // G
	ClearValueNormal.Color[2] = 0.0f; // B
	ClearValueNormal.Color[3] = 1.0f; // A

	HRESULT Result1 = Device.GetDevice()->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &GBufferADesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
		&ClearValue, IID_PPV_ARGS(&GBufferA));
	HRESULT Result2 = Device.GetDevice()->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &GBufferBDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
		&ClearValueNormal, IID_PPV_ARGS(&GBufferB));
	HRESULT Result3 = Device.GetDevice()->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &GBufferADesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
		&ClearValue, IID_PPV_ARGS(&GBufferC));

	if (FAILED(Result1) || FAILED(Result2) || FAILED(Result3))
	{
		return false;
	}
	
	//RTV Descriptor용 DescriptorHeap 생성
	D3D12_DESCRIPTOR_HEAP_DESC GBufferDescHeapDesc{};
	GBufferDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	GBufferDescHeapDesc.NumDescriptors = 3;
	GBufferDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT Result = Device.GetDevice()->CreateDescriptorHeap(&GBufferDescHeapDesc, IID_PPV_ARGS(&GBufferRTVDescriptorHeap));
	if (FAILED(Result))
	{
		return false;
	}
	D3D12_RENDER_TARGET_VIEW_DESC RTVADesc{};
	RTVADesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	RTVADesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_RENDER_TARGET_VIEW_DESC RTVBDesc{};
	RTVBDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	RTVBDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	
	D3D12_RENDER_TARGET_VIEW_DESC RTVCDesc{};
	RTVCDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	RTVCDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	UINT RTVDescriptorSize = Device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	GBufferARTV.ptr = GBufferRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	GBufferBRTV.ptr = GBufferRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(RTVDescriptorSize);
	GBufferCRTV.ptr = GBufferRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(RTVDescriptorSize * 2);
	
	Device.GetDevice()->CreateRenderTargetView(GBufferA.Get(), &RTVADesc, GBufferARTV);
	Device.GetDevice()->CreateRenderTargetView(GBufferB.Get(), &RTVBDesc, GBufferBRTV);
	Device.GetDevice()->CreateRenderTargetView(GBufferC.Get(), &RTVCDesc, GBufferCRTV);


	D3D12_SHADER_RESOURCE_VIEW_DESC GBufferASRVDesc{};
	GBufferASRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	GBufferASRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	GBufferASRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	GBufferASRVDesc.Texture2D.MostDetailedMip = 0;
	GBufferASRVDesc.Texture2D.MipLevels = 1;

	D3D12_SHADER_RESOURCE_VIEW_DESC GBufferBSRVDesc{};
	GBufferBSRVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	GBufferBSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	GBufferBSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	GBufferBSRVDesc.Texture2D.MostDetailedMip = 0;
	GBufferBSRVDesc.Texture2D.MipLevels = 1;

	GBufferASRV = SRVDescriptorAllocator.Allocate();
	GBufferBSRV = SRVDescriptorAllocator.Allocate();
	GBufferCSRV = SRVDescriptorAllocator.Allocate();

	Device.GetDevice()->CreateShaderResourceView(GBufferA.Get(), &GBufferASRVDesc, GBufferASRV.CPU);
	Device.GetDevice()->CreateShaderResourceView(GBufferB.Get(), &GBufferBSRVDesc, GBufferBSRV.CPU);
	Device.GetDevice()->CreateShaderResourceView(GBufferC.Get(), &GBufferASRVDesc, GBufferCSRV.CPU);

	return true;
}

bool Renderer::CreateMappedConstantBuffer(uint64_t DataSize, Microsoft::WRL::ComPtr<ID3D12Resource>& OutResource, void** OutMappedData)
{
	const uint64_t BufferSize = (DataSize + 255) & ~255;
	Microsoft::WRL::ComPtr<ID3D12Resource> ConstantBuffer;
	
	D3D12_RESOURCE_DESC CBDesc{};
	CBDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	CBDesc.Width = BufferSize;
	CBDesc.Height = 1;
	CBDesc.MipLevels = 1;
	CBDesc.DepthOrArraySize = 1;
	CBDesc.SampleDesc.Count = 1;
	CBDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_DESCRIPTOR_HEAP_DESC CBHeapDesc{};
	CBHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	CBHeapDesc.NodeMask = 0;
	CBHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	D3D12_HEAP_PROPERTIES HeapProp{};
	HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	HRESULT Result = Device.GetDevice()->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &CBDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&OutResource));
	if (FAILED(Result))
	{
		OutResource->Release();
		return false;
	}

	Result = OutResource->Map(0, nullptr, OutMappedData);
	if (FAILED(Result))
	{
		return false;
	}
	return true;
}

std::shared_ptr<Texture> Renderer::CreateTexture(const ImageData& Image, TextureColorSpace ColorSpace)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> TextureResource;
	if (!ResourceUploader.UploadTexture(Image.Pixels.data(), Image.Width, Image.Height, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, TextureResource))
	{
		return nullptr;
	}

	D3D12DescriptorHandle TextureSRV = SRVDescriptorAllocator.Allocate();

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	if (ColorSpace == TextureColorSpace::SRGB)
	{
		SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	}
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	Device.GetDevice()->CreateShaderResourceView(TextureResource.Get(), &SRVDesc, TextureSRV.CPU);

	return std::make_shared<Texture>(std::move(TextureResource), TextureSRV, Image.Width, Image.Height, DXGI_FORMAT_R8G8B8A8_UNORM);
}

std::unique_ptr<Mesh> Renderer::CreateMesh(const MeshData& Data)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> LocalVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> LocalIndexBuffer;

	const UINT VertexBufferSize = Data.Vertices.size() * sizeof(Vertex);
	const UINT IndexBufferSize = Data.Indices.size() * sizeof(uint32_t);

	ResourceUploader.UploadBuffer(Data.Vertices.data(), VertexBufferSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, LocalVertexBuffer);
	ResourceUploader.UploadBuffer(Data.Indices.data(), IndexBufferSize, D3D12_RESOURCE_STATE_INDEX_BUFFER, LocalIndexBuffer);

	return std::make_unique<Mesh>(std::move(LocalVertexBuffer), VertexBufferSize, sizeof(Vertex),
		std::move(LocalIndexBuffer), IndexBufferSize, static_cast<UINT>(Data.Indices.size()));
}

void Renderer::UpdateShadowObjectConstant(FrameResource& Frame, const DirectX::XMMATRIX& WorldMatrix)
{
	ShadowObjectConstant Data{};
	DirectX::XMStoreFloat4x4(&Data.WorldMatrix, DirectX::XMMatrixTranspose(WorldMatrix));

	memcpy(Frame.ShadowObjectMappedData, &Data, sizeof(Data));
}

void Renderer::UpdateShadowPassConstant(FrameResource& Frame)
{
	using namespace DirectX;

	XMVECTOR LightDir = XMVector3Normalize(XMVectorSet(DirLgtData.Direction.x, DirLgtData.Direction.y, DirLgtData.Direction.z, 0.0f));
	XMVECTOR SceneCenter = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	const float LightDistance = 20.0f;
	XMVECTOR LightPosition = SceneCenter - LightDir * LightDistance;

	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX LightViewMatrix = XMMatrixLookAtLH(LightPosition, SceneCenter, Up);
	XMMATRIX LightProjectionMatrix = XMMatrixOrthographicLH(20.0f, 20.0f, 0.1f, 100.0f);

	XMMATRIX LightViewProjection = LightViewMatrix * LightProjectionMatrix;


	ShadowPassConstant Data{};
	XMStoreFloat4x4(&Data.LightViewProjectionMatrix, XMMatrixTranspose(LightViewProjection));
	memcpy(Frame.ShadowPassMappedData, &Data, sizeof(Data));
}


void Renderer::UpdateShadowViewport(UINT Width, UINT Height)
{
	ShadowViewport.Width = static_cast<float>(Width);
	ShadowViewport.Height = static_cast<float>(Height);
	ShadowViewport.TopLeftX = 0.0f;
	ShadowViewport.TopLeftY = 0.0f;
	ShadowViewport.MinDepth = 0.0f;
	ShadowViewport.MaxDepth = 1.0f;
	ShadowScissorRect.left = 0;
	ShadowScissorRect.top = 0;
	ShadowScissorRect.right = static_cast<LONG>(Width);
	ShadowScissorRect.bottom = static_cast<LONG>(Height);
}

void Renderer::UpdateViewport(UINT Width, UINT Height)
{
	this->Width = Width;
	this->Height = Height;

	Viewport.Width = static_cast<float>(Width);
	Viewport.Height = static_cast<float>(Height);
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	ScissorRect.left = 0.0f;
	ScissorRect.top = 0.0f;
	ScissorRect.right = static_cast<LONG>(Width);
	ScissorRect.bottom = static_cast<LONG>(Height);
}
