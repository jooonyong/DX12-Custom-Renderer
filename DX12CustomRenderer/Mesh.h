#pragma once

#include "d3d12.h"
#include <wrl.h>
#include "MeshData.h"

class Mesh
{
public:
	Mesh(
		Microsoft::WRL::ComPtr<ID3D12Resource> InVertexBuffer,
		UINT VertexBufferSize,
		UINT VertexStride,
		Microsoft::WRL::ComPtr<ID3D12Resource> InIndexBuffer,
		UINT IndexBufferSize,
		UINT InIndexCount);

	bool Initialize(MeshData Data);

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();

	UINT GetIndexCount();
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;

	D3D12_VERTEX_BUFFER_VIEW VBView{};
	D3D12_INDEX_BUFFER_VIEW IBView{};

	UINT IndexCount = 0;
};