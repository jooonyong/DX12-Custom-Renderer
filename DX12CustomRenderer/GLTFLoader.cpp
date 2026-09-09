#define CGLTF_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include "GLTFLoader.h"
#include "ImageLoader.h"

#include <filesystem>

bool GLTFLoader::Load(const std::string& FilePath, ModelData& OutModel)
{
	cgltf_options Options;
	memset(&Options, 0, sizeof(cgltf_options));
	cgltf_data* Data = NULL;
	cgltf_result Result = cgltf_parse_file(&Options, FilePath.c_str(), &Data);

	if (Result == cgltf_result_success)
	{
		Result = cgltf_load_buffers(&Options, Data, FilePath.c_str());
		if (Data->meshes_count > 0 && Data->meshes->primitives_count > 0)
		{
			auto* Primitive = Data->meshes[0].primitives;
			OutModel.SubMeshes.resize(Data->meshes_count);

			if (Primitive->type == cgltf_primitive_type_triangles)
			{
				const cgltf_accessor* PositionAccessor = nullptr;
				const cgltf_accessor* NormalAccessor = nullptr;
				const cgltf_accessor* UVAccessor = nullptr;
				const cgltf_accessor* TangentAccessor = nullptr;

				for (auto j = 0; j < Primitive->attributes_count; j++)
				{
					cgltf_attribute Attribute = Primitive->attributes[j];
					switch (Attribute.type)
					{
					case cgltf_attribute_type_position:
						PositionAccessor = Attribute.data;
						break;

					case cgltf_attribute_type_normal:
						NormalAccessor = Attribute.data;
						break;

					case cgltf_attribute_type_tangent:
						TangentAccessor = Attribute.data;
						break;

					case cgltf_attribute_type_texcoord:
						if (Attribute.index == 0)
						{
							UVAccessor = Attribute.data;
						}
						break;
					}
				}
				if (!PositionAccessor || !NormalAccessor)
				{
					cgltf_free(Data);
					return false;
				}
				const cgltf_size VertexCount = PositionAccessor->count;
				OutModel.Mesh.Vertices.resize(VertexCount);

				for (auto j = 0; j < VertexCount; j++)
				{
					float Position[3]{};
					float Normal[3]{};
					float UV[2]{};
					float Tangent[4]{ 0.0f, 0.0f, 0.0f, 1.0f };

					if (!cgltf_accessor_read_float(PositionAccessor, j, Position, 3))
					{
						cgltf_free(Data);
						return false;
					}
					if (!cgltf_accessor_read_float(NormalAccessor, j, Normal, 3))
					{
						cgltf_free(Data);
						return false;
					}
					if (UVAccessor)
					{
						if (!cgltf_accessor_read_float(UVAccessor, j, UV, 2))
						{
							cgltf_free(Data);
							return false;
						}
					}
					if (TangentAccessor)
					{
						OutModel.Mesh.bHasTangent = true;
						if (!cgltf_accessor_read_float(TangentAccessor, j, Tangent, 4))
						{
							cgltf_free(Data);
							return false;
						}
					}
					Vertex VertexData;
					VertexData.Position = { Position[0], Position[1], Position[2] };
					VertexData.Normal = { Normal[0],Normal[1],Normal[2] };
					VertexData.UV = { UV[0], UV[1] };
					VertexData.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
					VertexData.Tangent = { Tangent[0], Tangent[1], Tangent[2], Tangent[3] };
					OutModel.Mesh.Vertices[j] = VertexData;
				}

				const cgltf_mesh& GLTFMesh = Data->meshes[0];

				OutModel.Mesh.Indices.clear();
				OutModel.SubMeshes.clear();
				OutModel.SubMeshes.reserve(GLTFMesh.primitives_count);

				for (cgltf_size PrimitiveIndex = 0; PrimitiveIndex < GLTFMesh.primitives_count; PrimitiveIndex++)
				{
					const cgltf_primitive& Primitive = GLTFMesh.primitives[PrimitiveIndex];
					
					SubMeshData SubMesh{};
					SubMesh.IndexStart = static_cast<uint32_t>(OutModel.Mesh.Indices.size());
					if (Primitive.indices)
					{
						const cgltf_accessor* IndexAccessor = Primitive.indices;

						for (cgltf_size i = 0; i < IndexAccessor->count; i++)
						{
							cgltf_size Index = cgltf_accessor_read_index(IndexAccessor, i);
							OutModel.Mesh.Indices.push_back(static_cast<uint32_t>(Index));
						}
					}
					else
					{
						for (cgltf_size i = 0;i < VertexCount; i++)
						{
							OutModel.Mesh.Indices.push_back(static_cast<uint32_t>(i));
						}
					}

					const uint32_t IndexEnd = static_cast<uint32_t>(OutModel.Mesh.Indices.size());
					SubMesh.IndexCount = IndexEnd - SubMesh.IndexStart;
					
					if (Primitive.material)
					{
						ptrdiff_t MaterialIndex = Primitive.material - Data->materials;
						if (MaterialIndex >= 0)
						{
							SubMesh.MaterialIndex = static_cast<uint32_t>(MaterialIndex);
						}
					}
					OutModel.SubMeshes.push_back(SubMesh);
				}
				if (!TangentAccessor)
				{
					if (!UVAccessor)
					{
						OutModel.Mesh.bHasTangent = false;
					}
					else
					{
						if (!GenerateTangent(OutModel.Mesh))
						{
							cgltf_free(Data);
							return false;
						}
						OutModel.Mesh.bHasTangent = true;
					}
				}
			}

			OutModel.Materials.resize(Data->materials_count);
			for (auto i = 0; i < Data->materials_count; i++)
			{
				const cgltf_material* GLTFMaterial = &Data->materials[i];
				if (GLTFMaterial->has_pbr_metallic_roughness)
				{
					const auto& PBR = GLTFMaterial->pbr_metallic_roughness;
					const cgltf_texture* Texture = PBR.base_color_texture.texture;
					const cgltf_texture* MRTexture = PBR.metallic_roughness_texture.texture;

					OutModel.Materials[i].BaseColor =
					{
						PBR.base_color_factor[0],
						PBR.base_color_factor[1],
						PBR.base_color_factor[2],
						PBR.base_color_factor[3]
					};
					OutModel.Materials[i].Roughness = PBR.roughness_factor;
					OutModel.Materials[i].Metallic = PBR.metallic_factor;

					if (Texture && Texture->image)
					{
						const cgltf_image* Image = Texture->image;
						ImageData ImageData{};
						if (!LoadImage(FilePath, Image, ImageData))
						{
							cgltf_free(Data);
							return false;
						}
						OutModel.Materials[i].BaseColorImage = std::move(ImageData);
					}
					if (MRTexture && MRTexture->image)
					{
						const cgltf_image* Image = MRTexture->image;
						ImageData ImageData{};
						if (!LoadImage(FilePath, Image, ImageData))
						{
							cgltf_free(Data);
							return false;
						}
						OutModel.Materials[i].MetallicRoughnessImage = std::move(ImageData);
					}
				}
				if (GLTFMaterial->normal_texture.texture)
				{
					const cgltf_texture* NormalTexture = GLTFMaterial->normal_texture.texture;
					const cgltf_image* NormalImage = NormalTexture->image;

					ImageData NormalData{};
					if (!LoadImage(FilePath, NormalImage, NormalData))
					{
						cgltf_free(Data);
						return false;
					}
					OutModel.Materials[i].NormalMapImage = std::move(NormalData);
				}

			}
		}
	}
	cgltf_free(Data);
	return true;
}

bool GLTFLoader::LoadImage(const std::string& FilePath, const cgltf_image* Image, ImageData& Data)
{
	//external texture
	if (Image->uri)
	{
		std::filesystem::path ModelPath(FilePath);
		std::filesystem::path ImagePath = ModelPath.parent_path() / Image->uri;

		if (!ImageLoader::LoadFromFile(ImagePath.wstring(), Data))
		{
			return false;
		}
	}
	//embedded texture
	else if (Image->buffer_view)
	{
		const uint8_t* CompressedData = cgltf_buffer_view_data(Image->buffer_view);
		size_t CompressedSize = Image->buffer_view->size;

		if (!ImageLoader::LoadFromMemory(CompressedData, CompressedSize, Data))
		{
			return false;
		}
	}
	return true;
}

bool GLTFLoader::GenerateTangent(MeshData& Mesh)
{
	using namespace DirectX;

	if (Mesh.Vertices.empty() || Mesh.Indices.empty() || Mesh.Indices.size() % 3 != 0)
	{
		return false;
	}

	const size_t VertexCount = Mesh.Vertices.size();
	std::vector<XMFLOAT3> TangentSums(VertexCount, XMFLOAT3{ 0.0f, 0.0f, 0.0f });
	std::vector<XMFLOAT3> BitangentSums(VertexCount,XMFLOAT3{ 0.0f, 0.0f, 0.0f });

	for (size_t i = 0; i < Mesh.Indices.size(); i += 3)
	{
		const uint32_t I0 = Mesh.Indices[i];
		const uint32_t I1 = Mesh.Indices[i + 1];
		const uint32_t I2 = Mesh.Indices[i + 2];

		if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
		{
			return false;
		}

		const Vertex& V0 = Mesh.Vertices[I0];
		const Vertex& V1 = Mesh.Vertices[I1];
		const Vertex& V2 = Mesh.Vertices[I2];

		XMVECTOR P0 = XMLoadFloat3(&V0.Position);
		XMVECTOR P1 = XMLoadFloat3(&V1.Position);
		XMVECTOR P2 = XMLoadFloat3(&V2.Position);

		XMVECTOR Edge1 = P1 - P0;
		XMVECTOR Edge2 = P2 - P0;

		float DU1 = V1.UV.x - V0.UV.x;
		float DV1 =	V1.UV.y - V0.UV.y;
		float DU2 =	V2.UV.x - V0.UV.x;
		float DV2 =	V2.UV.y - V0.UV.y;

		const float Det = DU1 * DV2 - DV1 * DU2;

		XMVECTOR Tangent = (DV2 * Edge1 - DV1 * Edge2) / Det;
		XMVECTOR Bitangent = (-DU2 * Edge1 + DU1 * Edge2) / Det;

		const uint32_t Indices[3] = { I0, I1, I2 };

		for (uint32_t Index : Indices)
		{
			XMVECTOR CurrentT = XMLoadFloat3(&TangentSums[Index]);
			XMVECTOR CurrentB = XMLoadFloat3(&BitangentSums[Index]);

			CurrentT += Tangent;
			CurrentB += Bitangent;

			XMStoreFloat3(&TangentSums[Index], CurrentT);
			XMStoreFloat3(&BitangentSums[Index], CurrentB);
		}
	}

	for (size_t i = 0; i < VertexCount; i++)
	{
		Vertex& V = Mesh.Vertices[i];
		XMVECTOR N = XMLoadFloat3(&V.Normal);

		N = XMVector3Normalize(N);
		XMVECTOR T = XMLoadFloat3(&TangentSums[i]);
		//그램슈미트 직교화로 T의 N성분 제거
		T = T - N * XMVector3Dot(N, T);

		XMVECTOR B = XMLoadFloat3(&BitangentSums[i]);
		XMVECTOR CalculatedB = XMVector3Cross(N, T);

		float Handedness = 1.0f;
		float Direction = XMVectorGetX(XMVector3Dot(CalculatedB, B));
		if (Direction < 0.0f)
		{
			Handedness = -1.0f;
		}

		XMFLOAT3 FinalTangent{};

		XMStoreFloat3(&FinalTangent,T);
		V.Tangent = 
		{
			FinalTangent.x,
			FinalTangent.y,
			FinalTangent.z,
			Handedness
		};
	}

	return true;
}
