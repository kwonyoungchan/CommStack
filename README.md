# CommStack: High-Performance Lock-Free Communication Framework

![C++ Standard](https://img.shields.io/badge/C++-17-blue.svg)
![.NET Version](https://img.shields.io/badge/.NET-8.0-purple.svg)
![Platform](https://img.shields.io/badge/Platform-Windows_x64-lightgrey.svg)
![Build](https://img.shields.io/badge/Build-MSVC-success.svg)

## 1. Project Overview
`CommStack`은 고성능 시뮬레이터의 실시간 데이터 처리와 초고속 네트워크 통신을 위해 설계된 C++ 네이티브 기반 하이브리드 통신 프레임워크입니다. 
동적 메모리 할당을 제거한 **Zero-Allocation Memory Pool**과 뮤텍스를 배제한 **SPSC(Single Producer Single Consumer) Lock-Free Queue**, 그리고 OS 스케줄링 오버헤드를 줄이는 **Thread Affinity** 기술을 결합하여 밀리초(ms) 이하의 결정론적 지연 시간(Deterministic Latency)을 보장합니다.

상위 레이어(C#/C++)에서는 P/Invoke 및 C-Style API를 통해 복잡한 동기화 과정 없이 다중 채널을 안전하고 쉽게 관리할 수 있습니다.

### Directory Map
~~~text
CommStack/
├── Bin/                      # 빌드된 CommStack.dll, .lib 및 실행 파일 출력 디렉토리
├── CommStack/                # C++ 코어 네이티브 프로젝트 구현부 (Src 등)
├── CommStackWrapper/         # 상위 레이어 연동을 위한 통신 래퍼 프로젝트
├── Include/                  # 외부 참조용 C++ 헤더 및 인터페이스
│   ├── CommApi.hpp           # C-Style Export API
│   ├── Common.hpp            # 공통 구조체 및 열거형 정의
│   ├── ICommunicationChannel.hpp 
│   ├── PacketMemoryPool.hpp  # Zero-Allocation 메모리 풀
│   ├── SpscQueue.hpp         # SPSC Lock-free 큐
│   ├── TcpChannel.hpp        # TCP 스트림 채널
│   └── UdpChannel.hpp        # UDP 데이터그램 채널
├── Tests/                    # 통합 테스트 및 클라이언트 프로젝트 모음
│   ├── Benchmark_CSharp/     # C# P/Invoke 통신 성능 벤치마크
│   ├── BenchMarkTest/        # C++ 네이티브 통신 성능 벤치마크
│   ├── CommStack.Client/     # C# 다중 채널 관리자 (ChannelManager) 통합 테스트
│   └── Cpp.Client/           # C++ 다중 채널 관리자 (ChannelManager) 통합 테스트
├── .gitignore                # Git 버전 관리 제외 목록
├── CommStack.sln             # Visual Studio 통합 솔루션 파일
└── README.md                 # 프로젝트 개요 및 가이드 문서
~~~

## 2. System Architecture
C/C++ 네이티브 통신 모듈과 Managed(C#) / Unmanaged(C++) 클라이언트 간의 데이터 라우팅 및 제어 흐름입니다.

~~~mermaid
sequenceDiagram
    participant App as Simulator Client (C#/C++)
    participant CM as ChannelManager
    participant API as CommApi (DLL Boundary)
    participant DLL as CommStack.dll
    participant HW as OS Network Stack

    App->>CM: AddChannel(ID: 101, UDP, Core 4)
    CM->>API: CreateCommunicationChannel()
    API->>DLL: Open() & Allocate Memory Pool
    DLL->>HW: Bind Socket & SetThreadAffinityMask
    DLL-->>App: Ready

    Note over App, HW: 고속 논블로킹 수신 루프 (Lock-free)
    
    HW->>DLL: recv/recvfrom (Packet Array)
    DLL->>DLL: Acquire Slot & Push to SPSC Queue
    App->>CM: UpdateReceive()
    CM->>API: ReceivePacketData()
    API->>DLL: Pop from SPSC Queue & Release Slot
    API-->>App: Invoke Callback (ID, byte[], size)
~~~

## 3. API Spec (Exported C-API)
DLL 경계면에서 제공되는 외부 노출 API 목록입니다. 상위 클라이언트는 클래스 내부 구현에 의존하지 않고 아래의 함수만으로 프레임워크를 제어합니다.

| API Name | Return Type | Parameter 1 | Parameter 2 | Parameter 3 | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `CreateCommunicationChannel`| `void*` | `int channelType` | - | - | 지정된 프로토콜(1:UDP, 2:TCP)의 채널 생성 |
| `OpenCommunicationChannel` | `bool` | `void* pChannelHandle`| `const ChannelConfig* pConfig` | - | 소켓 바인딩 및 백그라운드 수신 스레드 실행 |
| `SendPacketData` | `bool` | `void* pChannelHandle`| `const uint8_t* pData` | `uint32_t dataSizeByte` | 목적지로 패킷 송신 |
| `ReceivePacketData` | `bool` | `void* pChannelHandle`| `uint8_t* pOutBuffer` | `uint32_t* pOutSizeByte`| 락프리 큐에서 수신 패킷 인출 (Non-blocking) |
| `DestroyCommunicationChannel`| `void` | `void* pChannelHandle`| - | - | 스레드 안전 종료 및 메모리 풀 해제 |

## 4. Changelog

### v1.1.0 — 안정성 및 정확성 개선

코드 리뷰를 통해 발견된 Critical 이슈 3건 및 추가 이슈 5건을 수정하였습니다.

**Critical 수정**

- **[Thread Safety]** `TcpChannel`, `UdpChannel`의 `m_bIsRunning` 멤버를 `bool`에서 `std::atomic<bool>`로 변경.  
  메인 스레드의 `Close()`와 워커 스레드의 `RunReceiveLoop()` 간 데이터 레이스(Undefined Behavior)를 제거합니다.

- **[Buffer Overflow]** `UdpChannel::RunReceiveLoop()`의 `recvfrom` 수신 크기를 `m_config.m_nBufferSize` 대신 슬롯 실제 크기인 `SLOT_SIZE_BYTE(2048)`로 고정.  
  `BufferSize` 설정값이 2048보다 클 경우(기본 예시: 65536) 인접 슬롯 메모리를 덮어쓰는 버퍼 오버플로우를 방지합니다.

- **[Memory Pool]** `PacketMemoryPool` 생성자에서 `SpscQueue` 용량을 `slotCount + 1`로 수정.  
  SPSC 큐는 풀/엠프티 구분을 위해 슬롯 1개를 희생하므로, 기존 코드에서는 마지막 슬롯 인덱스가 영구적으로 유실되는 버그가 있었습니다.

**추가 수정**

- **[Correctness]** `TcpChannel::SendPacket()` — `send()` Partial Send 대응 루프 추가. TCP는 단일 호출로 전체 데이터가 전송되지 않을 수 있으므로, 전체 송신이 완료될 때까지 반복합니다.
- **[Correctness]** `TcpChannel::Open()`, `UdpChannel::Open()` — 소켓 생성/바인딩 실패 시 메모리 풀과 수신 큐를 즉시 해제하여 `Open()` 재호출 시 메모리 누수를 방지합니다.
- **[Maintainability]** `UdpChannel::CloseChannel()` — 호출되지 않는 데드코드 제거. 기존 `Close()`와 중복되며, 소켓 종료 순서와 타임아웃 처리가 불일치했습니다.
- **[Maintainability]** `SpscQueue` — 복사/이동 생성자 및 대입 연산자를 `= delete`로 명시. SPSC 큐는 단일 소유자를 전제하므로 의미론적으로 복사 불가입니다.
- **[Style]** `Common.hpp` — 헤더 가드를 `#ifndef` 방식에서 `#pragma once`로 통일. 나머지 헤더 파일과 일관성을 맞춥니다.

---

## 5. Configuration Config (JSON Example)
시뮬레이터 구동 시 `ChannelManager`에 주입하여 통신망을 초기화하기 위한 JSON 설정 구조 예시입니다. 다중 노드 및 병렬 코어 할당을 명시합니다.

~~~json
{
  "CommunicationSystem": {
    "Channels": [
      {
        "ChannelId": 101,
        "ProtocolType": "UDP",
        "TargetIp": "127.0.0.1",
        "TargetPort": 8001,
        "LocalPort": 8001,
        "BufferSize": 65536,
        "CpuCoreIndex": 4,
        "Description": "Flight Dynamics Data Stream"
      },
      {
        "ChannelId": 201,
        "ProtocolType": "TCP",
        "TargetIp": "127.0.0.1",
        "TargetPort": 9001,
        "LocalPort": 9001,
        "BufferSize": 65536,
        "CpuCoreIndex": 5,
        "Description": "Weapon System Control Command"
      }
    ]
  }
}
~~~