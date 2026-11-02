# DirectX 12 Custom Renderer

DirectX 12 기반의 실시간 렌더러를 직접 구현하며 GPU 파이프라인, Resource 관리, Deferred Rendering, RenderGraph, 그리고 DXR까지 단계적으로 학습하기 위한 개인 프로젝트입니다.

단순히 화면에 오브젝트를 출력하는 것을 목표로 하지 않고, CPU에서 생성된 데이터가 GPU Resource로 전달되고 여러 Render Pass를 거쳐 최종 픽셀로 출력되기까지의 흐름을 직접 구현하면서 현대 렌더링 엔진의 구조를 이해하는 것을 목표로 합니다.

> 기준일: 2026-11-02. 확인한 최신 구현 커밋은 [`3bd367b`](https://github.com/jooonyong/DX12-Custom-Renderer/commit/3bd367bd7ca87069836ebc3e86614178da76eb70) (2026-10-31)입니다. Raster / DXR / Hybrid 모드, GBuffer 기반 RT Shadow·1회 Reflection의 Deferred 합성, Reflection 누적·공간 필터, Main / Render Thread 분리와 Pass별 Command List 제출까지 구현했습니다. Worker 병렬 Command Recording과 정량 성능·동기화 검증은 진행할 과제입니다.

**1차 완료 목표: 2026년 11월 30일 — Raster와 DXR을 결합하고, 안전한 멀티스레드 렌더링 구조와 성능 비교를 갖춘 자체 렌더러.**

구현 상태와 향후 계획을 구분해 기록합니다. 아래 완료 표시는 코드에 구현된 경로를 의미하며, 모든 입력·장시간 실행·GPU 검증·성능 검증의 완료를 뜻하지 않습니다. 정량 성능 결과는 측정 후 별도로 기록할 예정입니다.

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
- 1차 완료 범위인 Render Thread / Worker Thread 기반 병렬 Command Recording

---

## Current Rendering Pipeline

키 `1` / `2` / `3`으로 Raster / DXR / Hybrid 모드를 요청합니다. Main Thread가 요청한 모드는 프레임 데이터와 함께 Render Thread로 전달됩니다.

| 모드 | RenderGraph에 등록하는 Pass | 최종 SceneColor |
|---|---|---|
| Raster (`1`) | GBufferPass → ShadowPass → DeferredLightingPass → ToneMapping | GBuffer 기반 PBR + Shadow Map |
| DXR (`2`) | RaytracingPass → CopyRaytracingOutput → ToneMapping | 카메라 primary ray 기반 PBR + RT Hard Shadow |
| Hybrid (`3`) | GBufferPass → RaytracedShadow / RaytracedReflectionPass → ReflectionAccumulationPass → DeferredLightingPass → ToneMapping | Raster 표면 + RT Shadow + 누적 Reflection |

표는 주요 데이터 흐름입니다. 실제 기록 순서는 RenderGraph의 dependency / topological sort가 계산합니다. GBufferPass와 ShadowPass는 Raster 모드에서 서로 직접적인 의존성이 없습니다. Hybrid는 Shadow Map Pass를 등록하지 않습니다. TLAS / SBT 최초 생성은 현재 모드 분기 전에 수행합니다.

### Hybrid Pipeline — Implemented

```mermaid
flowchart TD
    A["GBuffer + Depth"] --> B["RT Shadow"]
    A --> C["RT Reflection"]
    T["TLAS / SBT"] --> B
    T --> C
    C --> D["Reflection Filter / History"]
    H["Previous History"] --> D
    A --> E["Deferred Lighting"]
    B --> E
    D --> E
    E --> F["HDR SceneColor / Tone Mapping"]
```

DXR 모드에서만 전체 화면 RaytracingOutput을 SceneColor로 복사합니다. Hybrid 모드에서는 Shadow Mask와 Reflection History를 Deferred Lighting에서 합성합니다. 모드별 출력 비교와 성능 측정 결과는 아직 문서화하지 않았습니다.

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
 ├─ GBufferPassCommand (Allocator / Context)
 ├─ ShadowPassCommand (Allocator / Context)
 ├─ CommandAllocator
 ├─ FenceValue
 ├─ Object Constant Buffer
 ├─ Scene Constant Buffer
 ├─ Directional Light Constant Buffer
 ├─ Shadow Pass Constant Buffer
 ├─ Deferred Pass Constant Buffer
 └─ Reflection Accumulation Constant Buffer
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
 ├─ Materials
 └─ BLAS

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
- `Mesh`와 BLAS는 `RenderModel`이 `unique_ptr`로 소유합니다.
- 같은 `RenderModel`을 공유하는 Object는 Mesh / Material / BLAS를 공유하고, World Transform은 각자 가집니다.
- Pass는 Scene 전체를 직접 해석하지 않고 Frame 시작 시 생성한 `DrawCommand`를 사용해 Draw를 기록합니다.

DXR에서는 RenderModel의 BLAS를 여러 TLAS Instance에서 재사용합니다. 현재 Main Thread는 Scene / Camera를 값으로 복사한 `RenderFrameData`를 Render Thread에 전달합니다. Scene의 RenderObject는 모델을 `shared_ptr`로 공유하므로, 이는 GPU Asset까지 깊게 복사한 snapshot이나 타입 수준에서 불변성이 보장된 구조는 아닙니다. 소비 중인 공유 모델·재질의 변경 정책과 GPU Fence 기반 수명 관리는 별도로 검증해야 합니다.

### Procedural Model Input

`CreateRenderModel(FilePath)`는 glTF를 CPU `ModelData`로 읽고, `CreateRenderModel(const ModelData&)`가 GPU Mesh / Material / BLAS를 생성합니다. 직접 생성한 Cube Mesh도 같은 GPU 생성 경로를 사용합니다. 업로드의 Begin/End는 공통 GPU 생성 경로에서 관리합니다.

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
- RaytracingOutput (UAV / Copy Source)
- RaytracedShadowMask / RaytracedReflection
- ReflectionHistory 2개 (이전 SRV / 출력 UAV / Deferred 입력 SRV)

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

Raster 경로의 주요 dependency는 다음 형태입니다. DXR / Hybrid 모드에서는 위 모드 표에 해당하는 Pass만 등록합니다.

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

`RenderGraph::Execute()`는 AddPass 순서가 아니라 계산된 `ExecutionOrder`를 따라 Pass를 기록합니다. 현재 GBufferPass / ShadowPass에는 각 Frame의 전용 Command List를 선택하고, 나머지 Pass는 공통 Command List를 사용합니다. 기록은 Render Thread에서 순차 수행하며, 실제 Worker 병렬 recording은 아직 없습니다.

제출은 현재 GBuffer → Shadow → 공통 Command List의 고정 배열로 수행합니다. 사용하지 않는 Pass의 list도 reset / close 후 배열에 포함됩니다. 이 구조는 현재 경로를 위한 중간 단계이며, 향후 Pass 배치가 달라지면 그래프의 기록 순서·barrier 상태와 실제 GPU 제출 순서가 일치하도록 submission batch를 설계해야 합니다.

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

### Implemented

- Ray Tracing Tier 확인 및 `ID3D12Device5` / `ID3D12GraphicsCommandList4` 확보
- Mesh의 Vertex / Index Buffer와 Submesh 범위로 BLAS 생성
- RenderObject의 World Transform을 TLAS Instance에 반영
- DXIL Library / State Object / Global·Local Root Signature 구성
- Ray Generation / Miss / Closest Hit Shader 및 `DispatchRays()`
- 카메라 상수 버퍼와 inverse view-projection을 이용한 primary ray 생성
- Triangle index fetch 및 barycentric 기반 UV / Normal / Tangent 보간
- BaseColor / Metallic-Roughness / Normal Texture 연결
- Cook-Torrance PBR 및 월드 공간 TBN 기반 Normal Mapping
- Shadow Payload와 별도 Shadow Hit/Miss 경로
- Geometric normal 기반 origin offset 및 불투명 방향광 hard shadow
- 프레임별 Hit Table, 공통 Miss Table, HDR DXR 출력과 Tone Mapping 연결
- GBuffer Depth 복원 위치에서 RT Shadow / Reflection Ray 생성
- Hybrid Deferred Lighting에 RT Shadow Mask / Reflection 합성
- GGX 방향 샘플링, 픽셀당 4개 Reflection Ray 평균과 BRDF 가중치
- Compute 기반 Reflection 공간 필터 / ping-pong History 누적

### Binding Layout

| 범위 | HLSL binding | 전달 방식 |
|---|---|---|
| Global | t0 TLAS, u0 Output | Descriptor Table |
| Global — Hybrid | t6~t9 GBuffer A/B/C / Depth | Descriptor Table |
| Global — Hybrid | u1 Shadow Mask, u2 Reflection | Descriptor Table |
| Global | b0 Camera, b1 Light | Root CBV / GPU VA |
| Global | s0 MaterialSampler | Linear/Wrap Static Sampler |
| Local — Camera Hit Group | t1 Vertex, t2 Index | Root SRV / GPU VA |
| Local — Camera Hit Group | b2 Material | Root CBV / GPU VA |
| Local — Camera Hit Group | t3 BaseColor, t4 MR, t5 Normal | 개별 Descriptor Table / GPU Descriptor Handle |

Texture SRV는 개별 할당되므로, 각각 descriptor 1개를 가리키는 table을 사용합니다.

### Shader Binding Table

서브메시마다 카메라용 / Shadow용 record를 연속 배치합니다.

| Record | 내용 |
|---|---|
| Geometry 0 / Camera | HitGroup Identifier + Vertex VA + Index VA + Material VA + Texture Handle 3개 |
| Geometry 0 / Shadow | ShadowHitGroup Identifier |
| Geometry 1 / Camera | 다음 Geometry의 재질·메시 인자 |
| Geometry 1 / Shadow | ShadowHitGroup Identifier |

- Camera record: Identifier 32 + Local Arguments 48 = 80바이트, 정렬 후 **96바이트**.
- Shadow record: Identifier 32바이트만 사용하며, 같은 Hit Table의 **96바이트 stride**를 공유합니다.
- Miss Table: `Miss` / `ShadowMiss` 두 record, 각 32바이트, 총 64바이트.
- 카메라 TraceRay 선택값: ray offset 0 / geometry multiplier 2 / miss index 0.
- Shadow TraceRay 선택값: ray offset 1 / geometry multiplier 2 / miss index 1.
- TLAS instance offset은 이전 Object들의 서브메시 수 × 2만큼 누적합니다.

```text
HitRecordIndex = InstanceOffset + GeometryIndex × GeometryMultiplier + RayOffset
```

### Ray-Traced Shadow

`ClosestHit`에서 표면으로부터 빛 방향으로 Shadow Ray를 발사합니다.

- 삼각형 위치에서 구한 geometric normal로 시작점 offset 방향 선택
- 현재 origin bias / TMin: 각각 0.001 월드 단위
- TMax: 10000 월드 단위
- `RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH` 사용
- Shadow hit는 Occluded = 1, miss는 0 기록
- 최종 색: `Ambient + DirectLighting × (1 - Occluded)`
- Camera Ray 안에서 Shadow Ray를 호출하므로 최대 추적 깊이는 2

Bias와 검사 거리는 현재 테스트 Scene 기준의 실험값이며, 모든 크기의 장면에 대한 보장값은 아닙니다.

### Reflection Accumulation — Current Implementation

- `ReflectionRayGen`이 프레임마다 달라지는 GGX 방향을 샘플링하고, 픽셀당 4개 ray 결과에 BRDF 가중치를 적용합니다.
- `ReflectionAccumulation.hlsl`의 8×8 Compute Dispatch에서 현재 Reflection을 필터링하고 이전 History와 합칩니다.
- 현재 공간 필터는 **5×5 Gaussian × Normal 유사도**입니다. Depth 기반 위치 복원·가중치 계산 코드는 있지만, 최종 weight의 `* DepthWeight`는 주석 처리되어 **Depth 가중 필터링은 비활성**입니다.
- History 두 장의 SRV/UAV를 교대 사용합니다. 누적 카운트는 64에서 제한하며, 이후 현재 프레임 weight는 `1 / 65`로 유지합니다. History write index는 별도로 교대합니다.
- ViewProjection 변화 감지와 모드 변경 시 누적을 초기화합니다.
- 현재는 동일 화면 픽셀의 History를 사용하며, reprojection / motion vector / disocclusion rejection은 없습니다. 카메라가 계속 이동하면 누적을 매번 초기화하므로 이동 중 노이즈가 남을 수 있습니다.
- 광원·재질·오브젝트 변경 전반의 History invalidation 정책과 필터 품질·GPU 비용은 검증 과제입니다.

### Current Limitations

- BLAS geometry는 불투명으로 취급합니다. Alpha Mask / 투명 재질의 그림자는 미지원입니다.
- TLAS와 Hit Table은 첫 프레임의 초기화 경로에서 생성합니다. 런타임 Object 이동·추가·삭제에 대한 갱신은 아직 구현하지 않았습니다.
- Texture는 현재 단일 mip과 `SampleLevel(..., 0)`을 사용합니다.
- 1회 Reflection / 시간 누적 / 기본 공간 필터는 구현했지만, 이동 중 품질·장면별 정확성과 비용 검증은 미완료입니다. RTAO / 다중 반사 / 고급 reprojection·denoising은 미구현입니다.
- Main / Render Thread는 분리했습니다. Worker Thread Pool / 병렬 Command Recording은 미구현입니다.
- RenderGraph는 기존 Resource의 의존성과 실행 순서를 다루는 최소 구현입니다. Transient 할당·Pass Culling·병렬 스케줄링은 미구현입니다.

---

## Validation Scene

현재 Scene에는 AK 모델 Instance 두 개와 직접 생성한 회색 바닥·큐브를 배치했습니다.

| 대상 | 구성 | 확인 목적 |
|---|---|---|
| glTF 모델 | 같은 RenderModel을 공유하는 Instance 두 개 | Texture / Submesh / Instance / SBT 선택 |
| 바닥 | Cube Scale (32, 0.1, 32), Position (0, -0.05, 0) | Y=0의 그림자 수신면 |
| 큐브 | Cube Scale (2, 2, 2), Position (0, 1, 0) | 바닥 접촉과 hard shadow |
| 바닥·큐브 재질 | BaseColor (0.5, 0.5, 0.5), Roughness 0.1, Metallic 0 | 텍스처 무늬와 분리한 조명 확인 |

Camera와 방향광 조건을 고정한 Raster / DXR / Hybrid 비교, 이동·정지 시 Reflection 품질, 모드 전환·반복 종료와 Debug Layer 검증이 필요합니다. Window Title에는 Present 경로의 atomic frame count를 이용한 FPS 표시를 추가했지만, 이는 Pass별 CPU/GPU profiling이나 성능 향상 검증을 대체하지 않습니다. Worker 성능 측정에는 별도로 Object / Draw 수를 늘린 부하 Scene이 필요합니다.

---

## Threading / Command Recording Progress

### Implemented

- Main Thread: OS 메시지 / Camera·Scene 입력 갱신 / 렌더 모드 요청 / 프레임 데이터 생성.
- `RenderFrameData`: Scene·Camera 값 복사, Mode, FrameNumber 전달. 모델·GPU Asset은 공유합니다.
- 최대 **2개 대기 프레임**의 queue를 mutex / condition variable로 보호합니다. Render Thread에서 처리 중인 프레임은 이 queue 크기에 포함되지 않습니다.
- queue가 가득 차면 새 프레임 제출을 거절하고 Main Thread가 1ms 대기합니다. 모든 생성 프레임을 반드시 렌더링하는 blocking queue는 아닙니다.
- Render Thread가 queue에서 프레임을 꺼내 `SetRenderMode()` / `RenderFrame()`을 호출합니다.
- 종료 시 stop flag / notify / Windows 메시지 처리와 함께 thread 종료 대기 / `join()` 경로가 있습니다. 대기 queue의 나머지 프레임은 종료 시 렌더링하지 않습니다.
- Frame별 GBuffer / Shadow allocator·context 분리, 순차 기록 후 3개 Command List 일괄 제출.
- 해당 Frame의 기존 Fence를 기다린 뒤 allocator / upload 영역을 재사용하는 경로가 있습니다.

### Not Yet Validated / Next

- 위 경로의 구현과 thread safety·안전한 종료가 검증됐다는 주장은 구분합니다. Render Thread 예외 전파, GPU 작업 완료 후 해제, 반복 종료·모드 전환은 테스트가 필요합니다.
- Main Thread와 Render Thread가 공유하는 GPU Asset / Material의 변경 권한·수명 정책을 확정해야 합니다.
- 실제 Worker recording은 아직 없습니다. 현재 단일 `ResourceStateTracker`를 그대로 여러 worker에서 변경해서는 안 되며, barrier 계획 / 제출 순서 / worker-local 상태 병합 정책이 필요합니다.
- Pass 이름 문자열 기반 list 선택을 명시적인 recording group / submission batch 구조로 일반화하는 것은 향후 과제입니다.
- 단일/멀티스레드 화면 일치, 적은 draw·많은 draw에서 CPU 기록 시간 / GPU 시간 / frame latency를 비교해야 합니다.

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
 ├─ AccelerationStructure
 ├─ GeometryGenerator
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
 ├─ Tone Mapping Shader
 ├─ Raytracing.hlsl
 └─ ReflectionAccumulation.hlsl
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
- BLAS / TLAS와 모델 공유·Instance Transform
- DXIL Export / State Object / Hit Group / Root Signature Association
- SBT 정렬과 record index 계산
- GPU VA와 GPU Descriptor Handle의 역할 차이
- DXR의 barycentric 보간·TBN·Texture Sampling
- Primary Ray와 Shadow Ray의 payload / hit / miss 분리
- Geometric Normal과 Shading Normal의 역할 차이
- Shadow Bias, Visibility, 중첩 TraceRay 깊이
- GBuffer 기반 Hybrid RT 효과와 Deferred 합성
- GGX Reflection 샘플링 / BRDF weight / History 누적·공간 필터
- Main / Render Thread 간 bounded queue·frame data 전달
- CPU thread 완료와 GPU Fence 완료의 구분
- Pass별 Command List 기록 / 제출 순서

---

## Roadmap

### Implemented Foundation

- [x] DirectX 12 Core / 프레임별 Fence·Constant Buffer
- [x] Mesh / Texture / Material / glTF Loading
- [x] Metallic-Roughness PBR / Normal Mapping
- [x] Shadow Mapping / GBuffer / Deferred Lighting
- [x] HDR SceneColor / Tone Mapping
- [x] Scene / RenderObject / RenderModel / DrawCommand 분리
- [x] ResourceStateTracker / 최소 RenderGraph / Dependency Analysis / Topological Sort
- [x] DXR Capability / BLAS / TLAS
- [x] RayGen / Miss / ClosestHit / State Object / SBT / DispatchRays
- [x] DXR Texture 기반 PBR / Normal Mapping
- [x] 불투명 방향광 Ray-Traced Hard Shadow
- [x] Procedural Model 입력 / 바닥·큐브 검증 Scene
- [x] Raster / DXR / Hybrid 모드별 RenderGraph 경로
- [x] GBuffer RT Shadow / Reflection의 Deferred 합성
- [x] Reflection History / 5×5 Gaussian·Normal 필터 기반
- [x] Main / Render Thread 분리 및 bounded frame queue
- [x] Frame별 GBuffer / Shadow Command List 분리 및 일괄 제출 (순차 기록)

### Required for the First Release — 2026-11-30 Target

- [ ] Shadow Map / RT Shadow 및 기능 ON/OFF 비교
- [ ] Reflection의 이동·정지 품질과 누적·필터 비용 검증
- [ ] 동적 입력 변경 시 필요한 TLAS / SBT 갱신과 안전한 수명 관리
- [ ] 프레임 데이터가 공유하는 Asset의 불변성·변경 권한·수명 검증
- [ ] Worker Thread Pool / GBuffer 병렬 Command Recording
- [ ] 실제 Worker별·Frame별 Command Allocator / Command List와 barrier 정책
- [ ] CPU 작업 완료와 GPU Fence 완료를 구분한 재사용·종료 처리
- [ ] 단일 / 멀티스레드 결과·성능 비교
- [ ] 데모 / 영상 / 구조도 / 제약 및 측정 결과 문서화

### First Release Schedule

기간은 **2026년**, 목표 완료일은 **11월 30일**입니다. 주당 약 10~15시간의 학습·구현을 가정한 계획이며, 완료 보장이 아닌 진행 관리 기준입니다.

기존 목표일은 유지하되 아래 구현 현황과 검증 잔여를 분리합니다. Main / Render Thread 기반은 기존 11/9~11/15 계획보다 먼저 들어갔으며, Worker 병렬화 완료나 성능 향상을 뜻하지 않습니다.

| 기간 | 목표 | 2026-11-02 현황 / 완료 기준 |
|---|---|---|
| 10/16~10/18 | 모드 분리·기준 화면 | 모드 분리 구현, 동일 조건 비교 기록은 필요 |
| 10/19~10/25 | RT Shadow Deferred 통합 | Hybrid 합성 구현, Shadow Map / RT Shadow 검증 필요 |
| 10/26~11/1 | Reflection 구현·합성 | Reflection·누적·필터 및 Main/Render Thread 기반 구현, 품질·동기화 검증 필요 |
| 11/2~11/8 | 안정화·스레딩 기반 검증 | 화면 일치·History reset·queue·종료·Fence 검증, CPU/GPU 기준 시간 확보 |
| 11/9~11/15 | Snapshot·병렬화 설계 정리 | 공유 Asset 수명·불변성, recording group / barrier·제출 정책 확정 |
| 11/16~11/22 | Worker 병렬 Command Recording | 실제 동시 기록, Worker/Frame별 allocator·list, 작업 완료 후 제출 |
| 11/23~11/26 | 정확성·성능 검증 | 단일/멀티스레드 비교, 작업량별 CPU/GPU 비용 기록 |
| 11/27~11/30 | 지연 대응·최종 정리 | 실행 데모·영상·README·구조도와 측정 결과 |

### Multithreaded Rendering Completion Criteria

단순히 렌더 함수를 다른 스레드에서 호출하는 것을 완료로 보지 않습니다.

1. Main Thread는 Scene을 갱신하고, Render Thread에는 프레임 단위의 불변 Snapshot을 전달합니다.
2. Snapshot이 참조하는 GPU Asset의 수명을 유지하며, 소비 중인 프레임 데이터를 덮어쓰지 않습니다.
3. Render Thread는 프레임을 준비하고, Worker들은 우선 GBuffer draw recording을 분담합니다.
4. 각 Worker는 Frame별 allocator / list를 사용하고 필요한 상태·바인딩을 자기 list에 기록합니다.
5. CPU Worker 완료 후 Command List를 의존 순서에 맞춰 제출합니다.
6. GPU가 사용하는 allocator / upload memory / descriptor는 해당 Fence 완료 전에 재사용·해제하지 않습니다.
7. 단일/멀티스레드 모드에서 결과가 일치하고, 종료 시 진행 중인 작업을 안전하게 정리합니다.
8. 적은 draw와 많은 draw에서 모두 측정해 작업 분배 비용과 이득을 기록합니다. 속도 향상은 측정 전제이며 보장하지 않습니다.

### Scope Control

- 매끄러운 표면의 1회 Reflection을 우선 완성합니다.
- 확률적 샘플링을 도입해 노이즈가 생기는 범위에 맞춰 누적·필터링을 적용합니다. 매끄러운 반사에 시간 누적을 무조건 추가하지 않습니다.
- 현재 GBuffer / Shadow의 Pass별 Command List 분리는 구현됐지만, 기록은 순차입니다. 실제 병렬화는 독립 Pass 동시 기록과 GBuffer draw 분할 중 검증 가능한 작은 범위부터 적용합니다.
- 정확성·동기화 검증 시간을 유지하고, 지연 시 시각적 확장부터 축소합니다.

### After the First Release

- RTAO / Soft Shadow / 거친 반사 / 다중 반사
- 고급 Temporal Reprojection / Denoising
- IBL 및 확장된 재질·환경 조명
- Logical RenderGraph Resource / Resource Version / Pass Culling
- Transient Resource / Aliasing / Barrier Batching
- Async Compute / Multi Queue Scheduling
- 광범위한 glTF·재질 지원과 Asset Pipeline 개선

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

Raster Deferred Renderer를 기반으로 RenderGraph와 DXR을 결합한 Hybrid Renderer로 확장하고, Render Thread와 Worker Thread를 분리해 실제 엔진에 가까운 멀티스레드 렌더링 구조를 구현합니다.

1차 결과물은 기능 목록뿐 아니라 **데이터 흐름, Ownership/Lifetime, CPU/GPU 동기화, 병렬화의 정확성과 측정된 비용**을 설명할 수 있는 형태로 정리합니다.
