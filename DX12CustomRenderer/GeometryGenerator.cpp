#include "GeometryGenerator.h"
#include "DirectXMath.h"

MeshData GeometryGenerator::CreateCube()
{
    MeshData Data;
	Data.Vertices = {
		//Front
		{
			{  0.5f, 0.5f, 0.5f },
			{  1.0f, 0.0f, 0.0f, 1.0f },
			{  1.0f, 0.0f },
			{0.0f , 0.0f, 1.0f}
		},
		{
			{  -0.5f, 0.5f, 0.5f },
			{  0.0f, 1.0f, 0.0f, 1.0f },
			{  0.0f, 0.0f },
			{0.0f , 0.0f, 1.0f}
		},
		{
			{ -0.5f, -0.5f, 0.5f },
			{  0.0f,  0.0f, 1.0f, 1.0f },
			{ 0.0f, 1.0f },
			{0.0f , 0.0f, 1.0f}
		},
		{
			{0.5f, -0.5f, 0.5f},
			{0.3f, 0.2f, 0.6f, 1.0f},
			{1.0f, 1.0f},
			{0.0f , 0.0f, 1.0f}
		},
		//Top
		{
			{  0.5f, 0.5f, -0.5f },
			{  1.0f, 0.0f, 0.0f, 1.0f },
			{  1.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f}
		},
		{
			{  -0.5f, 0.5f, -0.5f },
			{  0.0f, 1.0f, 0.0f, 1.0f },
			{  0.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f}
		},
		{
			{ -0.5f, 0.5f, 0.5f },
			{  0.0f,  0.0f, 1.0f, 1.0f },
			{  0.0f, 1.0f },
			{ 0.0f, -1.0f, 0.0f}
		},
		{
			{0.5f, 0.5f, 0.5f},
			{0.3f, 0.2f, 0.6f, 1.0f},
			{1.0f, 1.0f},
			{ 0.0f, -1.0f, 0.0f}
		},
		//Right
		{
			{ 0.5f, 0.5f, -0.5f },
			{  1.0f, 0.0f, 0.0f, 1.0f },
			{  1.0f, 0.0f },
			{ -1.0f, 0.0f, 0.0f}
		},
		{
			{  0.5f, 0.5f, 0.5f },
			{  0.0f, 1.0f, 0.0f, 1.0f },
			{  0.0f, 0.0f },
			{ -1.0f, 0.0f, 0.0f}
		},
		{
			{  0.5f, -0.5f, 0.5f },
			{  0.0f,  0.0f, 1.0f, 1.0f },
			{  0.0f, 1.0f },
			{ -1.0f, 0.0f, 0.0f}
		},
		{
			{0.5f, -0.5f, -0.5f},
			{0.3f, 0.2f, 0.6f, 1.0f},
			{1.0f, 1.0f},
			{ -1.0f, 0.0f, 0.0f}
		},
		//Left
		{
			{ -0.5f, 0.5f, 0.5f },
			{  1.0f, 0.0f, 0.0f, 1.0f },
			{  1.0f, 0.0f },
			{  1.0f, 0.0f, 0.0f}
		},
		{
			{  -0.5f, 0.5f, -0.5f },
			{  0.0f, 1.0f, 0.0f, 1.0f },
			{  0.0f, 0.0f },
			{  1.0f, 0.0f, 0.0f}
		},
		{
			{ -0.5f, -0.5f, -0.5f },
			{  0.0f,  0.0f, 1.0f, 1.0f },
			{  0.0f, 1.0f },
			{  1.0f, 0.0f, 0.0f}
		},
		{
			{ -0.5f, -0.5f, 0.5f},
			{0.3f, 0.2f, 0.6f, 1.0f},
			{1.0f, 1.0f},
			{  1.0f, 0.0f, 0.0f}
		},
		//Back
		{
			{  -0.5f, 0.5f, -0.5f },
			{  1.0f, 0.0f, 0.0f, 1.0f },
			{  1.0f, 0.0f },
			{ 0.0f, 0.0f, -1.0f}
		},
		{
			{  0.5f, 0.5f, -0.5f },
			{  0.0f, 1.0f, 0.0f, 1.0f },
			{  0.0f, 0.0f },
			{ 0.0f, 0.0f, -1.0f}
		},
		{
			{  0.5f, -0.5f, -0.5f },
			{  0.0f,  0.0f, 1.0f, 1.0f },
			{  0.0f, 1.0f },
			{ 0.0f, 0.0f, -1.0f}
		},
		{
			{ -0.5f, -0.5f, -0.5f},
			{0.3f, 0.2f, 0.6f, 1.0f},
			{1.0f, 1.0f},
			{ 0.0f, 0.0f, -1.0f}
		},
		//Bottom
		{
			{  0.5f, -0.5f, -0.5f },
			{  1.0f, 0.0f, 0.0f, 1.0f },
			{  1.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f}
		},
		{
			{  -0.5f, -0.5f, -0.5f },
			{  0.0f, 1.0f, 0.0f, 1.0f },
			{  0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f}
		},
		{
			{ -0.5f, -0.5f, 0.5f },
			{  0.0f,  0.0f, 1.0f, 1.0f },
			{  0.0f, 1.0f },
			{ 0.0f, 1.0f, 0.0f}
		},
		{
			{0.5f, -0.5f, 0.5f},
			{0.3f, 0.2f, 0.6f, 1.0f},
			{1.0f, 1.0f},
			{ 0.0f, 1.0f, 0.0f}
		} 
	};
	Data.Indices = {
		0, 2, 1,
		0, 3, 2,
		4,6,5,
		4,7,6,
		8,10,9,
		8,11,10,
		12,14,13,
		12,15,14,
		16,18,17,
		16,19,18,
		20,22,21,
		20,23,22
	};

    return Data;
}

MeshData GeometryGenerator::CreateSphere(float Radius, uint32_t SliceCount, uint32_t StackCount)
{
	MeshData Data;
	for (uint32_t Stack = 0; Stack <= StackCount; Stack++)
	{
		float Theta = PI * Stack / StackCount;
		for (uint32_t Slice = 0; Slice <= SliceCount; Slice++)
		{
			Vertex SphereVertex{};

			float Alpha = 2 * PI * Slice / SliceCount;
			float X = Radius * sin(Theta) * sin(Alpha);
			float Y = Radius * cos(Theta);
			float Z = Radius * sin(Theta) * cos(Alpha);

			DirectX::XMVECTOR P = DirectX::XMVectorSet(X, Y, Z, 0.0f);
			DirectX::XMVECTOR N = DirectX::XMVector3Normalize(P);
			
			float U = static_cast<float>(Slice) / SliceCount;
			float V = static_cast<float>(Stack) / StackCount;
			DirectX::XMVECTOR UV = DirectX::XMVectorSet(U, V, 0.0f, 0.0f);

			SphereVertex.Position = { X,Y,Z };
			DirectX::XMStoreFloat3(&SphereVertex.Normal, N);
			DirectX::XMStoreFloat2(&SphereVertex.UV, UV);

			Data.Vertices.push_back(SphereVertex);
		}
	}

	uint32_t RowCount = SliceCount + 1;
	for (uint32_t Stack = 0; Stack < StackCount; ++Stack)
	{
		for (uint32_t Slice = 0; Slice < SliceCount; ++Slice)
		{
			uint32_t A = Stack * RowCount + Slice;
			uint32_t B = A + 1;
			uint32_t C = (Stack + 1) * RowCount + Slice;
			uint32_t D = C + 1;

			Data.Indices.push_back(A);
			Data.Indices.push_back(B);
			Data.Indices.push_back(C);

			Data.Indices.push_back(B);
			Data.Indices.push_back(D);
			Data.Indices.push_back(C);
		}
	}
	return Data;
}
