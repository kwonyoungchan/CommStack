/**
 * @file ChannelManager.hpp
 * @brief CommStack.dll을 활용하여 다수의 통신 채널을 관리하고 데이터를 라우팅하는 매니저 클래스
 */

#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>
#include <string>

 // 수신 데이터를 처리할 콜백 함수 정의 (채널 식별자, 데이터 포인터, 데이터 크기)
using ReceiveCallback = std::function<void(uint32_t, const uint8_t*, uint32_t)>;

/**
 * @brief 채널 생성을 위한 설정 정보 구조체 (매개변수 최소화 규칙 적용)
 */
struct ChannelCreationInfo {
    uint32_t m_channelId;         ///< 채널 고유 식별자
    int m_protocolType;           ///< 1: UDP, 2: TCP
    std::string m_targetIp;       ///< 목적지 IP 주소
    uint16_t m_targetPort;        ///< 목적지 포트 번호
    uint16_t m_localPort;         ///< 로컬 수신 포트 번호
    uint32_t m_cpuCoreIndex;      ///< 수신 스레드를 할당할 논리 코어 인덱스
};

class ChannelManager {
public:
    ChannelManager();
    ~ChannelManager();

    bool AddCommunicationChannel(const ChannelCreationInfo& creationInfo);
    bool SendDataToChannel(uint32_t channelId, const uint8_t* pData, uint32_t dataSizeByte);
    void UpdateReceiveData(ReceiveCallback pCallback);
    void CloseAllChannels();

private:
    std::unordered_map<uint32_t, void*> m_channelMap; ///< 활성화된 채널 핸들 보관 맵
    std::vector<uint8_t> m_receiveBuffer;             ///< 수신용 공용 버퍼 (동적 할당 방지)
};