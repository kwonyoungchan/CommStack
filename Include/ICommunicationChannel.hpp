/**
 * @file ICommunicationChannel.hpp
 * @brief 통신 채널 추상화 인터페이스
 */
#pragma once
#include <cstdint>
#include <vector>

 /**
  * @struct ChannelConfig
  * @brief 채널 설정을 위한 구조체
  */
struct ChannelConfig {
    uint32_t m_nChannelId;
    char m_szTargetIp[16];
    uint16_t m_nTargetPort;
    uint16_t m_nLocalPort;
    uint32_t m_nBufferSize;    ///< 큐 크기 (2의 거듭제곱 권장)
    uint32_t m_nCpuCoreIndex;  ///< Thread Affinity용 코어 인덱스
};

class ICommunicationChannel {
public:
    virtual ~ICommunicationChannel() = default;
    virtual bool Open(const ChannelConfig& config) = 0;
    virtual void Close() = 0;
    virtual bool SendPacket(const uint8_t* pData, uint32_t nSize) = 0;
    virtual bool ReceivePacket(uint8_t* pOutBuffer, uint32_t& nOutSize) = 0;
};