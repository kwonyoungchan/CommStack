/**
 * @file TcpChannel.cpp
 * @brief TCP ä�� ��� ���� �� ������ ���� ���� ����
 */
#include "pch.h"
#include "../Include/TcpChannel.hpp"
#include <ws2tcpip.h>
#include <process.h>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

TcpChannel::TcpChannel()
    : m_socket(INVALID_SOCKET),
    m_hWorkerThread(nullptr),
    m_bIsRunning(false),
    m_pMemoryPool(nullptr),
    m_pReceiveQueue(nullptr)
{
    std::memset(&m_config, 0, sizeof(m_config));
}

TcpChannel::~TcpChannel()
{
    Close();

    delete m_pReceiveQueue;
    m_pReceiveQueue = nullptr;

    delete m_pMemoryPool;
    m_pMemoryPool = nullptr;
}

bool TcpChannel::Open(const ChannelConfig& config)
{
    m_config = config;

    m_pMemoryPool = new PacketMemoryPool(m_config.m_nBufferSize, 2048);
    m_pReceiveQueue = new SpscQueue<PacketInfo>(m_config.m_nBufferSize);

    // TCP ���� ����
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        // [Fix #6] ���� ���� ½Ã ���콺 ó��
        delete m_pMemoryPool;   m_pMemoryPool = nullptr;
        delete m_pReceiveQueue; m_pReceiveQueue = nullptr;
        return false;
    }

    // ���� ������ �ּ� ����
    sockaddr_in targetAddress = {};
    targetAddress.sin_family = AF_INET;
    targetAddress.sin_port = htons(m_config.m_nTargetPort);
    inet_pton(AF_INET, m_config.m_szTargetIp, &targetAddress.sin_addr);

    // TCP Ŭ���̾�Ʈ�μ� ������ ���� (Connect)
    if (connect(m_socket, reinterpret_cast<sockaddr*>(&targetAddress), sizeof(targetAddress)) == SOCKET_ERROR) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        // [Fix #6] ���� ���� ½Ã ���콺 ó��
        delete m_pMemoryPool;   m_pMemoryPool = nullptr;
        delete m_pReceiveQueue; m_pReceiveQueue = nullptr;
        return false;
    }

    m_bIsRunning = true;
    m_hWorkerThread = reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr, 0, &TcpChannel::StartThreadRoutine, this, 0, nullptr)
        );

    return m_hWorkerThread != nullptr;
}

void TcpChannel::Close()
{
    if (!m_bIsRunning) {
        return;
    }

    m_bIsRunning = false;

    if (m_socket != INVALID_SOCKET) {
        // TCP�� ����� ����(Graceful Shutdown) ����
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_hWorkerThread != nullptr) {
        WaitForSingleObject(m_hWorkerThread, INFINITE);
        CloseHandle(m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }
}

bool TcpChannel::SendPacket(const uint8_t* pData, uint32_t nSizeByte)
{
    if (m_socket == INVALID_SOCKET || pData == nullptr || nSizeByte == 0) {
        return false;
    }

    // [Fix #7] TCP send()�� partial send�� �����Ƿ� ��ü ������ ���ŵ� ������ ���ε�
    uint32_t nTotalSent = 0;
    while (nTotalSent < nSizeByte) {
        int nSentByte = send(m_socket,
            reinterpret_cast<const char*>(pData + nTotalSent),
            static_cast<int>(nSizeByte - nTotalSent),
            0);

        if (nSentByte == SOCKET_ERROR) {
            return false;
        }
        nTotalSent += static_cast<uint32_t>(nSentByte);
    }
    return true;
}

bool TcpChannel::ReceivePacket(uint8_t* pOutBuffer, uint32_t& nOutSizeByte)
{
    if (pOutBuffer == nullptr || m_pReceiveQueue == nullptr) {
        return false;
    }

    PacketInfo packetInfo;

    if (!m_pReceiveQueue->PopData(packetInfo)) {
        nOutSizeByte = 0;
        return false;
    }

    uint8_t* pSourceBuffer = m_pMemoryPool->GetBufferPointer(packetInfo.m_nSlotIndex);
    std::memcpy(pOutBuffer, pSourceBuffer, packetInfo.m_nDataSizeByte);
    nOutSizeByte = packetInfo.m_nDataSizeByte;

    m_pMemoryPool->ReleaseSlot(packetInfo.m_nSlotIndex);

    return true;
}

unsigned __stdcall TcpChannel::StartThreadRoutine(void* pContext)
{
    TcpChannel* pChannel = static_cast<TcpChannel*>(pContext);
    if (pChannel != nullptr) {
        pChannel->RunReceiveLoop();
    }
    return 0;
}

void TcpChannel::RunReceiveLoop()
{
    DWORD_PTR nAffinityMask = (DWORD_PTR)1 << m_config.m_nCpuCoreIndex;
    SetThreadAffinityMask(GetCurrentThread(), nAffinityMask);

    // ����ŷ ������ ���� ���� Ÿ�Ӿƿ�
    DWORD nTimeoutMs = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&nTimeoutMs), sizeof(nTimeoutMs));

    while (m_bIsRunning) {
        size_t nSlotIndex = 0;

        if (!m_pMemoryPool->AcquireSlot(nSlotIndex)) {
            Sleep(0);
            continue;
        }

        uint8_t* pReceiveBuffer = m_pMemoryPool->GetBufferPointer(nSlotIndex);

        // TCP ���� ���
        int nReceivedByte = recv(m_socket, reinterpret_cast<char*>(pReceiveBuffer), 2048, 0);

        if (nReceivedByte > 0) {
            PacketInfo info;
            info.m_nSlotIndex = nSlotIndex;
            info.m_nDataSizeByte = static_cast<uint32_t>(nReceivedByte);

            if (!m_pReceiveQueue->PushData(info)) {
                m_pMemoryPool->ReleaseSlot(nSlotIndex);
            }
        }
        else if (nReceivedByte == 0) {
            // TCP ������ ���������� ����� (Graceful Close)
            m_pMemoryPool->ReleaseSlot(nSlotIndex);
            break;
        }
        else {
            m_pMemoryPool->ReleaseSlot(nSlotIndex);
        }
    }
}