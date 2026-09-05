# DirectX 12 Renderer

DirectX 12와 실시간 그래픽스 파이프라인을 깊이 이해하기 위해 개발 중인 개인 렌더러 프로젝트입니다.

단순히 화면에 오브젝트를 출력하는 것을 목표로 하지 않고,  
GPU Resource 관리, Descriptor, Frame Resource, Lighting, PBR, Asset Loading 등
렌더링 엔진을 구성하는 주요 요소를 직접 구현하며 학습하는 것을 목표로 하고 있습니다.

---

## Current Features

### DirectX 12 Core

- D3D12 Device / Adapter 초기화
- Command Queue / Command Allocator / Command List 관리
- Swap Chain 및 Back Buffer 관리
- RTV / DSV 생성
- Fence 기반 CPU-GPU 동기화
- Triple Buffering 기반 Frame Resource 관리

### Resource Management

- Vertex / Index Buffer 생성
- Upload Heap을 이용한 Default Heap Resource 업로드
- Texture Resource 생성 및 업로드
- Shader Visible Descriptor Heap 관리
- SRV Descriptor 생성
- Constant Buffer 관리

### Rendering

- Indexed Mesh Rendering
- Depth Test
- Perspective Camera
- World / View / Projection Transform
- Normal Transform (`WorldInverseTranspose`)
- Directional Light

### PBR

Cook-Torrance BRDF 기반 기본적인 Metallic-Roughness PBR을 구현했습니다.

- Lambert Diffuse
- GGX Normal Distribution
- Schlick Fresnel
- Smith Geometry Function
- Metallic / Roughness Material Parameter
- sRGB BaseColor Texture 처리
- Linear Data Texture 처리

---

## glTF Asset Loading

[cgltf](https://github.com/jkuhlmann/cgltf)를 이용해 glTF 모델을
렌더러 내부 데이터 구조로 변환하는 Asset Loading Pipeline을 구현했습니다.

### Supported Data

- Position
- Normal
- Texcoord
- Index
- Multiple Primitives
- Multiple Materials
- BaseColor Factor
- Roughness Factor
- Metallic Factor
- BaseColor Texture
- Metallic-Roughness Texture binding/fallback

### Data Flow

```text
glTF
 ↓
cgltf
 ↓
GLTFLoader
 ↓
ModelData
 ├─ MeshData
 │   ├─ Vertices
 │   └─ Indices
 │
 ├─ SubMeshData
 │   ├─ IndexStart
 │   ├─ IndexCount
 │   └─ MaterialIndex
 │
 └─ MaterialData
     ├─ BaseColor
     ├─ Roughness
     ├─ Metallic
     └─ Texture Data
 ↓
GPU Resources
 ↓
DrawIndexedInstanced
