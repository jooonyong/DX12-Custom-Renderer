# DirectX 12 Renderer

DirectX 12와 실시간 그래픽스 파이프라인을 깊이 이해하기 위해 개발 중인 개인 렌더러 프로젝트입니다.

단순히 오브젝트를 화면에 출력하는 것을 목표로 하지 않고, GPU Resource 관리, Descriptor, Frame Resource, Asset Loading, PBR, Shadow Mapping, GBuffer 등 렌더링 엔진을 구성하는 요소를 직접 구현하면서 **각 데이터가 CPU에서 생성되어 GPU Pipeline을 거쳐 최종 픽셀로 출력되는 과정**을 이해하는 것을 목표로 하고 있습니다.

> 현재 상태: glTF 모델과 Material을 로드해 PBR + Normal Mapping으로 렌더링하고 있으며, Directional Light Shadow Mapping과 GBuffer Pass까지 구현했습니다. GBuffer를 이용한 Deferred Lighting Pass는 다음 단계로 진행 중입니다.

---

## Current Features

### DirectX 12 Core

- D3D12 Device / Adapter 초기화
- Command Queue / Command Allocator / Command List 관리
- Swap Chain / Back Buffer 관리
- RTV / DSV 생성 및 관리
- Root Signature / Graphics PSO 구성
- Fence 기반 CPU-GPU 동기화
- Triple Buffering 기반 Frame Resource 관리
- Resource Barrier를 통한 명시적 Resource State Transition

### Resource Management

- Vertex / Index Buffer 생성
- Upload Heap → Default Heap Resource 업로드
- `D3D12ResourceUploader`를 통한 Buffer / Texture Upload 관리
- Shader Visible SRV Descriptor Heap 관리
- SRV Descriptor 동적 할당
- Constant Buffer 생성 및 Frame별 데이터 갱신
- Texture의 sRGB / Linear Color Space 구분
- 기본 White Texture / Normal Texture fallback

### Rendering

- Indexed Mesh Rendering (`DrawIndexedInstanced`)
- Depth Test
- Perspective Camera
- World / View / Projection Transform
- Normal Transform (`WorldInverseTranspose`)
- Directional Light
- Normal Mapping
- Tangent / Bitangent 기반 Tangent Space → World Space Normal 변환
- glTF에 Tangent가 없을 경우 Vertex Position / UV로 Tangent 생성

---

## PBR Material

Cook-Torrance BRDF 기반 Metallic-Roughness PBR을 구현했습니다.

- Lambert Diffuse
- GGX Normal Distribution Function
- Schlick Fresnel
- Smith Geometry Function
- BaseColor Factor / Texture
- Metallic Factor / Texture
- Roughness Factor / Texture
- Normal Map
- sRGB BaseColor Texture 처리
- Metallic-Roughness / Normal Texture Linear 처리

Material별 Constant Buffer와 Texture SRV를 바인딩하며, SubMesh의 `MaterialIndex`를 이용해 Draw 단위로 Material을 변경합니다.

```text
SubMesh
 ├─ IndexStart
 ├─ IndexCount
 └─ MaterialIndex
        ↓
     Material
     ├─ BaseColor Texture
     ├─ Metallic-Roughness Texture
     ├─ Normal Texture
     └─ Material Constant Buffer
```

---

## glTF Asset Loading

[cgltf](https://github.com/jkuhlmann/cgltf)를 사용해 glTF 데이터를 렌더러 내부 구조로 변환하는 Asset Loading Pipeline을 구현했습니다.

### Supported Data

- Position
- Normal
- Tangent
- Texcoord
- Index
- Multiple Primitives
- Multiple Materials
- BaseColor Factor
- Roughness Factor
- Metallic Factor
- BaseColor Texture
- Metallic-Roughness Texture
- Normal Texture
- External Image
- BufferView에 포함된 Embedded Image

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
 │   │   ├─ Position
 │   │   ├─ Normal
 │   │   ├─ UV
 │   │   └─ Tangent
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
     ├─ BaseColorImage
     ├─ MetallicRoughnessImage
     └─ NormalMapImage
 ↓
Mesh / Material / Texture GPU Resources
 ↓
DrawIndexedInstanced
```

### Tangent Generation

Normal Map을 사용하려면 Tangent Space가 필요합니다. glTF에 Tangent Attribute가 존재하면 해당 데이터를 사용하고, Tangent가 없는 경우 Position / UV / Index를 이용해 Triangle별 Tangent와 Bitangent를 계산합니다.

계산된 Tangent는 Vertex별로 누적한 뒤 Normal과 Gram-Schmidt 직교화를 수행하고, Bitangent 방향을 이용해 Tangent의 Handedness를 계산합니다.

---

## GBuffer Pass

Deferred Rendering 구조를 학습하기 위해 3개의 MRT(Render Target)를 사용하는 GBuffer Pass를 구현했습니다.

```text
GBuffer A : BaseColor
GBuffer B : World Space Normal
GBuffer C : Roughness / Metallic
```

GBuffer Pixel Shader에서는 Material Texture와 Constant Buffer를 이용해 각 정보를 출력하고, GBuffer Pass 종료 후 Resource를

```text
RENDER_TARGET
    ↓ ResourceBarrier
PIXEL_SHADER_RESOURCE
```

상태로 전환합니다.

현재는 GBuffer 생성 단계까지 구현되어 있으며, **Main Pass는 아직 Material Texture를 직접 샘플링하여 Lighting을 계산합니다.** 향후 GBuffer를 직접 읽는 Deferred Lighting Pass로 확장할 예정입니다.

---

## Directional Light Shadow Mapping

Directional Light를 기준으로 Scene Depth를 기록하는 Shadow Pass를 구현했습니다.

Directional Light는 평행광이므로 Light Projection에는 Orthographic Projection을 사용합니다.

### Shadow Pass

```text
World Position
      ↓
Light View
      ↓
Orthographic Projection
      ↓
Shadow Depth Texture
```

Shadow Pass에서는 Color Render Target 없이 DSV만 바인딩해 Light 기준 Depth를 기록합니다.

Shadow Pass 종료 후 Shadow Texture를 Main Pass의 Pixel Shader에서 읽기 위해 다음과 같이 상태를 전환합니다.

```text
DEPTH_WRITE
    ↓ ResourceBarrier
PIXEL_SHADER_RESOURCE
```

Main Pass 종료 후 다음 Frame의 Shadow Pass를 위해 다시 `DEPTH_WRITE` 상태로 복원합니다.

### Shadow Test

Main Pass의 Vertex를 동일한 `LightViewProjection`으로 변환한 뒤 NDC 좌표를 Shadow Map UV로 변환합니다.

```text
Current Pixel World Position
       ↓
Light View Projection
       ↓
Shadow UV + Current Depth
       ↓
Shadow Map Stored Depth와 비교
       ↓
Shadow Factor
```

현재 구현에는 다음 기능이 포함되어 있습니다.

- Shadow Map Resolution: 2048 × 2048
- Depth Bias
- 3 × 3 PCF(Percentage-Closer Filtering)
- Shadow Map Resource State Transition

---

## Frame Rendering Flow

현재 한 Frame은 다음 순서로 처리됩니다.

```text
Wait Current Frame Fence
        ↓
Update Transform / Light / Material Constant Buffers
        ↓
Update Shadow Matrices
        ↓
RenderGBufferPass
        ↓
RenderShadowPass
        ↓
RenderMainPass
        ↓
Close / Execute Command List
        ↓
Present
        ↓
Signal Fence
```

각 Frame Resource가 마지막으로 제출한 Fence 값을 보관하고, 해당 Frame Resource를 다시 사용하기 전에 GPU 작업 완료를 확인합니다.

---

## Project Structure

```text
DX12CustomRenderer/
 ├─ Application              - Application lifecycle / main loop
 ├─ WindowsWindow            - Win32 Window
 ├─ D3D12Device              - Device / Adapter
 ├─ D3D12CommandQueue        - Command Queue / Fence
 ├─ D3D12CommandContext      - Command List 관리
 ├─ D3D12SwapChain           - Swap Chain / Back Buffer
 ├─ D3D12DescriptorAllocator - SRV Descriptor 관리
 ├─ D3D12ResourceUploader    - GPU Resource Upload
 ├─ Renderer                 - Render Pass / Pipeline 관리
 ├─ Camera                   - View / Projection
 ├─ Mesh                     - Vertex / Index GPU Resource
 ├─ Material                 - Material Parameter / Texture
 ├─ Texture                  - Texture Resource / SRV
 ├─ GLTFLoader               - glTF → ModelData 변환
 ├─ ImageLoader              - Image File / Memory Loading
 ├─ GBufferShader.hlsl       - GBuffer Pass
 ├─ ShadowShader.hlsl        - Shadow Depth Pass
 └─ Triangle.hlsl            - Main PBR / Shadow Lighting Pass
```

---

## What I Learned

이 프로젝트를 진행하면서 DirectX 12에서는 단순히 API 호출 순서를 외우는 것보다 **Resource가 언제 생성되고, 어떤 상태로 사용되며, CPU와 GPU 사이에서 데이터가 언제 유효한지 이해하는 것**이 중요하다는 점을 배우고 있습니다.

특히 다음 개념을 직접 구현하며 학습했습니다.

- Upload Heap과 Default Heap의 역할 차이
- Descriptor Heap과 Root Signature를 통한 Resource Binding
- Frame Resource와 Fence를 이용한 CPU-GPU Synchronization
- glTF의 Mesh / Primitive / Accessor / Material 구조를 Renderer Data로 변환하는 과정
- Tangent Space와 Normal Mapping
- Cook-Torrance PBR의 각 BRDF 항이 Lighting에 미치는 영향
- Render Pass 간 Resource State Transition
- Light Space Depth를 이용한 Shadow Mapping
- PCF를 이용한 Shadow Edge Filtering
- MRT를 이용한 GBuffer 구성

---

## Next Steps

- [ ] GBuffer를 이용한 Deferred Lighting Pass
- [ ] GBuffer Depth / World Position Reconstruction
- [ ] Point / Spot Light
- [ ] Multiple Lights
- [ ] IBL(Image Based Lighting)
- [ ] Skybox / Environment Map
- [ ] Shadow Mapping 품질 개선
- [ ] Renderer / Render Pass 구조 리팩터링
- [ ] Resource Lifetime / Descriptor 관리 구조 개선

---

## Development Goal

최종 목표는 특정 API 사용법을 암기하는 것이 아니라, 직접 작은 Renderer를 구성하면서 **현대 실시간 렌더링 엔진의 구조와 GPU Pipeline을 이해하고 Unreal Engine의 Rendering / RDG 구조까지 연결해서 이해하는 것**입니다.
