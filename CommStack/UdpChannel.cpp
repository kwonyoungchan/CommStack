/**
 * @file UdpChannel.cpp
 * @brief UDP 채널 통신 제어 및 고성능 수신 루프 구현 (생명주기 및 API 포함)
 */

#include "pch.h"    
#include "../Include/UdpChannel.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

 /**
  * @brief UdpChannel 생성자
  * @details 멤버 변수의 초기화를 수행합니다. (자원 할당은 Open에서 지연 수행)
  */
UdpChannel::UdpChannel()
    : m_socket(INVALID_SOCKET),
    m_hWorkerThread(nullptr),
    m_bIsRunning(false),
    m_pMemoryPool(nullptr),
    m_pReceiveQueue(nullptr)
{
    // C++11 이후 구조체/배열 초기화는 0으로 안전하게 세팅
    std::memset(&m_config, 0, sizeof(m_config));
}

/**
 * @brief UdpChannel 소멸자
 * @details 스레드 및 소켓을 안전하게 종료하고 동적 할당된 메모리를 해제합니다.
 */
UdpChannel::~UdpChannel()
{
    Close();

    // 동적 할당된 락프리 큐와 메모리 풀 해제
    delete m_pReceiveQueue;
    m_pReceiveQueue = nullptr;

    delete m_pMemoryPool;
    m_pMemoryPool = nullptr;
}

/**
 * @brief 채널 초기화 및 스레드 시작
 * @param config 채널 설정 정보 (포트, IP, 코어 할당 등)
 * @return bool 오픈 성공 여부
 */
bool UdpChannel::Open(const ChannelConfig& config)
{
    m_config = config;

    // 1. 메모리 풀 및 락프리 큐 할당 (크기는 2의 거듭제곱을 따름)
    // 단일 패킷의 최대 크기를 2048 Byte로 가정하여 풀을 생성합니다.
    m_pMemoryPool = new PacketMemoryPool(m_config.m_nBufferSize, 2048);
    m_pReceiveQueue = new SpscQueue<PacketInfo>(m_config.m_nBufferSize);

    // 2. UDP 소켓 생성 및 바인딩
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        return false;
    }
    // OS 커널의 UDP 수신 버퍼를 8MB로 확장하여 유실(Drop) 방지
    int nReceiveBufferSize = 8 * 1024 * 1024; // 8MB
    setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&nReceiveBufferSize), sizeof(nReceiveBufferSize));

    sockaddr_in localAddress = {};
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = INADDR_ANY;
    localAddress.sin_port = htons(m_config.m_nLocalPort);

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&localAddress), sizeof(localAddress)) == SOCKET_ERROR) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    // 3. 수신 루프를 구동할 백그라운드 스레드 시작
    m_bIsRunning = true;
    m_hWorkerThread = reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr, 0, &UdpChannel::StartThreadRoutine, this, 0, nullptr)
        );

    return m_hWorkerThread != nullptr;
}

/**
 * @brief 채널 닫기 및 스레드 완전 종료
 */
void UdpChannel::Close()
{
    if (!m_bIsRunning) {
        return;
    }

    m_bIsRunning = false;

    // 소켓을 닫아 recvfrom 블로킹을 해제시킵니다.
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    // 스레드가 정상 종료될 때까지 대기
    if (m_hWorkerThread != nullptr) {
        WaitForSingleObject(m_hWorkerThread, INFINITE);
        CloseHandle(m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }
}

/**
 * @brief 데이터를 지정된 목적지로 송신
 * @param pData 송신할 데이터 포인터
 * @param nSizeByte 데이터 크기
 * @return bool 송신 성공 여부
 */
bool UdpChannel::SendPacket(const uint8_t* pData, uint32_t nSizeByte)
{
    if (m_socket == INVALID_SOCKET || pData == nullptr || nSizeByte == 0) {
        return false;
    }

    sockaddr_in targetAddress = {};
    targetAddress.sin_family = AF_INET;
    targetAddress.sin_port = htons(m_config.m_nTargetPort);
    inet_pton(AF_INET, m_config.m_szTargetIp, &targetAddress.sin_addr);

    int nSentByte = sendto(m_socket,
        reinterpret_cast<const char*>(pData),
        static_cast<int>(nSizeByte),
        0,
        reinterpret_cast<sockaddr*>(&targetAddress),
        sizeof(targetAddress));

    return nSentByte == static_cast<int>(nSizeByte);
}

/**
 * @brief 락프리 큐에서 수신된 패킷 데이터를 가져옴 (Consumer 역할)
 * @param pOutBuffer 데이터를 복사받을 외부 버퍼
 * @param nOutSizeByte 복사된 실제 데이터 크기를 반환받을 참조 변수
 * @return bool 큐에 데이터가 존재하여 복사에 성공했는지 여부
 */
bool UdpChannel::ReceivePacket(uint8_t* pOutBuffer, uint32_t& nOutSizeByte)
{
    if (pOutBuffer == nullptr || m_pReceiveQueue == nullptr) {
        return false;
    }

    PacketInfo packetInfo;

    // 1. 락프리 큐에서 데이터 인덱스 추출 시도
    if (!m_pReceiveQueue->PopData(packetInfo)) {
        nOutSizeByte = 0;
        return false; // 큐가 비어있음
    }

    // 2. 인덱스를 이용해 메모리 풀에서 실제 버퍼 주소를 얻고 데이터 복사
    uint8_t* pSourceBuffer = m_pMemoryPool->GetBufferPointer(packetInfo.m_nSlotIndex);
    std::memcpy(pOutBuffer, pSourceBuffer, packetInfo.m_nDataSizeByte);
    nOutSizeByte = packetInfo.m_nDataSizeByte;

    // 3. 중요: 데이터 복사가 끝난 슬롯을 메모리 풀에 다시 반환
    m_pMemoryPool->ReleaseSlot(packetInfo.m_nSlotIndex);

    return true;
}
 /**
  * @brief 스레드 진입점 (정적 메서드)
  * @param pContext UdpChannel 인스턴스의 포인터
  * @return unsigned 스레드 종료 코드
  */
unsigned __stdcall UdpChannel::StartThreadRoutine(void* pContext) {
    UdpChannel* pChannel = static_cast<UdpChannel*>(pContext);
    if (pChannel != nullptr) {
        pChannel->RunReceiveLoop();
    }
    return 0;
}

/**
 * @brief 실제 UDP 패킷 수신 및 락프리 큐 전달 루프
 * @details 스레드 어피니티를 설정하고, 메모리 풀에서 슬롯을 빌려 데이터를 수신한 뒤 인덱스를 큐에 넘깁니다.
 */
void UdpChannel::RunReceiveLoop() {
    // 1. 하드웨어 제어: 스레드 어피니티 설정 (Cache Hit 율 극대화)
    // 지정된 CPU 코어(m_config.m_nCpuCoreIndex)에 이 스레드를 고정합니다.
    DWORD_PTR nAffinityMask = (DWORD_PTR)1 << m_config.m_nCpuCoreIndex;
    SetThreadAffinityMask(GetCurrentThread(), nAffinityMask);

    sockaddr_in remoteAddress;
    int nAddressLength = sizeof(remoteAddress);

    // 타임아웃 설정을 통해 스레드가 블로킹 상태에 영원히 빠지는 것을 방지합니다.
    DWORD nTimeoutMs = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeoutMs, sizeof(nTimeoutMs));

    while (m_bIsRunning) {
        size_t nSlotIndex = 0;

        // 2. 메모리 최적화: 풀에서 빈 패킷 슬롯 인덱스 획득 (락프리)
        if (!m_pMemoryPool->AcquireSlot(nSlotIndex)) {
            // 풀이 가득 찼다면 (메인 스레드가 처리를 못 따라감) CPU 사이클을 잠시 양보합니다.
            Sleep(0);
            continue;
        }

        // 실제 메모리 주소를 가져옵니다.
        uint8_t* pReceiveBuffer = m_pMemoryPool->GetBufferPointer(nSlotIndex);

        // 3. 데이터 수신 (시스템 콜)
        int nReceivedByte = recvfrom(m_socket,
            reinterpret_cast<char*>(pReceiveBuffer),
            m_config.m_nBufferSize,
            0,
            reinterpret_cast<sockaddr*>(&remoteAddress),
            &nAddressLength);

        if (nReceivedByte > 0) {
            // 4. 수신 성공: 데이터 크기와 인덱스를 구조체로 묶어 큐에 삽입 (락프리)
            PacketInfo info;
            info.m_nSlotIndex = nSlotIndex;
            info.m_nDataSizeByte = static_cast<uint32_t>(nReceivedByte);

            if (!m_pReceiveQueue->PushData(info)) {
                // 큐가 가득 찬 극단적인 상황: 슬롯을 다시 풀에 반환하여 메모리 누수를 막습니다.
                m_pMemoryPool->ReleaseSlot(nSlotIndex);
            }
        }
        else {
            // 수신 실패 또는 타임아웃: 빌려온 슬롯을 즉시 풀에 반환합니다.
            m_pMemoryPool->ReleaseSlot(nSlotIndex);
        }
    }
}

/**
 * @brief 채널 닫기 및 수신 스레드 안전 종료
 */
void UdpChannel::CloseChannel() {
    m_bIsRunning = false; // 루프 종료 플래그 설정

    if (m_hWorkerThread != nullptr) {
        // 스레드가 정상 종료될 때까지 대기합니다 (최대 1000ms)
        WaitForSingleObject(m_hWorkerThread, 1000);
        CloseHandle(m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}