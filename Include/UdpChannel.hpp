/**
 * @file UdpChannel.hpp
 * @brief UDP ä�� ��� ���� Ŭ���� �����
 * @details ICommunicationChannel�� ��ӹ޾� �����Ǹ�, ���������� ������ ť�� �޸� Ǯ�� �����ϴ�.
 */

#pragma once

 // Winsock2�� Windows.h���� ���� ��Ŭ���Ǿ�� ��ũ�� �浹�� �߻����� �ʽ��ϴ�.
#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <atomic>

#include "ICommunicationChannel.hpp"
#include "PacketMemoryPool.hpp"
#include "SpscQueue.hpp"
#include "Common.hpp"




class UdpChannel : public ICommunicationChannel {
public:
    UdpChannel();
    virtual ~UdpChannel();

    // ICommunicationChannel �������̽� �������̵�
    bool Open(const ChannelConfig& config) override;
    void Close() override;
    bool SendPacket(const uint8_t* pData, uint32_t nSizeByte) override;
    bool ReceivePacket(uint8_t* pOutBuffer, uint32_t& nOutSizeByte) override;

private:
    /**
     * @brief ���� ���� �������� ������ (���� �Լ�)
     * @param pContext UdpChannel ��ü�� this ������
     * @return unsigned ������ ���� �ڵ�
     */
    static unsigned __stdcall StartThreadRoutine(void* pContext);

    /**
     * @brief ���� ���� ���� �� ������ ť ������ �����ϴ� ���� ����
     */
    void RunReceiveLoop();


private:
    ChannelConfig m_config;                      ///< ä�� ���� ���� ����
    SOCKET m_socket;                             ///< UDP ��ſ� ����Ƽ�� ���� �ڵ�
    HANDLE m_hWorkerThread;                      ///< ���� ���� ��׶��� ������ �ڵ�
    std::atomic<bool> m_bIsRunning;              ///< ������ ���� ���� ���� �÷��� (atomic: Close()�� RunReceiveLoop() ���� ����)

    PacketMemoryPool* m_pMemoryPool;             ///< ���� �Ҵ� ������ ���� ��Ŷ �޸� Ǯ
    SpscQueue<PacketInfo>* m_pReceiveQueue;      ///< ���ŵ� ��Ŷ �ε����� �����ϴ� ������ ť
};