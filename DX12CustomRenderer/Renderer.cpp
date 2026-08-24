#include "Renderer.h"
#include "Camera.h"
#include "GeometryGenerator.h"

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
	if (DepthBuffer)
	{
		DepthBuffer->Release();
		DepthBuffer = nullptr;
	}
	if (DSVHeap)
	{
		DSVHeap->Release();
		DSVHeap = nullptr;
	}
	if (Texture)
	{
		Texture->Release();
		Texture = nullptr;
	}
	if (SRVHeap)
	{
		SRVHeap->Release();
		SRVHeap = nullptr;
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
		
		//ConstantBuffer용 UploadHeap
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

	UpdateViewport(Width, Height);

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

	CubeMesh = CreateMesh(GeometryGenerator::CreateCube());
	if (!CubeMesh)
	{
		return false;
	}

	if (!CreateDepthBuffer())
	{
		return false;
	}
	if (!CreateTexture())
	{
		return false;
	}
	return true;
}

void Renderer::RenderFrame(const Camera& MainCamera)
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

	D3D12_CPU_DESCRIPTOR_HANDLE DSV = DSVHeap->GetCPUDescriptorHandleForHeapStart();
	CommandList->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0.0f, 0, nullptr);

	//ConstantBuffer Data(World,View,Projection) 세팅
	Angle += 0.01f;
	DirectX::XMMATRIX World = DirectX::XMMatrixRotationY(Angle);
	DirectX::XMMATRIX View = MainCamera.GetViewMatrix();
	DirectX::XMMATRIX Projection = MainCamera.GetProjectionMatrix();

	TransformConstant ConstantData;
	DirectX::XMStoreFloat4x4(&ConstantData.WorldMatrix, DirectX::XMMatrixTranspose(World));
	DirectX::XMStoreFloat4x4(&ConstantData.ViewMatrix, DirectX::XMMatrixTranspose(View));
	DirectX::XMStoreFloat4x4(&ConstantData.ProjectionMatrix, DirectX::XMMatrixTranspose(Projection));

	memcpy(CurrentFrame.ConstantBufferMappedData, &ConstantData, sizeof(TransformConstant));

	//삼각형 그리기
	CommandList->SetPipelineState(PipelineState);
	CommandList->SetGraphicsRootSignature(RootSignature);
	
	ID3D12DescriptorHeap* DescriptorHeaps[] = { SRVHeap };
	CommandList->SetDescriptorHeaps(1, DescriptorHeaps);

	CommandList->SetGraphicsRootConstantBufferView(0, CurrentFrame.ConstantBuffer->GetGPUVirtualAddress());
	CommandList->SetGraphicsRootDescriptorTable(1, SRVHeap->GetGPUDescriptorHandleForHeapStart());
	
	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE RTV = SwapChain.GetCurrentRTV();
	CommandList->OMSetRenderTargets(1, &RTV, FALSE, &DSV);

	const D3D12_VERTEX_BUFFER_VIEW& VBView = CubeMesh->GetVertexBufferView();
	const D3D12_INDEX_BUFFER_VIEW& IBView =	CubeMesh->GetIndexBufferView();

	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CommandList->IASetVertexBuffers(0, 1, &VBView);
	CommandList->IASetIndexBuffer(&IBView);
	
	CommandList->DrawIndexedInstanced(CubeMesh->GetIndexCount(), 1, 0, 0, 0);
	
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
	D3D12_DESCRIPTOR_RANGE SRVRange{};
	SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	SRVRange.RegisterSpace = 0;
	SRVRange.NumDescriptors = 1;
	SRVRange.BaseShaderRegister = 0; //t0
	SRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER RootParameters[2]{};
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor.ShaderRegister = 0; // b0
	RootParameters[0].Descriptor.RegisterSpace = 0;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[1].DescriptorTable.pDescriptorRanges = &SRVRange;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC SamplerDesc{};
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
	RootDesc.NumParameters = 2;
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
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			28,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc{};
	PipelineStateDesc.pRootSignature = RootSignature;
	PipelineStateDesc.VS = VS;
	PipelineStateDesc.PS = PS;
	PipelineStateDesc.InputLayout.NumElements= 3;
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

	if (FAILED(Device.GetDevice()->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&PipelineState))))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateDefaultBuffer(const void* Data, UINT64 Size, D3D12_RESOURCE_STATES FinalState, Microsoft::WRL::ComPtr<ID3D12Resource>& OutBuffer)
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
		&BufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(OutBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(Result))
	{
		return false;
	}

	CommandContext.Reset(Frame[0].CommandAllocator);
	CommandContext.GetCommandList()->CopyBufferRegion(OutBuffer.Get(), 0, UploadBuffer, 0, Size);

	D3D12_RESOURCE_BARRIER ResourceBarrier{};
	ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	ResourceBarrier.Transition.pResource = OutBuffer.Get();
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
	DepthBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
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

	D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc{};
	DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;
	DSVDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle = DSVHeap->GetCPUDescriptorHandleForHeapStart();
	Device.GetDevice()->CreateDepthStencilView(DepthBuffer, &DSVDesc, DSVHandle);

	return true;
}

bool Renderer::LoadImage(const wchar_t* FilePath, std::vector<uint8_t>& OutPixels, UINT& OutWidth, UINT& OutHeight)
{
	Microsoft::WRL::ComPtr<IWICImagingFactory> Factory;
	HRESULT Result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(Factory.GetAddressOf()));
	if (FAILED(Result))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IWICBitmapDecoder> Decoder;
	Result = Factory->CreateDecoderFromFilename(FilePath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, Decoder.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> Frame;
	Result = Decoder->GetFrame(0, Frame.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}

	Frame->GetSize(&OutWidth, &OutHeight);

	Microsoft::WRL::ComPtr<IWICFormatConverter> Converter;
	Result = Factory->CreateFormatConverter(Converter.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}
	Result = Converter->Initialize(Frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
		nullptr, 0.0, WICBitmapPaletteTypeCustom);
	if (FAILED(Result))
	{
		return false;
	}

	const UINT BytesPerPixel = 4;
	const UINT RowPitch = OutWidth * BytesPerPixel;
	const UINT ImageSize = RowPitch * OutHeight;
	OutPixels.resize(ImageSize);

	Result = Converter->CopyPixels(nullptr,	RowPitch, ImageSize, OutPixels.data());
	if (FAILED(Result))
	{
		return false;
	}
	return true;
}

bool Renderer::CreateTexture()
{
	std::vector<uint8_t> OutPixels;
	UINT TextureWidth = 0;
	UINT TextureHeight = 0;
	if (!LoadImage( L"Assets/Test.jpg", OutPixels, TextureWidth, TextureHeight))
	{
		return false;
	}

	D3D12_RESOURCE_DESC TextureDesc{};
	TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TextureDesc.MipLevels = 1;
	TextureDesc.Alignment = 0;
	TextureDesc.Width = TextureWidth;
	TextureDesc.Height = TextureHeight;
	TextureDesc.DepthOrArraySize = 1;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	TextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES DefaultHeapProperties{};
	DefaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT Result = Device.GetDevice()->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &TextureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&Texture));
	if (FAILED(Result))
	{
		return false;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT FootPrint{};
	UINT NumRow;
	UINT64 RowSize;
	UINT64 UploadBufferSize;
	Device.GetDevice()->GetCopyableFootprints(&TextureDesc, 0, 1, 0, &FootPrint, &NumRow, &RowSize, &UploadBufferSize);

	ID3D12Resource* TextureUploadBuffer = nullptr;

	D3D12_RESOURCE_DESC UploadDesc{};
	UploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	UploadDesc.Width = UploadBufferSize;
	UploadDesc.Height = 1;
	UploadDesc.MipLevels = 1;
	UploadDesc.DepthOrArraySize = 1;
	UploadDesc.SampleDesc.Count = 1;
	UploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_HEAP_PROPERTIES UploadHeapProperties{};
	UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	Result = Device.GetDevice()->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &UploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&TextureUploadBuffer));
	if (FAILED(Result))
	{
		return false;
	}

	uint8_t* MappedData = nullptr;
	TextureUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&MappedData));
	for (int i = 0; i < NumRow; i++)
	{
		const uint8_t* Src = OutPixels.data() + i * RowSize;
		uint8_t* Dest = MappedData + FootPrint.Offset + i * FootPrint.Footprint.RowPitch;

		memcpy(Dest, Src, RowSize);
	}

	D3D12_TEXTURE_COPY_LOCATION Dst{};
	Dst.pResource = Texture;
	Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	Dst.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION Src{};
	Src.pResource = TextureUploadBuffer;
	Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	Src.PlacedFootprint = FootPrint;

	TextureUploadBuffer->Unmap(0, nullptr);

	CommandContext.Reset(Frame[0].CommandAllocator);
	CommandContext.GetCommandList()->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
	
	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = Texture;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	CommandContext.GetCommandList()->ResourceBarrier(1, &Barrier);

	CommandContext.Close();

	CommandQueue.Execute(&CommandContext);

	UINT64 FenceValue = CommandQueue.Signal();
	if (FenceValue == 0)
	{
		return false;
	}
	CommandQueue.WaitForFence(FenceValue);

	TextureUploadBuffer->Release();
	TextureUploadBuffer = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC SRVHeapDesc{};
	SRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	SRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	SRVHeapDesc.NumDescriptors = 1;
	SRVHeapDesc.NodeMask = 0;

	Result = Device.GetDevice()->CreateDescriptorHeap(&SRVHeapDesc, IID_PPV_ARGS(&SRVHeap));
	if (FAILED(Result))
	{
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	Device.GetDevice()->CreateShaderResourceView(Texture, &SRVDesc, SRVHeap->GetCPUDescriptorHandleForHeapStart());

	return true;
}

std::unique_ptr<Mesh> Renderer::CreateMesh(const MeshData& Data)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> LocalVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> LocalIndexBuffer;

	const UINT VertexBufferSize = Data.Vertices.size() * sizeof(Vertex);
	const UINT IndexBufferSize = Data.Indices.size() * sizeof(uint32_t);

	if (!CreateDefaultBuffer(Data.Vertices.data(), VertexBufferSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, LocalVertexBuffer))
	{
		return nullptr;
	}
	if (!CreateDefaultBuffer(Data.Indices.data(), IndexBufferSize, D3D12_RESOURCE_STATE_INDEX_BUFFER, LocalIndexBuffer))
	{
		return nullptr;
	}

	return std::make_unique<Mesh>(std::move(LocalVertexBuffer), VertexBufferSize, sizeof(Vertex),
		std::move(LocalIndexBuffer), IndexBufferSize, static_cast<UINT>(Data.Indices.size()));
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

