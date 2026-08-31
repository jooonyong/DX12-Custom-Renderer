#define CGLTF_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include "GLTFLoader.h"
#include "cgltf.h"

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
			if (Primitive->type == cgltf_primitive_type_triangles)
			{
				const cgltf_accessor* PositionAccessor = nullptr;
				const cgltf_accessor* NormalAccessor = nullptr;
				const cgltf_accessor* UVAccessor = nullptr;
				
				for (auto i = 0; i < Primitive->attributes_count; i++)
				{
					cgltf_attribute Attribute = Primitive->attributes[i];
					switch (Attribute.type)
					{
					case cgltf_attribute_type_position:
						PositionAccessor = Attribute.data;
						break;

					case cgltf_attribute_type_normal:
						NormalAccessor = Attribute.data;
						break;

					case cgltf_attribute_type_texcoord:
						if (Primitive->attributes->index == 0)
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

				for (auto i = 0; i < VertexCount; i++)
				{
					float Position[3]{};
					float Normal[3]{};
					float UV[3]{};

					if (!cgltf_accessor_read_float(PositionAccessor, i, Position, 3))
					{
						cgltf_free(Data);
						return false;
					}
					if (!cgltf_accessor_read_float(NormalAccessor, i, Normal, 3))
					{
						cgltf_free(Data);
						return false;
					}
					if (UVAccessor)
					{
						if (!cgltf_accessor_read_float(UVAccessor, i, UV, 2))
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
					OutModel.Mesh.Vertices[i] = VertexData;
				}

				cgltf_accessor* IndexAccessor = Primitive->indices;
				if (IndexAccessor)
				{
					OutModel.Mesh.Indices.resize(IndexAccessor->count);
					for (auto i = 0; i < IndexAccessor->count; i++)
					{
						cgltf_size Index = cgltf_accessor_read_index(IndexAccessor, i);
						OutModel.Mesh.Indices[i] = Index;
					}
				}
				else //indices가 없을때
				{
					OutModel.Mesh.Indices.resize(VertexCount);

					for (auto i = 0; i < VertexCount; ++i)
					{
						OutModel.Mesh.Indices[i] = i;
					}
				}
				
				if (Primitive->material)
				{
					const cgltf_material* GLTFMaterial = Primitive->material;
					if (GLTFMaterial->has_pbr_metallic_roughness)
					{
						const auto& PBR = GLTFMaterial->pbr_metallic_roughness;
						OutModel.Material.BaseColor =
						{
							PBR.base_color_factor[0],
							PBR.base_color_factor[1],
							PBR.base_color_factor[2],
							PBR.base_color_factor[3]
						};
						OutModel.Material.Roughness = PBR.roughness_factor;
						OutModel.Material.Metallic = PBR.metallic_factor;
					}
				}
			}
		}
	}
	cgltf_free(Data);
	return true;
}
