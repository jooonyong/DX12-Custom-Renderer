#include "Renderer.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.h"
#include "GeometryGenerator.h"
#include "AccelerationStructure.h"

Renderer::Renderer() = default;
Renderer::~Renderer()
{
	if (CommandQueue.GetNativeCommandQueue())
	{
		CommandQueue.WaitForIdle();
	}

	for (int i = 0; i < BufferCount; i++)
	{
		if (Frame[i].ObjectConstantBuffer)
		{
			Frame[i].ObjectConstantBuffer->Unmap(0, nullptr);
			Frame[i].ObjectConstantBufferMappedData = nullptr;
		}
		if (Frame[i].SceneConstantBuffer)
		{
			Frame[i].SceneConstantBuffer->Unmap(0, nullptr);
			Frame[i].SceneConstantBufferMappedData = nullptr;
		}
		if (Frame[i].DirLgtConstantBuffer)
		{
			Frame[i].DirLgtConstantBuffer->Unmap(0, nullptr);
			Frame[i].DirLgtConstantBufferMappedData = nullptr;
		}
		if (Frame[i].ShadowPassConstantBuffer)
		{
			Frame[i].ShadowPassConstantBuffer->Unmap(0, nullptr);
			Frame[i].ShadowPassMappedData = nullptr;
		}
		if (Frame[i].DeferredPassConstantBuffer)
		{
			Frame[i].DeferredPassConstantBuffer->Unmap(0, nullptr);
			Frame[i].DeferredPassMappedData = nullptr;
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
		UINT ObjectConstantStride = (sizeof(ObjectConstant) + 255) & ~255;
		//Object ConstantBuffer용 UploadHeap
		CreateMappedConstantBuffer(ObjectConstantStride * MaxRenderObjects, Frame[i].ObjectConstantBuffer, reinterpret_cast<void**>(&Frame[i].ObjectConstantBufferMappedData));
		//SceneConstantBuffer용 UploadHeap생성
		CreateMappedConstantBuffer(sizeof(SceneConstant), Frame[i].SceneConstantBuffer, &Frame[i].SceneConstantBufferMappedData);
		//DirectionalLight ConstantBuffer용 UploadHeap
		CreateMappedConstantBuffer(sizeof(DirectionalLightConstant), Frame[i].DirLgtConstantBuffer, &Frame[i].DirLgtConstantBufferMappedData);
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
	if (!CreateSceneColor(Width, Height))
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
	if (!CreateToneMappingRootSignature())
	{
		return false;
	}
	if (!CreateToneMappingPipelineState())
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

	DirectX::XMFLOAT4 BaseColor = { 1.0f,1.0f,1.0f,1.0f };
	DefaultMaterial = std::make_shared<Material>(DefaultWhiteTexture, DefaultWhiteTexture, DefaultNormalTexture, BaseColor, 0.3f, 0.0f);
	if (DefaultMaterial)
	{
		DefaultMaterial->InitializeGPU(Device.GetDevice(), BufferCount);
	}
	if (!ResourceUploader.End())
	{
		return false;
	}

	TLAS = std::make_unique<TopLevelAccelerationStructure>();
	if (!CreateRaytracingOutputBuffer(Width, Height))
	{
		return false;
	}
	if (!CreateRaytracingGlobalRootSignature())
	{
		return false;
	}
	if (!CreateRaytracingStateObject())
	{
		return false;
	}
	if (!CreateShaderBindingTable())
	{
		return false;
	}
	return true;
}

void Renderer::RenderGBufferPass(ID3D12GraphicsCommandList* CommandList, std::vector<DrawCommand>& DrawCommands, FrameResource& Frame, UINT FrameIndex)
{
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
	
	for (auto& Command : DrawCommands)
	{
		const D3D12_VERTEX_BUFFER_VIEW& VBView = Command.Mesh->GetVertexBufferView();
		const D3D12_INDEX_BUFFER_VIEW& IBView = Command.Mesh->GetIndexBufferView();

		CommandList->IASetVertexBuffers(0, 1, &VBView);
		CommandList->IASetIndexBuffer(&IBView);

		uint64_t Stride = Command.ObjectIndex * ((sizeof(ObjectConstant) + 255) & ~255);
		CommandList->SetGraphicsRootConstantBufferView(0, Frame.ObjectConstantBuffer->GetGPUVirtualAddress() + Stride); //b0
		CommandList->SetGraphicsRootConstantBufferView(1, Frame.SceneConstantBuffer->GetGPUVirtualAddress()); //b1

		CommandList->SetGraphicsRootDescriptorTable(2, Command.Material->GetAlbedoTexture()->GetSRV().GPU); //t0
		CommandList->SetGraphicsRootDescriptorTable(4, Command.Material->GetMetallicRoughnessTexture()->GetSRV().GPU); //t1
		CommandList->SetGraphicsRootDescriptorTable(5, Command.Material->GetNormalTexture()->GetSRV().GPU); //t2
		CommandList->SetGraphicsRootConstantBufferView(3, Command.Material->GetConstantBufferGPUAddress(FrameIndex)); //b2

		CommandList->DrawIndexedInstanced(Command.IndexCount, 1, Command.IndexStart, 0, 0);
	}
}

void Renderer::RenderShadowPass(ID3D12GraphicsCommandList* CommandList,std::vector<DrawCommand>& DrawCommands, FrameResource& Frame)
{
	CommandList->SetPipelineState(ShadowPipelineState.Get());
	CommandList->SetGraphicsRootSignature(ShadowRootSignature.Get());

	CommandList->RSSetViewports(1, &ShadowViewport);
	CommandList->RSSetScissorRects(1, &ShadowScissorRect);

	//RenderTarget를 사용하지 않고 DepthBuffer만 사용하기 때문에 RenderTarget은 nullptr로 설정
	CommandList->OMSetRenderTargets(0, nullptr, FALSE, &ShadowDSV);
	CommandList->ClearDepthStencilView(ShadowDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CommandList->SetGraphicsRootConstantBufferView(1, Frame.ShadowPassConstantBuffer->GetGPUVirtualAddress());

	for (auto& Command : DrawCommands)
	{
		const D3D12_VERTEX_BUFFER_VIEW& VBView = Command.Mesh->GetVertexBufferView();
		const D3D12_INDEX_BUFFER_VIEW& IBView = Command.Mesh->GetIndexBufferView();

		CommandList->IASetVertexBuffers(0, 1, &VBView);
		CommandList->IASetIndexBuffer(&IBView);

		uint64_t Stride = Command.ObjectIndex * ((sizeof(ObjectConstant) + 255) & ~255);
		CommandList->SetGraphicsRootConstantBufferView(0, Frame.ObjectConstantBuffer->GetGPUVirtualAddress() + Stride);
		CommandList->DrawIndexedInstanced(Command.IndexCount, 1, Command.IndexStart, 0, 0);
	}
}

void Renderer::RenderDeferredLightingPass(ID3D12GraphicsCommandList* CommandList, FrameResource& Frame)
{
	ID3D12Resource* CurrentBackBuffer = SwapChain.GetCurrentBackBuffer();
	UINT32 CurrentIndex = SwapChain.GetBackBufferIndex();

	CommandList->SetPipelineState(DeferredLightingPipelineState.Get());
	CommandList->SetGraphicsRootSignature(DeferredLightingRootSignature.Get());

	ID3D12DescriptorHeap* DescriptorHeaps[] = { SRVDescriptorAllocator.GetHeap() };
	CommandList->SetDescriptorHeaps(1, DescriptorHeaps);

	CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	CommandList->OMSetRenderTargets(1, &SceneColorRTV, FALSE, nullptr);

	CommandList->SetGraphicsRootDescriptorTable(0, GBufferDescriptorTable.GPU); //t0(GBufferA), t1(GBufferB), t2, t3(Depth)
	CommandList->SetGraphicsRootConstantBufferView(1, Frame.DeferredPassConstantBuffer->GetGPUVirtualAddress()); //b0
	CommandList->SetGraphicsRootConstantBufferView(2, Frame.DirLgtConstantBuffer->GetGPUVirtualAddress()); //b1
	CommandList->SetGraphicsRootConstantBufferView(3, Frame.ShadowPassConstantBuffer->GetGPUVirtualAddress()); //b2
	CommandList->SetGraphicsRootDescriptorTable(4, ShadowSRV.GPU); //t4(ShadowTexture)

	CommandList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::RenderToneMapping(ID3D12GraphicsCommandList* CommandList, FrameResource& Frame)
{
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

	CommandList->SetPipelineState(ToneMappingPipelineState.Get());
	CommandList->SetGraphicsRootSignature(ToneMappingRootSignature.Get());

	CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE RTV = SwapChain.GetCurrentRTV();
	CommandList->OMSetRenderTargets(1, &RTV, false, nullptr);

	ID3D12DescriptorHeap* DescriptorHeaps[] = { SRVDescriptorAllocator.GetHeap()};

	CommandList->SetDescriptorHeaps(1, DescriptorHeaps);

	Exposure = 0.75f;
	CommandList->SetGraphicsRootDescriptorTable(0, SceneColorSRV.GPU);
	CommandList->SetGraphicsRoot32BitConstants(1, 1, &Exposure, 0);
	CommandList->DrawInstanced(3, 1, 0, 0);

	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	CommandList->ResourceBarrier(1, &Barrier);
}

void Renderer::RenderRaytracingPass(ID3D12GraphicsCommandList4* CommandList, FrameResource& Frame)
{
	CommandList->SetPipelineState1(RaytracingStateObject.Get());
	CommandList->SetComputeRootSignature(RaytracingGlobalRootSignature.Get());
	
	ID3D12DescriptorHeap* DescriptorHeaps[] = { SRVDescriptorAllocator.GetHeap() };	
	CommandList->SetDescriptorHeaps(_countof(DescriptorHeaps), DescriptorHeaps);

	CommandList->SetComputeRootDescriptorTable(0, RaytracingTLASSRV.GPU); //t0 TLAS SRV
	CommandList->SetComputeRootDescriptorTable(1, RaytracingOutputUAV.GPU); //u0 Output UAV
	CommandList->SetComputeRootConstantBufferView(2, Frame.DeferredPassConstantBuffer->GetGPUVirtualAddress()); //b0 CameraData constantBuffer
	
	D3D12_DISPATCH_RAYS_DESC DispatchDesc{};
	DispatchDesc.RayGenerationShaderRecord.StartAddress = RayGenShaderTable->GetGPUVirtualAddress();
	DispatchDesc.RayGenerationShaderRecord.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	DispatchDesc.MissShaderTable.StartAddress = MissShaderTable->GetGPUVirtualAddress();
	DispatchDesc.MissShaderTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	DispatchDesc.MissShaderTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	
	DispatchDesc.HitGroupTable.StartAddress = HitGroupShaderTable->GetGPUVirtualAddress();
	DispatchDesc.HitGroupTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	DispatchDesc.HitGroupTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	DispatchDesc.Width = Width;
	DispatchDesc.Height = Height;
	DispatchDesc.Depth = 1;

	CommandList->DispatchRays(&DispatchDesc);
}

void Renderer::RenderFrame(const Scene& MainScene, const Camera& MainCamera)
{
	UINT32 CurrentIndex = SwapChain.GetBackBufferIndex();
	FrameResource& CurrentFrame = Frame[CurrentIndex];

	if (CurrentFrame.FenceValue != 0)
	{
		CommandQueue.WaitForFence(CurrentFrame.FenceValue);
	}
	auto& Objects = MainScene.GetRenderObjects();
	if (Objects.size() > MaxRenderObjects)
	{
		return;
	}

	CommandContext.Reset(CurrentFrame.CommandAllocator.Get());

	std::vector<DrawCommand> DrawCommands;
	BuildDrawCommand(MainScene, DrawCommands);

	for (int i= 0; i<Objects.size();i++)
	{
		//ObjectConstantBuffer Data세팅
		ObjectConstant ObjectConstantData;
		DirectX::XMMATRIX World = DirectX::XMLoadFloat4x4(&Objects[i].World);
		DirectX::XMMATRIX WorldInverseTranspose = XMMatrixInverse(nullptr, World);

		DirectX::XMStoreFloat4x4(&ObjectConstantData.WorldMatrix, DirectX::XMMatrixTranspose(World));
		DirectX::XMStoreFloat4x4(&ObjectConstantData.WorldInverseTranspose, DirectX::XMMatrixTranspose(WorldInverseTranspose));

		memcpy(CurrentFrame.ObjectConstantBufferMappedData + i * ((sizeof(ObjectConstant) + 255) & ~255), &ObjectConstantData, sizeof(ObjectConstant));
}
	//Angle += 0.001f;
	DirectX::XMMATRIX View = MainCamera.GetViewMatrix();
	DirectX::XMMATRIX Projection = MainCamera.GetProjectionMatrix();
	
	SceneConstant SceneConstantData;
	
	DirectX::XMStoreFloat4x4(&SceneConstantData.ViewMatrix, DirectX::XMMatrixTranspose(View));
	DirectX::XMStoreFloat4x4(&SceneConstantData.ProjectionMatrix, DirectX::XMMatrixTranspose(Projection));
	SceneConstantData.CameraPosition = MainCamera.GetPosition();

	memcpy(CurrentFrame.SceneConstantBufferMappedData, &SceneConstantData, sizeof(SceneConstant));
	memcpy(CurrentFrame.DirLgtConstantBufferMappedData, &DirLgtData, sizeof(DirectionalLightConstant));
	
	UpdateShadowPassConstant(CurrentFrame);
	
	DeferredPassConstant InverseViewData;
	DirectX::XMMATRIX ViewProjection = DirectX::XMMatrixMultiply(View, Projection);
	DirectX::XMMATRIX InverseViewProjection = DirectX::XMMatrixInverse(nullptr, ViewProjection);
	DirectX::XMStoreFloat4x4(&InverseViewData.InverseViewMatrix,XMMatrixTranspose(InverseViewProjection));
	InverseViewData.CameraPosition = MainCamera.GetPosition();

	memcpy(CurrentFrame.DeferredPassMappedData, &InverseViewData, sizeof(DeferredPassConstant));
	DefaultMaterial->UpdateGPU(CurrentIndex);
	for (auto& Object : Objects)
	{
		for (auto Material : Object.Model->Materials)
		{
			Material->UpdateGPU(CurrentIndex);
		}
	}
	if (!TLAS->IsBuilt())
	{
		if (!TLAS->BuildTLAS(&Device, &CommandContext, MainScene))
		{
			return;
		}
		if (!CreateRaytracingDescriptors())
		{
			return;
		}
	}
	
	Graph.AddPass("GBufferPass",
		{
			{ GBufferA.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, RenderGraphAccess::Write},
			{ GBufferB.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, RenderGraphAccess::Write},
			{ GBufferC.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, RenderGraphAccess::Write},
			{ DepthBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, RenderGraphAccess::Write}
		},
		[&](ID3D12GraphicsCommandList* CommandList)
		{
			Renderer::RenderGBufferPass(CommandList, DrawCommands, CurrentFrame, CurrentIndex);
		}
	);
	Graph.AddPass("ShadowPass",
		{
			{ ShadowDepthTexture.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, RenderGraphAccess::Write}
		},
		[&](ID3D12GraphicsCommandList* CommandList)
		{
			Renderer::RenderShadowPass(CommandList, DrawCommands, CurrentFrame);
		}
	);
	Graph.AddPass("DeferredLightingPass",
		{
			{ GBufferA.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RenderGraphAccess::Read},
			{ GBufferB.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RenderGraphAccess::Read},
			{ GBufferC.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RenderGraphAccess::Read},
			{ DepthBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RenderGraphAccess::Read},
			{ ShadowDepthTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RenderGraphAccess::Read},
			{ SceneColor.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, RenderGraphAccess::Write}
		},
		[&](ID3D12GraphicsCommandList* CommandList)
		{
			Renderer::RenderDeferredLightingPass(CommandList, CurrentFrame);
		}
	);
	Graph.AddPass("RaytracingPass",
		{
			{ RaytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, RenderGraphAccess::Write}
		},
		[&](ID3D12GraphicsCommandList* CommandList)
		{
			ID3D12GraphicsCommandList4* RaytracingCommandList = static_cast<ID3D12GraphicsCommandList4*>(CommandList);
			Renderer::RenderRaytracingPass(RaytracingCommandList, CurrentFrame);
		}
	);
	Graph.AddPass("CopyRaytracingOutput",
		{
			{ RaytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, RenderGraphAccess::Read},
			{ SceneColor.Get(), D3D12_RESOURCE_STATE_COPY_DEST, RenderGraphAccess::Write}
		},
		[&](ID3D12GraphicsCommandList* CommandList)
		{
			CommandList->CopyResource(SceneColor.Get(), RaytracingOutput.Get());
		}
	);
	Graph.AddPass("ToneMapping",
		{
			{ SceneColor.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RenderGraphAccess::Read}
		},
		[&](ID3D12GraphicsCommandList* CommandList)
		{
			Renderer::RenderToneMapping(CommandList, CurrentFrame);
		}
	);

	Graph.Compile();

	Graph.Execute(CommandContext.GetCommandList(), StateTracker);

	CommandContext.Close();
	CommandQueue.Execute(&CommandContext);
	SwapChain.Present();

	Graph.Reset();

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

	D3D12_ROOT_PARAMETER RootParameters[6]{};
	//Transform ConstantBuffer ->Object ConstantBuffer
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0; // b0
	RootParameters[0].Descriptor.RegisterSpace = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	//SceneConstantBuffer
	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[1].Descriptor.ShaderRegister = 1; // b1
	RootParameters[1].Descriptor.RegisterSpace = 0;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	//AlbedoTexture DescriptorTable
	RootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[2].DescriptorTable.pDescriptorRanges = &SRVRange;
	RootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//Material ConstantBuffer
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
	RootDesc.NumParameters = 6;
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
	SRVRange.BaseShaderRegister = 0; //t0 ~ t3
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
	PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
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

bool Renderer::CreateToneMappingRootSignature()
{
	D3D12_DESCRIPTOR_RANGE SRVRange{};
	SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	SRVRange.RegisterSpace = 0;
	SRVRange.NumDescriptors = 1;
	SRVRange.BaseShaderRegister = 0; //t0
	SRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER RootParam[2]{};
	RootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParam[0].DescriptorTable.NumDescriptorRanges = 1;
	RootParam[0].DescriptorTable.pDescriptorRanges = &SRVRange;
	RootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	RootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	RootParam[1].Constants.RegisterSpace = 0;
	RootParam[1].Constants.ShaderRegister = 0;
	RootParam[1].Constants.Num32BitValues = 1;
	RootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC SamplerDesc{};
	SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	SamplerDesc.MipLODBias = 0.0f;
	SamplerDesc.MaxAnisotropy = 1;
	SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	SamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	SamplerDesc.MinLOD = 0.0f;
	SamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	SamplerDesc.ShaderRegister = 0; ///s0
	SamplerDesc.RegisterSpace = 0;
	SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC RootDesc{};
	RootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	RootDesc.NumParameters = 2;
	RootDesc.NumStaticSamplers = 1;
	RootDesc.pParameters = RootParam;
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

	if (FAILED(Device.GetDevice()->CreateRootSignature(0, SerializedRootSignature->GetBufferPointer(), SerializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&ToneMappingRootSignature))))
	{
		return false;
	}

	return true;
}

bool Renderer::CreateToneMappingPipelineState()
{
	D3D12_SHADER_BYTECODE VS;
	D3D12_SHADER_BYTECODE PS;
	VS.pShaderBytecode = ToneMappingVertexShader->GetBufferPointer();
	VS.BytecodeLength = ToneMappingVertexShader->GetBufferSize();

	PS.pShaderBytecode = ToneMappingPixelShader->GetBufferPointer();
	PS.BytecodeLength = ToneMappingPixelShader->GetBufferSize();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc{};
	PipelineStateDesc.pRootSignature = ToneMappingRootSignature.Get();
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

	if (FAILED(Device.GetDevice()->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&ToneMappingPipelineState))))
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
	ToneMappingVertexShader = CompileShader(L"ToneMapping.hlsl", L"VSMain", L"vs_6_0");
	if (!ToneMappingVertexShader)
	{
		return false;
	}
	ToneMappingPixelShader = CompileShader(L"ToneMapping.hlsl", L"PSMain", L"ps_6_0");
	if (!ToneMappingPixelShader)
	{
		return false;
	}
	if (!CompileRaytracingShader(L"Raytracing.hlsl", RaytracingLibrary))
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

bool Renderer::CompileRaytracingShader(const wchar_t* FilePath, Microsoft::WRL::ComPtr<IDxcBlob>& OutShaderBlob)
{
	Microsoft::WRL::ComPtr<IDxcUtils> Utils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> Compiler;
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> SourceBlob;

	if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils))))
	{
		return false;
	}
	if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler))))
	{
		return false;
	}
	if (FAILED(Utils->LoadFile(FilePath, nullptr, &SourceBlob)))
	{
		return false;
	}

	DxcBuffer Source{};
	Source.Ptr = SourceBlob->GetBufferPointer();
	Source.Size = SourceBlob->GetBufferSize();
	Source.Encoding = DXC_CP_UTF8;

	LPCWSTR Arguments[] =
	{
		L"-T", L"lib_6_3",
		L"-Zi",
		L"-Qembed_debug"
	};

	Microsoft::WRL::ComPtr<IDxcIncludeHandler> IncludeHandler;
	if (FAILED(Utils->CreateDefaultIncludeHandler(&IncludeHandler)))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IDxcResult> Result;
	if (FAILED(Compiler->Compile(&Source, Arguments, _countof(Arguments), IncludeHandler.Get(), IID_PPV_ARGS(&Result))))
	{
		return false;
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
		return false;
	}

	if (FAILED(Result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&OutShaderBlob), nullptr)))
	{
		return false;
	}
	return true;
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

	StateTracker.RegisterResource(DepthBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	D3D12_DEPTH_STENCIL_VIEW_DESC DSVWriteDesc{};
	DSVWriteDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DSVWriteDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVWriteDesc.Texture2D.MipSlice = 0;
	DSVWriteDesc.Flags = D3D12_DSV_FLAG_NONE;

	DSV = DSVHeap->GetCPUDescriptorHandleForHeapStart();
	Device.GetDevice()->CreateDepthStencilView(DepthBuffer.Get(), &DSVWriteDesc, DSV);

	D3D12_SHADER_RESOURCE_VIEW_DESC DepthSRVDesc{};
	DepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	DepthSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	DepthSRVDesc.Texture2D.MipLevels = 1;
	DepthSRVDesc.Texture2D.MostDetailedMip = 0;
	DepthSRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	DepthSRVDesc.Texture2D.PlaneSlice = 0;
	DepthSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	D3D12_CPU_DESCRIPTOR_HANDLE DepthDescHandle = SRVDescriptorAllocator.GetCPUHandle(GBufferDescriptorTable, 3);
	Device.GetDevice()->CreateShaderResourceView(DepthBuffer.Get(), &DepthSRVDesc, DepthDescHandle);

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

	StateTracker.RegisterResource(ShadowDepthTexture.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

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

	ShadowSRV = SRVDescriptorAllocator.Allocate(1);
	
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

	StateTracker.RegisterResource(GBufferA.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	StateTracker.RegisterResource(GBufferB.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	StateTracker.RegisterResource(GBufferC.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	
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

	GBufferDescriptorTable = SRVDescriptorAllocator.Allocate(4);
	
	Device.GetDevice()->CreateShaderResourceView(GBufferA.Get(), &GBufferASRVDesc, SRVDescriptorAllocator.GetCPUHandle(GBufferDescriptorTable, 0));
	Device.GetDevice()->CreateShaderResourceView(GBufferB.Get(), &GBufferBSRVDesc, SRVDescriptorAllocator.GetCPUHandle(GBufferDescriptorTable, 1));
	Device.GetDevice()->CreateShaderResourceView(GBufferC.Get(), &GBufferASRVDesc, SRVDescriptorAllocator.GetCPUHandle(GBufferDescriptorTable, 2));

	return true;
}

bool Renderer::CreateSceneColor(uint32_t Width, uint32_t Height)
{
	D3D12_RESOURCE_DESC SceneColorDesc{};
	SceneColorDesc.Width = Width;
	SceneColorDesc.Height = Height;
	SceneColorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	SceneColorDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	SceneColorDesc.MipLevels = 1;
	SceneColorDesc.DepthOrArraySize = 1;
	SceneColorDesc.SampleDesc = { 1,0 };
	SceneColorDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	SceneColorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	
	D3D12_HEAP_PROPERTIES HeapProp{};
	HeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE ClearValue{};
	ClearValue.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	ClearValue.Color[0] = 0.0f;
	ClearValue.Color[1] = 0.0f;
	ClearValue.Color[2] = 0.0f;
	ClearValue.Color[3] = 1.0f;

	HRESULT Result = Device.GetDevice()->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &SceneColorDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET, &ClearValue, IID_PPV_ARGS(&SceneColor));
	if (FAILED(Result))
	{
		return false;
	}

	StateTracker.RegisterResource(SceneColor.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc{};
	RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	RTVHeapDesc.NumDescriptors = 1;
	RTVHeapDesc.NodeMask = 0;
	RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	Result = Device.GetDevice()->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&SceneColorRTVHeap));
	if (FAILED(Result))
	{
		return false;
	}
	SceneColorRTV = SceneColorRTVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc{};
	RTVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	Device.GetDevice()->CreateRenderTargetView(SceneColor.Get(), &RTVDesc, SceneColorRTV);

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Texture2D.MipLevels = 1;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	SceneColorSRV = SRVDescriptorAllocator.Allocate(1);
	Device.GetDevice()->CreateShaderResourceView(SceneColor.Get(), &SRVDesc, SceneColorSRV.CPU);
	
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

	D3D12DescriptorHandle TextureSRV = SRVDescriptorAllocator.Allocate(1);

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

	ResourceUploader.UploadBuffer(Data.Vertices.data(), VertexBufferSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, LocalVertexBuffer);
	ResourceUploader.UploadBuffer(Data.Indices.data(), IndexBufferSize, D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, LocalIndexBuffer);

	return std::make_unique<Mesh>(std::move(LocalVertexBuffer), VertexBufferSize, sizeof(Vertex),
		std::move(LocalIndexBuffer), IndexBufferSize, static_cast<UINT>(Data.Indices.size()));
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

std::shared_ptr<RenderModel> Renderer::CreateRenderModel(const std::string& FilePath)
{
	if (!ResourceUploader.Begin())
	{
		return nullptr;
	}

	ModelData LoadedModel{};
	if (!ModelLoader.Load(FilePath, LoadedModel))
	{
		return nullptr;
	}

	std::shared_ptr<RenderModel> Model = std::make_shared<RenderModel>();
	Model->Mesh = CreateMesh(LoadedModel.Mesh);
	Model->SubMeshes = LoadedModel.SubMeshes;
	Model->Materials.reserve(LoadedModel.Materials.size());
	Model->BLAS = std::make_unique<BottomLevelAccelerationStructure>();
	if (!Model->BLAS->BuildBLAS(&Device, &CommandContext, Model->Mesh.get()))
	{
		return nullptr;
	}

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
				return nullptr;
			}
		}
		if (MaterialData.MetallicRoughnessImage.has_value())
		{
			MRTexture = CreateTexture(MaterialData.MetallicRoughnessImage.value(), TextureColorSpace::Linear);
			if (!MRTexture)
			{
				return nullptr;
			}
		}
		if (MaterialData.NormalMapImage.has_value())
		{
			NormalTexture = CreateTexture(MaterialData.NormalMapImage.value(), TextureColorSpace::Linear);
			if (!NormalTexture)
			{
				return nullptr;
			}
		}
		auto ModelMaterial = std::make_shared<Material>(ModelTexture, MRTexture, NormalTexture, MaterialData.BaseColor, MaterialData.Roughness, MaterialData.Metallic);
		if (!ModelMaterial)
		{
			return nullptr;
		}
		if (!ModelMaterial->InitializeGPU(Device.GetDevice(), BufferCount))
		{
			return nullptr;
		}

		Model->Materials.push_back(ModelMaterial);
	}

	if (!ResourceUploader.End())
	{
		return nullptr;
	}

	return Model;
}

void Renderer::BuildDrawCommand(const Scene& Scene, std::vector<DrawCommand>& OutCommands)
{
	auto& Objects = Scene.GetRenderObjects();
	uint32_t Index = 0;
	OutCommands.clear();

	for (auto& Object : Objects)
	{
		for (auto SubMesh : Object.Model->SubMeshes)
		{
			DrawCommand Command;
			Command.Mesh = Object.Model->Mesh.get();
			Command.Material = DefaultMaterial.get();
			if (SubMesh.MaterialIndex != InvalidMaterialIndex && SubMesh.MaterialIndex < Object.Model->Materials.size())
			{
				Command.Material = Object.Model->Materials[SubMesh.MaterialIndex].get();
			}
			Command.IndexStart = SubMesh.IndexStart;
			Command.IndexCount = SubMesh.IndexCount;
			Command.ObjectIndex = Index;

			OutCommands.push_back(Command);
		}
		Index++;
	}
}

bool Renderer::CreateRaytracingOutputBuffer(UINT Width, UINT Height)
{
	D3D12_RESOURCE_DESC Desc{};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Desc.MipLevels = 1;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	Desc.DepthOrArraySize = 1;
	Desc.SampleDesc = { 1,0 };
	Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES HeapProp{};
	HeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT Result = Device.GetRaytracingDevice()->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
		nullptr, IID_PPV_ARGS(&RaytracingOutput));
	if (FAILED(Result))
	{
		return false;
	}

	StateTracker.RegisterResource(RaytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	return true;
}

bool Renderer::CreateRaytracingDescriptors()
{
	RaytracingTLASSRV = SRVDescriptorAllocator.Allocate(1);
	RaytracingOutputUAV = SRVDescriptorAllocator.Allocate(1);

	D3D12_SHADER_RESOURCE_VIEW_DESC TLASSRVDesc{};
	TLASSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	TLASSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	TLASSRVDesc.RaytracingAccelerationStructure.Location = TLAS->GetTLASBuffer()->GetGPUVirtualAddress();

	Device.GetRaytracingDevice()->CreateShaderResourceView(nullptr, &TLASSRVDesc, RaytracingTLASSRV.CPU);

	D3D12_UNORDERED_ACCESS_VIEW_DESC OutputUAVDesc{};
	OutputUAVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	OutputUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	OutputUAVDesc.Texture2D.MipSlice = 0;
	OutputUAVDesc.Texture2D.PlaneSlice = 0;

	Device.GetRaytracingDevice()->CreateUnorderedAccessView(RaytracingOutput.Get(), nullptr, &OutputUAVDesc, RaytracingOutputUAV.CPU);
	
	return true;
}

bool Renderer::CreateRaytracingGlobalRootSignature()
{
	D3D12_DESCRIPTOR_RANGE Ranges[2]{};

	// t0 : TLAS
	Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	Ranges[0].NumDescriptors = 1;
	Ranges[0].BaseShaderRegister = 0;
	Ranges[0].RegisterSpace = 0;
	Ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// u0 : Output
	Ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	Ranges[1].NumDescriptors = 1;
	Ranges[1].BaseShaderRegister = 0;
	Ranges[1].RegisterSpace = 0;
	Ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER RootParameters[3]{};
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[0].DescriptorTable.pDescriptorRanges = &Ranges[0];
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[1].DescriptorTable.pDescriptorRanges = &Ranges[1];
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; 

	RootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[2].Descriptor.RegisterSpace = 0;
	RootParameters[2].Descriptor.ShaderRegister = 0; //b0 CameraData constantBuffer
	RootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc{};
	RootSignatureDesc.NumParameters = _countof(RootParameters);
	RootSignatureDesc.pParameters = RootParameters;
	RootSignatureDesc.NumStaticSamplers = 0;
	RootSignatureDesc.pStaticSamplers = nullptr;
	RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	
	Microsoft::WRL::ComPtr<ID3DBlob> SignatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;

	HRESULT Result = D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &SignatureBlob, &ErrorBlob);
	if (FAILED(Result))
	{
		return false;
	}
	
	Result = Device.GetRaytracingDevice()->CreateRootSignature(0, SignatureBlob->GetBufferPointer(), SignatureBlob->GetBufferSize(), IID_PPV_ARGS(&RaytracingGlobalRootSignature));
	if (FAILED(Result))
	{
		return false;
	}

	return true;
}

bool Renderer::CreateRaytracingStateObject()
{
	D3D12_EXPORT_DESC Exports[3] =
	{
		{ L"RayGen", nullptr,D3D12_EXPORT_FLAG_NONE},
		{ L"Miss", nullptr, D3D12_EXPORT_FLAG_NONE},
		{ L"ClosestHit", nullptr, D3D12_EXPORT_FLAG_NONE}
	};

	D3D12_DXIL_LIBRARY_DESC DXILLibraryDesc{};
	DXILLibraryDesc.DXILLibrary.BytecodeLength = RaytracingLibrary->GetBufferSize();
	DXILLibraryDesc.DXILLibrary.pShaderBytecode = RaytracingLibrary->GetBufferPointer();
	DXILLibraryDesc.NumExports = _countof(Exports);
	DXILLibraryDesc.pExports = Exports;

	D3D12_HIT_GROUP_DESC HitGroupDesc{};
	HitGroupDesc.HitGroupExport = L"HitGroup";
	HitGroupDesc.ClosestHitShaderImport = L"ClosestHit";
	HitGroupDesc.AnyHitShaderImport = nullptr;
	HitGroupDesc.IntersectionShaderImport = nullptr;
	HitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

	D3D12_RAYTRACING_SHADER_CONFIG ShaderConfig{};
	ShaderConfig.MaxPayloadSizeInBytes = sizeof(float) * 3;
	ShaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2;
	
	D3D12_RAYTRACING_PIPELINE_CONFIG PipelineConfig{};
	PipelineConfig.MaxTraceRecursionDepth = 1;

	D3D12_GLOBAL_ROOT_SIGNATURE GlobalRootSignature{};
	GlobalRootSignature.pGlobalRootSignature = RaytracingGlobalRootSignature.Get();

	D3D12_STATE_SUBOBJECT Subobjects[5]{};
	Subobjects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	Subobjects[0].pDesc = &DXILLibraryDesc;

	Subobjects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	Subobjects[1].pDesc = &HitGroupDesc;

	Subobjects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	Subobjects[2].pDesc = &ShaderConfig;

	Subobjects[3].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	Subobjects[3].pDesc = &PipelineConfig;

	Subobjects[4].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	Subobjects[4].pDesc = &GlobalRootSignature;

	D3D12_STATE_OBJECT_DESC StateObjectDesc{};
	StateObjectDesc.NumSubobjects = _countof(Subobjects);
	StateObjectDesc.pSubobjects = Subobjects;
	StateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	HRESULT Result = Device.GetRaytracingDevice()->CreateStateObject(&StateObjectDesc, IID_PPV_ARGS(&RaytracingStateObject));
	if (FAILED(Result))
	{
		return false;
	}
	Result = RaytracingStateObject.As(&RaytracingPipelineStateProperties);
	if (FAILED(Result))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateShaderBindingTable()
{
	void* RayGenIdentifier = RaytracingPipelineStateProperties->GetShaderIdentifier(L"RayGen");
	void* MissIdentifier = RaytracingPipelineStateProperties->GetShaderIdentifier(L"Miss");
	void* HitGroupIdentifier = RaytracingPipelineStateProperties->GetShaderIdentifier(L"HitGroup");

	if(!RayGenIdentifier || !MissIdentifier || !HitGroupIdentifier)
	{
		return false;
	}

	if (!CreateShaderTable(RayGenIdentifier, RayGenShaderTable))
	{
		return false;
	}
	if (!CreateShaderTable(MissIdentifier, MissShaderTable))
	{
		return false;
	}
	if (!CreateShaderTable(HitGroupIdentifier, HitGroupShaderTable))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateShaderTable(const void* ShaderIdentifier, Microsoft::WRL::ComPtr<ID3D12Resource>& OutBuffer)
{
	UINT BufferSize = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

	D3D12_HEAP_PROPERTIES HeapProp{};
	HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC BufferDesc{};
	BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufferDesc.Width = BufferSize;
	BufferDesc.Height = 1;
	BufferDesc.MipLevels = 1;
	BufferDesc.DepthOrArraySize = 1;
	BufferDesc.SampleDesc.Count = 1;
	BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT Result = Device.GetRaytracingDevice()->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&OutBuffer));
	if (FAILED(Result))
	{
		return false;
	}

	void* MappedData;
	Result = OutBuffer->Map(0, nullptr, &MappedData);
	if (FAILED(Result))
	{
		return false;
	}
	memcpy(MappedData, ShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	OutBuffer->Unmap(0, nullptr);

	return true;
}
