/**
 * @file UdpChannel.cpp
 * @brief UDP ä�� ��� ���� �� ������ ���� ���� ���� (�����ֱ� �� API ����)
 */

#include "pch.h"    
#include "../Include/UdpChannel.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

 /**
  * @brief UdpChannel ������
  * @details ��� ������ �ʱ�ȭ�� �����մϴ�. (�ڿ� �Ҵ��� Open���� ���� ����)
  */
UdpChannel::UdpChannel()
    : m_socket(INVALID_SOCKET),
    m_hWorkerThread(nullptr),
    m_bIsRunning(false),
    m_pMemoryPool(nullptr),
    m_pReceiveQueue(nullptr)
{
    // C++11 ���� ����ü/�迭 �ʱ�ȭ�� 0���� �����ϰ� ����
    std::memset(&m_config, 0, sizeof(m_config));
}

/**
 * @brief UdpChannel �Ҹ���
 * @details ������ �� ������ �����ϰ� �����ϰ� ���� �Ҵ�� �޸𸮸� �����մϴ�.
 */
UdpChannel::~UdpChannel()
{
    Close();

    // ���� �Ҵ�� ������ ť�� �޸� Ǯ ����
    delete m_pReceiveQueue;
    m_pReceiveQueue = nullptr;

    delete m_pMemoryPool;
    m_pMemoryPool = nullptr;
}

/**
 * @brief ä�� �ʱ�ȭ �� ������ ����
 * @param config ä�� ���� ���� (��Ʈ, IP, �ھ� �Ҵ� ��)
 * @return bool ���� ���� ����
 */
bool UdpChannel::Open(const ChannelConfig& config)
{
    m_config = config;

    // 1. �޸� Ǯ �� ������ ť �Ҵ� (ũ��� 2�� �ŵ������� ����)
    // ���� ��Ŷ�� �ִ� ũ�⸦ 2048 Byte�� �����Ͽ� Ǯ�� �����մϴ�.
    m_pMemoryPool = new PacketMemoryPool(m_config.m_nBufferSize, 2048);
    m_pReceiveQueue = new SpscQueue<PacketInfo>(m_config.m_nBufferSize);

    // 2. UDP ���� ���� �� ���ε�
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        // [Fix #6] ���� ���� ½Ã ���콺 ó��
        delete m_pMemoryPool;   m_pMemoryPool = nullptr;
        delete m_pReceiveQueue; m_pReceiveQueue = nullptr;
        return false;
    }
    // OS Ŀ���� UDP ���� ���۸� 8MB�� Ȯ���Ͽ� ����(Drop) ����
    int nReceiveBufferSize = 8 * 1024 * 1024; // 8MB
    setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&nReceiveBufferSize), sizeof(nReceiveBufferSize));

    sockaddr_in localAddress = {};
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = INADDR_ANY;
    localAddress.sin_port = htons(m_config.m_nLocalPort);

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&localAddress), sizeof(localAddress)) == SOCKET_ERROR) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        // [Fix #6] ���ε� ���� ½Ã ���콺 ó��
        delete m_pMemoryPool;   m_pMemoryPool = nullptr;
        delete m_pReceiveQueue; m_pReceiveQueue = nullptr;
        return false;
    }

    // 3. ���� ������ ������ ��׶��� ������ ����
    m_bIsRunning = true;
    m_hWorkerThread = reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr, 0, &UdpChannel::StartThreadRoutine, this, 0, nullptr)
        );

    return m_hWorkerThread != nullptr;
}

/**
 * @brief ä�� �ݱ� �� ������ ���� ����
 */
void UdpChannel::Close()
{
    if (!m_bIsRunning) {
        return;
    }

    m_bIsRunning = false;

    // ������ �ݾ� recvfrom ����ŷ�� ������ŵ�ϴ�.
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    // �����尡 ���� ����� ������ ���
    if (m_hWorkerThread != nullptr) {
        WaitForSingleObject(m_hWorkerThread, INFINITE);
        CloseHandle(m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }
}

/**
 * @brief �����͸� ������ �������� �۽�
 * @param pData �۽��� ������ ������
 * @param nSizeByte ������ ũ��
 * @return bool �۽� ���� ����
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
 * @brief ������ ť���� ���ŵ� ��Ŷ �����͸� ������ (Consumer ����)
 * @param pOutBuffer �����͸� ������� �ܺ� ����
 * @param nOutSizeByte ����� ���� ������ ũ�⸦ ��ȯ���� ���� ����
 * @return bool ť�� �����Ͱ� �����Ͽ� ���翡 �����ߴ��� ����
 */
bool UdpChannel::ReceivePacket(uint8_t* pOutBuffer, uint32_t& nOutSizeByte)
{
    if (pOutBuffer == nullptr || m_pReceiveQueue == nullptr) {
        return false;
    }

    PacketInfo packetInfo;

    // 1. ������ ť���� ������ �ε��� ���� �õ�
    if (!m_pReceiveQueue->PopData(packetInfo)) {
        nOutSizeByte = 0;
        return false; // ť�� �������
    }

    // 2. �ε����� �̿��� �޸� Ǯ���� ���� ���� �ּҸ� ��� ������ ����
    uint8_t* pSourceBuffer = m_pMemoryPool->GetBufferPointer(packetInfo.m_nSlotIndex);
    std::memcpy(pOutBuffer, pSourceBuffer, packetInfo.m_nDataSizeByte);
    nOutSizeByte = packetInfo.m_nDataSizeByte;

    // 3. �߿�: ������ ���簡 ���� ������ �޸� Ǯ�� �ٽ� ��ȯ
    m_pMemoryPool->ReleaseSlot(packetInfo.m_nSlotIndex);

    return true;
}
 /**
  * @brief ������ ������ (���� �޼���)
  * @param pContext UdpChannel �ν��Ͻ��� ������
  * @return unsigned ������ ���� �ڵ�
  */
unsigned __stdcall UdpChannel::StartThreadRoutine(void* pContext) {
    UdpChannel* pChannel = static_cast<UdpChannel*>(pContext);
    if (pChannel != nullptr) {
        pChannel->RunReceiveLoop();
    }
    return 0;
}

/**
 * @brief ���� UDP ��Ŷ ���� �� ������ ť ���� ����
 * @details ������ ���Ǵ�Ƽ�� �����ϰ�, �޸� Ǯ���� ������ ���� �����͸� ������ �� �ε����� ť�� �ѱ�ϴ�.
 */
void UdpChannel::RunReceiveLoop() {
    // 1. �ϵ���� ����: ������ ���Ǵ�Ƽ ���� (Cache Hit �� �ش�ȭ)
    // ������ CPU �ھ�(m_config.m_nCpuCoreIndex)�� �� �����带 �����մϴ�.
    DWORD_PTR nAffinityMask = (DWORD_PTR)1 << m_config.m_nCpuCoreIndex;
    SetThreadAffinityMask(GetCurrentThread(), nAffinityMask);

    sockaddr_in remoteAddress;
    int nAddressLength = sizeof(remoteAddress);

    // Ÿ�Ӿƿ� ������ ���� �����尡 ����ŷ ���¿� ������ ������ ���� �����մϴ�.
    DWORD nTimeoutMs = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeoutMs, sizeof(nTimeoutMs));

    // [Fix #2] ���� ������ ũ�⸦ ������ ���� ũ��(2048 Byte)�� ���������
    // m_config.m_nBufferSize�� ť ���̸� ���ϴ� ���̹Ƿ�, ���� ������ ũ������ ����.
    // 2048 Byte�� ����ŷ ������ ũ�⸦ ����ϴ�. (PacketMemoryPool ���볪 ũ�Ⱑ ����)
    static constexpr int SLOT_SIZE_BYTE = 2048;

    while (m_bIsRunning) {
        size_t nSlotIndex = 0;

        // 2. �޸� ����ȭ: Ǯ���� �� ��Ŷ ���� �ε��� ȹ�� (������)
        if (!m_pMemoryPool->AcquireSlot(nSlotIndex)) {
            // Ǯ�� ���� á�ٸ� (���� �����尡 ó���� �� ����) CPU ����Ŭ�� ��� �纸�մϴ�.
            Sleep(0);
            continue;
        }

        // ���� �޸� �ּҸ� �����ɴϴ�.
        uint8_t* pReceiveBuffer = m_pMemoryPool->GetBufferPointer(nSlotIndex);

        // 3. ������ ���� (�ý��� ��)
        // [Fix #2] SLOT_SIZE_BYTE ���뢽 m_config.m_nBufferSize(�¿ÀÇ�ÿ뷮) ����
        int nReceivedByte = recvfrom(m_socket,
            reinterpret_cast<char*>(pReceiveBuffer),
            SLOT_SIZE_BYTE,
            0,
            reinterpret_cast<sockaddr*>(&remoteAddress),
            &nAddressLength);

        if (nReceivedByte > 0) {
            // 4. ���� ����: ������ ũ��� �ε����� ����ü�� ���� ť�� ���� (������)
            PacketInfo info;
            info.m_nSlotIndex = nSlotIndex;
            info.m_nDataSizeByte = static_cast<uint32_t>(nReceivedByte);

            if (!m_pReceiveQueue->PushData(info)) {
                // ť�� ���� �� �ش����� ��Ȳ: ������ �ٽ� Ǯ�� ��ȯ�Ͽ� �޸� ������ �����ϴ�.
                m_pMemoryPool->ReleaseSlot(nSlotIndex);
            }
        }
        else {
            // ���� ���� �Ǵ� Ÿ�Ӿƿ�: ������ ������ ��� Ǯ�� ��ȯ�մϴ�.
            m_pMemoryPool->ReleaseSlot(nSlotIndex);
        }
    }
}

// [Fix #8] CloseChannel()�� ȣ���ǰ��ȿ쓴 ������ ������ Close()���� ��Ȳ�ǰ�
// ������ ó�� ���и� ���ڿ��ͼ�(INFINITE vs 1000ms) ����ŵ���.
// ���� ĸ���ϹǷ�, CloseChannel() ������ ����̴�.