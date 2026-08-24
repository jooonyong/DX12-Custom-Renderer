#include "Mesh.h"

Mesh::Mesh(Microsoft::WRL::ComPtr<ID3D12Resource> InVertexBuffer, UINT VertexBufferSize, UINT VertexStride, Microsoft::WRL::ComPtr<ID3D12Resource> InIndexBuffer, UINT IndexBufferSize, UINT InIndexCount)
	: VertexBuffer(std::move(InVertexBuffer)), IndexBuffer(std::move(InIndexBuffer)), IndexCount(InIndexCount)
{
	VBView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VBView.SizeInBytes = VertexBufferSize;
	VBView.StrideInBytes = VertexStride;

	IBView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
	IBView.Format = DXGI_FORMAT_R32_UINT;
	IBView.SizeInBytes = IndexBufferSize;
}

bool Mesh::Initialize(MeshData Data)
{
	return false;
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView()
{
	return VBView;
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView()
{
	return IBView;
}

UINT Mesh::GetIndexCount()
{
	return IndexCount;
}
