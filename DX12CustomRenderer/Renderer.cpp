#include "Renderer.h"

Renderer::~Renderer()
{
	if (CommandQueue.GetNativeCommandQueue())
	{
		CommandQueue.WaitForIdle();
	}
	if (RootSignature)
	{
		RootSignature->Release();
		RootSignature = nullptr;
	}
	if (PipelineState)
	{
		PipelineState->Release();
		PipelineState = nullptr;
	}
	for (int i = 0; i < BufferCount; i++)
	{
		if (Frame[i].CommandAllocator)
		{
			Frame[i].CommandAllocator->Release();
			Frame[i].CommandAllocator = nullptr;
		}
		if (Frame[i].ConstantBuffer)
		{
			Frame[i].ConstantBuffer->Release();
			Frame[i].ConstantBufferMappedData = nullptr;
		}
	}
	if (VertexBuffer)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}
	if (IndexBuffer)
	{
		IndexBuffer->Release();
		IndexBuffer = nullptr;
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
		
		D3D12_RESOURCE_DESC BufferDesc{};
		BufferDesc.Width = sizeof(TransformConstant);
		BufferDesc.Height = 1;
		BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		BufferDesc.DepthOrArraySize = 1;
		BufferDesc.MipLevels = 1;
		BufferDesc.SampleDesc.Count = 1;
		BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_HEAP_PROPERTIES UploadHeapProperties{};
		UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		HRESULT Result = Device.GetDevice()->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE,
			&BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Frame[i].ConstantBuffer));
		if (FAILED(Result))
		{
			Frame[i].ConstantBuffer->Release();
			return false;
		}
		if (FAILED(Frame[i].ConstantBuffer->Map(0, nullptr, &Frame[i].ConstantBufferMappedData)))
		{
			return false;
		}
	}
	//CommandList는 1개만 있어도 됨
	if (!CommandContext.Initialize(&Device, Frame[0].CommandAllocator))
	{
		return false;
	}
	if (!SwapChain.Initialize(&Device, &CommandQueue, Hwnd, Width, Height))
	{
		return false;
	}
	if (!CreateRootSignature())
	{
		return false;
	}

	if (!CreateShaders())
	{
		return false;
	}
	if (!CreatePipelineState())
	{
		return false;
	}
	if (!CreateVertexBuffer())
	{
		return false;
	}
	if (!CreateIndexBuffer())
	{
		return false;
	}
	UpdateViewport(Width, Height);

	return true;
}

void Renderer::RenderFrame()
{
	UINT32 CurrentIndex = SwapChain.GetBackBufferIndex();
	FrameResource& CurrentFrame = Frame[CurrentIndex];
	
	if(CurrentFrame.FenceValue != 0)
	{
		CommandQueue.WaitForFence(CurrentFrame.FenceValue);
	}

	CommandContext.Reset(CurrentFrame.CommandAllocator);
	
	ID3D12GraphicsCommandList* CommandList = CommandContext.GetCommandList();
	ID3D12Resource* CurrentBackBuffer = SwapChain.GetCurrentBackBuffer();

	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = CurrentBackBuffer;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandList->ResourceBarrier(1, &Barrier);

	float ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
	CommandList->ClearRenderTargetView(SwapChain.GetCurrentRTV(), ClearColor, 0, nullptr);

	//ConstantBuffer Data 세팅
	Angle += 0.01f;
	DirectX::XMMATRIX World = DirectX::XMMatrixRotationY(Angle);

	TransformConstant ConstantData;
	DirectX::XMStoreFloat4x4(&ConstantData.WorldMatrix, DirectX::XMMatrixTranspose(World));

	memcpy(CurrentFrame.ConstantBufferMappedData, &ConstantData, sizeof(TransformConstant));

	//삼각형 그리기
	CommandList->SetPipelineState(PipelineState);
	CommandList->SetGraphicsRootSignature(RootSignature);
	CommandList->SetGraphicsRootConstantBufferView(0, CurrentFrame.ConstantBuffer->GetGPUVirtualAddress());
	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE RTV = SwapChain.GetCurrentRTV();
	CommandList->OMSetRenderTargets(1, &RTV, FALSE, nullptr);

	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CommandList->IASetVertexBuffers(0, 1, &VBView);
	CommandList->IASetIndexBuffer(&IBView);
	
	CommandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
	
	//CommandList->DrawInstanced(3, 1, 0, 0);

	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	CommandList->ResourceBarrier(1, &Barrier);

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

bool Renderer::CreateRootSignature()
{
	D3D12_ROOT_PARAMETER RootParameter{};
	RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameter.Descriptor.ShaderRegister = 0; // b0
	RootParameter.Descriptor.RegisterSpace = 0;
	RootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_ROOT_SIGNATURE_DESC RootDesc;
	RootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	RootDesc.NumParameters = 1;
	RootDesc.NumStaticSamplers = 0;
	RootDesc.pParameters = &RootParameter;
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

	if (FAILED(Device.GetDevice()->CreateRootSignature(0, SerializedRootSignature->GetBufferPointer(), SerializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&RootSignature))))
	{
		return false;
	}

	return true;
}

bool Renderer::CreateShaders()
{
	VertexShader = CompileShader(L"Triangle.hlsl", L"VSMain",L"vs_6_0");
	if (!VertexShader)
	{
		return false;
	}

	PixelShader = CompileShader(L"Triangle.hlsl", L"PSMain",L"ps_6_0");
	if (!PixelShader)
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

bool Renderer::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE VS;
	D3D12_SHADER_BYTECODE PS;
	VS.pShaderBytecode = VertexShader->GetBufferPointer();
	VS.BytecodeLength = VertexShader->GetBufferSize();

	PS.pShaderBytecode = PixelShader->GetBufferPointer();
	PS.BytecodeLength = PixelShader->GetBufferSize();

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
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc{};
	PipelineStateDesc.pRootSignature = RootSignature;
	PipelineStateDesc.VS = VS;
	PipelineStateDesc.PS = PS;
	PipelineStateDesc.InputLayout.NumElements= 2;
	PipelineStateDesc.InputLayout.pInputElementDescs = InputLayout;
	PipelineStateDesc.NodeMask = 0;
	PipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	PipelineStateDesc.RasterizerState.MultisampleEnable = false;
	PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	PipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	PipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	PipelineStateDesc.DepthStencilState.DepthEnable = false;
	PipelineStateDesc.DepthStencilState.StencilEnable = false;
	PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	PipelineStateDesc.NumRenderTargets = 1;
	PipelineStateDesc.SampleDesc.Count = 1;
	PipelineStateDesc.SampleDesc.Quality = 0;
	PipelineStateDesc.SampleMask = UINT_MAX;

	if (FAILED(Device.GetDevice()->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&PipelineState))))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateVertexBuffer()
{
	//cube 정점 8개
	Vertex Vertices[] =
	{
		{
			{  0.5f, 0.5f, 0.5f },
			{  1.0f, 0.0f, 0.0f, 1.0f }
		},

		{
			{  -0.5f, 0.5f, 0.5f },
			{  0.0f, 1.0f, 0.0f, 1.0f }
		},

		{
			{ -0.5f, -0.5f, 0.5f },
			{  0.0f,  0.0f, 1.0f, 1.0f }
		},
		{
			{0.5f, -0.5f, 0.5f},
			{0.3f, 0.2f, 0.6f, 1.0f}
		},
		///
		{
			{  0.5f, 0.5f, -0.5f },
			{  1.0f, 0.0f, 0.0f, 1.0f }
		},

		{
			{  -0.5f, 0.5f, -0.5f },
			{  0.0f, 1.0f, 0.0f, 1.0f }
		},

		{
			{ -0.5f, -0.5f, -0.5f },
			{  0.0f,  0.0f, 1.0f, 1.0f }
		},
		{
			{0.5f, -0.5f, -0.5f},
			{0.3f, 0.2f, 0.6f, 1.0f}
		}
	};

	if (!CreateDefaultBuffer(Vertices, sizeof(Vertices), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, VertexBuffer))
	{
		return false;
	}

	VBView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VBView.SizeInBytes = sizeof(Vertices);
	VBView.StrideInBytes = sizeof(Vertex);

	return true;
}

bool Renderer::CreateIndexBuffer()
{
	UINT16 Indices[] = {
		0, 2, 1,
		0, 3, 2,
		4,6,5,
		4,7,6,
		4,1,5,
		4,0,1,
		7,2,6,
		7,3,2,
		5,1,6,
		1,2,6,
		0,4,7,
		0,7,3
	};

	if (!CreateDefaultBuffer(Indices, sizeof(Indices), D3D12_RESOURCE_STATE_INDEX_BUFFER, IndexBuffer))
	{
		return false;
	}

	IBView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
	IBView.SizeInBytes = sizeof(Indices);
	IBView.Format = DXGI_FORMAT_R16_UINT;

	return true;
}

bool Renderer::CreateDefaultBuffer(const void* Data, UINT64 Size, D3D12_RESOURCE_STATES FinalState, ID3D12Resource*& OutBuffer)
{
	//ex) Vertices
	ID3D12Resource* UploadBuffer = nullptr;

	D3D12_RESOURCE_DESC BufferDesc{};
	BufferDesc.Width = Size;
	BufferDesc.Height = 1;
	BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufferDesc.DepthOrArraySize = 1;
	BufferDesc.MipLevels = 1;
	BufferDesc.SampleDesc.Count = 1;
	BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_HEAP_PROPERTIES UploadHeapProperties{};
	UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	HRESULT Result = Device.GetDevice()->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer));
	if (FAILED(Result))
	{
		UploadBuffer->Release();
		return false;
	}
	void* MappedData = nullptr;
	if (FAILED(UploadBuffer->Map(0, nullptr, &MappedData)))
	{
		return false;
	}
	//배열 데이터를 해당 주소에 복사
	memcpy(MappedData, Data, Size);

	UploadBuffer->Unmap(0, nullptr);
	
	D3D12_HEAP_PROPERTIES DefaultHeapProperties{};
	DefaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	Result = Device.GetDevice()->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE,
		&BufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&OutBuffer));
	if (FAILED(Result))
	{
		return false;
	}

	CommandContext.Reset(Frame[0].CommandAllocator);
	CommandContext.GetCommandList()->CopyBufferRegion(OutBuffer, 0, UploadBuffer, 0, Size);

	D3D12_RESOURCE_BARRIER ResourceBarrier{};
	ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ResourceBarrier.Transition.pResource = OutBuffer;
	ResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	ResourceBarrier.Transition.StateAfter = FinalState;
	ResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandContext.GetCommandList()->ResourceBarrier(1, &ResourceBarrier);
	CommandContext.Close();

	CommandQueue.Execute(&CommandContext);

	UINT64 FenceValue = CommandQueue.Signal();
	if (FenceValue == 0)
	{
		return false;
	}
	CommandQueue.WaitForFence(FenceValue);

	if (UploadBuffer)
	{
		UploadBuffer->Release();
		UploadBuffer = nullptr;
	}

	return true;
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
	ScissorRect.right = static_cast<float>(Width);
	ScissorRect.bottom = static_cast<float>(Height);
}

