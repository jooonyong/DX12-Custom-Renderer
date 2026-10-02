# DirectX 12 Custom Renderer

DirectX 12 기반의 실시간 렌더러를 직접 구현하며 GPU 파이프라인, Resource 관리, Deferred Rendering, RenderGraph, 그리고 DXR까지 단계적으로 학습하기 위한 개인 프로젝트입니다.

단순히 화면에 오브젝트를 출력하는 것을 목표로 하지 않고, CPU에서 생성된 데이터가 GPU Resource로 전달되고 여러 Render Pass를 거쳐 최종 픽셀로 출력되기까지의 흐름을 직접 구현하면서 현대 렌더링 엔진의 구조를 이해하는 것을 목표로 합니다.

> 현재 상태: glTF 모델 로딩, PBR + Normal Mapping, Directional Light Shadow Mapping, Deferred Rendering, HDR SceneColor + Tone Mapping, Scene/DrawCommand 분리, ResourceStateTracker, 최소 RenderGraph(Dependency 분석 + Topological Sort), DXR 지원 확인 및 `ID3D12GraphicsCommandList4` 인터페이스 확보까지 구현했습니다. 다음 단계는 Mesh 기반 BLAS 생성입니다.

---

## Goals

이 프로젝트에서 중점적으로 학습하고 있는 내용은 다음과 같습니다.

- DirectX 12의 명시적인 GPU Resource / Descriptor / Synchronization 모델
- CPU Scene 데이터와 GPU Draw 데이터의 분리
- Material / Texture / Mesh Asset Pipeline
- Deferred Rendering과 GBuffer 구성
- Shadow Mapping과 PBR Lighting
- HDR Intermediate Render Target과 Tone Mapping
- Render Pass 간 Resource State 관리
- RenderGraph를 이용한 Pass Dependency 분석과 실행 순서 계산
- DXR의 BLAS / TLAS / Ray Generation / Hit / Miss Pipeline
- 이후 Render Thread / Worker Thread 기반 병렬 Command Recording

---

## Current Rendering Pipeline

현재 한 Frame의 주요 렌더링 흐름은 다음과 같습니다.

```text
Scene
  ↓
BuildDrawCommand
  ↓
RenderGraph AddPass
  │
  ├─ GBufferPass
  │    ├─ GBuffer A : BaseColor
  │    ├─ GBuffer B : World Normal
  │    ├─ GBuffer C : Roughness / Metallic
  │    └─ Depth
  │
  ├─ ShadowPass
  │    └─ Directional Light Shadow Depth
  │
  ├─ DeferredLightingPass
  │    ├─ Read GBuffer / Depth / Shadow
  │    └─ Write HDR SceneColor
  │
  └─ ToneMapping
       ├─ Read HDR SceneColor
       └─ Write BackBuffer
  ↓
RenderGraph Compile
  ├─ Dependency Analysis
  └─ Topological Sort
  ↓
RenderGraph Execute
  ├─ ResourceStateTracker
  ├─ Resource Barrier
  └─ Pass Command Recording
  ↓
Execute Command List
  ↓
Present
```

---

## DirectX 12 Core

- D3D12 Device / Adapter 초기화
- High Performance GPU Adapter 선택
- Command Queue / Command Allocator / Command List 관리
- Swap Chain / Back Buffer 관리
- RTV / DSV / SRV 생성 및 관리
- Root Signature / Graphics PSO 구성
- Fence 기반 CPU-GPU 동기화
- Triple Buffering 기반 Frame Resource
- Upload Heap → Default Heap Resource Upload
- 명시적인 Resource Barrier 처리
- Shader Visible Descriptor Heap
- Descriptor 연속 할당 지원

### Frame Resource

각 BackBuffer index별로 Frame Resource를 분리해 이전 GPU 작업이 끝나기 전에 같은 Command Allocator / Constant Buffer 영역을 재사용하지 않도록 관리합니다.

```text
FrameResource
 ├─ CommandAllocator
 ├─ FenceValue
 ├─ Object Constant Buffer
 ├─ Scene Constant Buffer
 ├─ Directional Light Constant Buffer
 ├─ Shadow Pass Constant Buffer
 └─ Deferred Pass Constant Buffer
```

---

## Scene / Render Data Architecture

Asset, Instance, Draw 데이터를 분리했습니다.

```text
ModelData
  └─ CPU-side import result

RenderModel
 ├─ Mesh
 ├─ SubMeshes
 └─ Materials

RenderObject
 ├─ shared_ptr<RenderModel>
 └─ World Transform

Scene
 └─ RenderObject[]

DrawCommand
 ├─ Mesh*
 ├─ Material*
 ├─ IndexStart
 ├─ IndexCount
 └─ ObjectIndex
```

### Ownership

- `RenderModel`은 GPU-ready shared model asset입니다.
- 여러 `RenderObject`가 하나의 `RenderModel`을 공유할 수 있습니다.
- `Mesh`는 `RenderModel`이 `unique_ptr`로 소유합니다.
- Pass는 Scene 전체를 직접 해석하지 않고 Frame 시작 시 생성한 `DrawCommand`를 사용해 Draw를 기록합니다.

이 구조는 이후 Render Thread용 RenderScene / Proxy 구조와 DXR의 BLAS/TLAS 구조로 확장하기 위한 기반입니다.

---

## glTF Asset Loading

[cgltf](https://github.com/jkuhlmann/cgltf)를 사용해 glTF 데이터를 내부 Renderer 데이터로 변환합니다.

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
- BufferView Embedded Image

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
 ├─ SubMeshData
 └─ MaterialData
 ↓
RenderModel
 ├─ Mesh GPU Resources
 ├─ Materials
 └─ SubMesh Ranges
 ↓
RenderObject
 ↓
Scene
```

### Tangent Generation

glTF에 Tangent Attribute가 없는 경우 Position / UV / Index를 이용해 Triangle별 Tangent와 Bitangent를 계산합니다.

Vertex별 누적 후 Normal에 대해 Gram-Schmidt 직교화를 수행하고 Bitangent 방향을 이용해 Tangent Handedness를 계산합니다.

---

## Texture / Material System

### Texture

- WIC 기반 이미지 로딩
- PNG / JPG 지원
- Embedded glTF Image 로딩
- Upload Heap을 통한 GPU Texture Upload
- SRV 생성
- sRGB / Linear Color Space 구분

Color Space 규칙:

```text
BaseColor            → sRGB
Metallic-Roughness   → Linear
Normal Map           → Linear
```

Fallback Resource:

- White Texture: `(255, 255, 255, 255)`
- Default Normal: `(128, 128, 255, 255)`

### PBR Material

Cook-Torrance BRDF 기반 Metallic-Roughness PBR을 사용합니다.

- Lambert Diffuse
- GGX Normal Distribution Function
- Schlick Fresnel
- Smith Geometry Function
- BaseColor
- Roughness
- Metallic
- Normal Mapping
- Material Constant Buffer

glTF Metallic-Roughness Texture의 packing 규칙을 사용합니다.

```text
G Channel → Roughness
B Channel → Metallic
```

---

## GBuffer Pass

Deferred Rendering을 위해 3개의 MRT와 Depth Buffer를 사용합니다.

```text
GBuffer A : BaseColor
GBuffer B : World Space Normal
GBuffer C : Roughness / Metallic
Depth     : Device Z
```

Material Texture와 Constant Buffer를 GBuffer Pass에서 읽고 Lighting에 필요한 정보를 저장합니다.

---

## Deferred Lighting

GBuffer를 읽는 Fullscreen Triangle Pass로 Lighting을 계산합니다.

입력:

```text
GBuffer A
GBuffer B
GBuffer C
Depth
Directional Light
Shadow Map
Camera Position
Inverse ViewProjection
```

Depth를 이용해 World Position을 복원하고, Directional Light와 Cook-Torrance BRDF를 이용해 HDR SceneColor에 결과를 출력합니다.

```text
GBuffer + Depth
      ↓
World Position Reconstruction
      ↓
PBR Lighting
      ↓
Shadow Factor
      ↓
HDR SceneColor
```

---

## Directional Light Shadow Mapping

Directional Light 기준 Depth를 별도의 Shadow Map에 기록합니다.

- Shadow Map Resolution: 2048 × 2048
- Orthographic Light Projection
- Depth Bias
- 3 × 3 PCF

```text
World Position
     ↓
Light ViewProjection
     ↓
Shadow UV / Current Depth
     ↓
Shadow Map Compare
     ↓
Shadow Factor
```

Shadow는 현재 direct lighting에 적용하고 ambient term에는 적용하지 않습니다.

---

## HDR SceneColor / Tone Mapping

Deferred Lighting 결과를 BackBuffer에 바로 쓰지 않고 HDR Intermediate Render Target에 저장합니다.

```text
GBuffer
  ↓
Deferred Lighting
  ↓
SceneColor : R16G16B16A16_FLOAT
  ↓
Tone Mapping
  ↓
BackBuffer : R8G8B8A8_UNORM
```

Tone Mapping Pass는 Fullscreen Triangle을 사용하며 Exposure Root Constant를 전달하고 ACES 계열 tone mapping과 Linear → sRGB 변환을 수행합니다.

---

## Resource State Tracker

Pass마다 Resource Barrier를 수동으로 배치하던 구조에서 Resource의 현재 상태를 중앙에서 추적하는 구조로 변경했습니다.

```text
Pass
 ↓
Required Resource State
 ↓
ResourceStateTracker
 ↓
Current State 비교
 ↓
필요한 경우 D3D12_RESOURCE_BARRIER 기록
```

현재 추적 대상:

- GBuffer A / B / C
- Main Depth
- Shadow Depth
- SceneColor

BackBuffer의 PRESENT ↔ RENDER_TARGET 전환은 현재 Tone Mapping Pass 내부에서 처리합니다.

---

## RenderGraph

Render Pass가 자신이 사용하는 Resource와 Access 유형을 선언하도록 구성했습니다.

```cpp
enum class RenderGraphAccess
{
    Read,
    Write,
    ReadWrite
};
```

각 Pass는 다음 정보를 가집니다.

```text
RenderGraphPass
 ├─ Name
 ├─ ResourceUsages
 ├─ Dependencies
 └─ Execute
```

Resource Usage는 다음 정보를 표현합니다.

```text
Resource
RequiredState
Read / Write / ReadWrite
```

### Dependency Analysis

같은 Resource를 사용하는 Pass들에 대해 다음 hazard를 기준으로 dependency를 생성합니다.

```text
Read  → Read   : Dependency 없음
Write → Read   : RAW
Read  → Write  : WAR
Write → Write  : WAW
```

현재 Frame의 dependency는 다음 형태입니다.

```text
GBuffer ───────────┐
                   ▼
                Deferred ─────→ ToneMapping
                   ▲
                   │
Shadow ────────────┘
```

GBuffer와 Shadow는 서로 다른 Resource를 Write하므로 직접적인 dependency가 없습니다.

### Topological Sort

Dependency를 이용해 각 Pass의 indegree를 계산하고 Kahn Algorithm으로 실행 순서를 생성합니다.

```text
Dependencies
     ↓
Indegree 계산
     ↓
Indegree == 0인 Pass Queue 삽입
     ↓
Pass 처리
     ↓
해당 Pass를 기다리는 Pass들의 Indegree 감소
     ↓
새롭게 0이 된 Pass Queue 삽입
     ↓
ExecutionOrder
```

`RenderGraph::Execute()`는 AddPass 순서가 아니라 계산된 `ExecutionOrder`를 따라 Pass를 실행합니다.

현재 RenderGraph는 학습 목적의 최소 구현이며, 향후 다음 기능으로 확장할 예정입니다.

- Logical Resource / Resource Version
- Pass Culling
- Transient Resource
- Resource Lifetime
- Barrier Batching
- Parallel Command Recording
- Async Compute / Multi Queue Scheduling

---

## DXR Progress

DXR 구현을 시작했습니다.

### Implemented

- `D3D12_FEATURE_D3D12_OPTIONS5`를 통한 Ray Tracing 지원 확인
- `RaytracingTier` 검사
- `ID3D12Device5` QueryInterface
- `ID3D12GraphicsCommandList4` QueryInterface
- Raster / DXR Command를 동일한 underlying Command List에 기록할 수 있는 기반 구성

현재 구조:

```text
ID3D12Device
      ↓ QueryInterface
ID3D12Device5

ID3D12GraphicsCommandList
      ↓ QueryInterface
ID3D12GraphicsCommandList4
```

### Next DXR Step

다음 단계에서는 Mesh의 Vertex / Index Buffer를 이용해 BLAS(Bottom Level Acceleration Structure)를 생성합니다.

예정된 흐름:

```text
Mesh
 ↓
D3D12_RAYTRACING_GEOMETRY_DESC
 ↓
BLAS Build Inputs
 ↓
PrebuildInfo
 ↓
Scratch / Result Resource
 ↓
BuildRaytracingAccelerationStructure
 ↓
BLAS
```

이후:

```text
RenderModel → BLAS
RenderObject → TLAS Instance
Scene → TLAS
```

구조로 확장할 예정입니다.

---

## Project Structure

```text
DX12CustomRenderer/
 ├─ Application
 ├─ WindowsWindow
 │
 ├─ D3D12Device
 ├─ D3D12CommandQueue
 ├─ D3D12CommandContext
 ├─ D3D12SwapChain
 ├─ D3D12DescriptorAllocator
 ├─ D3D12ResourceUploader
 ├─ D3D12ResourceStateTracker
 │
 ├─ Renderer
 ├─ RenderGraph
 ├─ Scene
 ├─ RenderObject
 ├─ RenderModel
 ├─ DrawCommand
 │
 ├─ Mesh
 ├─ Material
 ├─ Texture
 │
 ├─ GLTFLoader
 ├─ ImageLoader
 │
 ├─ GBuffer Shader
 ├─ Shadow Shader
 ├─ Deferred Lighting Shader
 └─ Tone Mapping Shader
```

---

## What I Learned

현재까지 직접 구현하며 학습한 핵심 내용입니다.

- DirectX 12에서 Device / Queue / Allocator / Command List의 역할
- Fence를 이용한 CPU-GPU 동기화
- Triple Buffered Frame Resource
- Upload Heap과 Default Heap의 차이
- Descriptor Heap과 Root Signature
- Texture의 sRGB / Linear 처리
- glTF Mesh / Primitive / Material 구조
- Tangent Space와 Normal Mapping
- Cook-Torrance PBR
- Directional Light Shadow Mapping
- PCF
- MRT 기반 GBuffer
- Device Z에서 World Position 복원
- Deferred Lighting
- HDR Intermediate Render Target
- Tone Mapping
- Scene / RenderModel / RenderObject 역할 분리
- Scene 데이터에서 DrawCommand 생성
- Resource State 추적
- Render Pass Resource Usage 선언
- RAW / WAR / WAW Data Hazard
- DAG / Indegree / Topological Sort
- Kahn Algorithm
- RenderGraph Dependency 기반 실행
- COM QueryInterface와 D3D12 확장 Interface
- DXR Device / Command List 준비 과정

---

## Roadmap

### Rendering

- [x] DirectX 12 Core
- [x] Mesh / Texture / Material
- [x] glTF Loading
- [x] Normal Mapping
- [x] Metallic-Roughness PBR
- [x] Directional Light Shadow Mapping
- [x] GBuffer
- [x] Deferred Lighting
- [x] HDR SceneColor
- [x] Tone Mapping
- [x] Scene / RenderObject / RenderModel 구조
- [x] DrawCommand
- [x] ResourceStateTracker
- [x] Minimal RenderGraph
- [x] Pass Dependency Analysis
- [x] Topological Sort
- [x] DXR Capability / Interface Setup
- [ ] BLAS
- [ ] TLAS
- [ ] Ray Generation / Miss / Closest Hit Shader
- [ ] DXR State Object
- [ ] Shader Binding Table
- [ ] DispatchRays
- [ ] Ray Traced Shadow
- [ ] Ray Traced Reflection
- [ ] Temporal Accumulation / Denoising

### Engine Architecture

- [ ] Logical RenderGraph Resource
- [ ] Resource Lifetime / Transient Resource
- [ ] Descriptor Lifetime Management
- [ ] Upload / Resource Lifetime 개선
- [ ] Render Thread 분리
- [ ] Main Scene → Render Scene Snapshot
- [ ] Worker Thread Pool
- [ ] Parallel Command List Recording
- [ ] Per-Worker / Per-Frame Command Allocator
- [ ] PIX / RenderDoc / GPU Timing 기반 Profiling

---

## Development Direction

최종 목표는 특정 DirectX 12 API 호출을 암기하는 것이 아니라 다음과 같은 렌더링 엔진의 핵심 문제를 직접 해결해보는 것입니다.

```text
Scene
 ↓
Render Data
 ↓
Draw / Ray Tracing Commands
 ↓
RenderGraph
 ↓
Resource / Dependency / Scheduling
 ↓
D3D12
 ↓
GPU
```

Raster Deferred Renderer를 기반으로 RenderGraph와 DXR을 결합한 Hybrid Renderer로 확장하고, 이후 Render Thread와 Worker Thread를 분리해 실제 엔진에 가까운 멀티스레드 렌더링 구조까지 구현하는 것을 목표로 합니다.
