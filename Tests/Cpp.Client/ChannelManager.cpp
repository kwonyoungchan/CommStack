/**
 * @file ChannelManager.cpp
 * @brief ChannelManager 클래스의 구현부
 */

#include "ChannelManager.hpp"
#include "../Include/CommApi.hpp"
#include <cstring>
#include <iostream>

ChannelManager::ChannelManager() {
    // 가장 큰 패킷 크기를 상정하여 공용 버퍼를 1회만 할당합니다.
    m_receiveBuffer.resize(65536);
}

ChannelManager::~ChannelManager() {
    CloseAllChannels();
}

/**
 * @brief 새로운 통신 채널을 생성하고 관리 맵에 등록합니다.
 * @param creationInfo 채널 생성에 필요한 설정 값들이 담긴 구조체
 * @return bool 생성 및 등록 성공 여부
 */
bool ChannelManager::AddCommunicationChannel(const ChannelCreationInfo& creationInfo) {
    void* pChannelHandle = CreateCommunicationChannel(creationInfo.m_protocolType);
    if (pChannelHandle == nullptr) {
        return false;
    }

    ChannelConfig config = {};
    config.m_nChannelId = creationInfo.m_channelId;
    config.m_nTargetPort = creationInfo.m_targetPort;
    config.m_nLocalPort = creationInfo.m_localPort;
    config.m_nBufferSize = 65536;
    config.m_nCpuCoreIndex = creationInfo.m_cpuCoreIndex;

    // std::string을 C-style 문자열 배열로 안전하게 복사합니다.
    strncpy_s(config.m_szTargetIp, sizeof(config.m_szTargetIp), creationInfo.m_targetIp.c_str(), _TRUNCATE);
    config.m_szTargetIp[sizeof(config.m_szTargetIp) - 1] = '\0';

    if (OpenCommunicationChannel(pChannelHandle, &config)) {
        m_channelMap[creationInfo.m_channelId] = pChannelHandle;
        return true;
    }

    DestroyCommunicationChannel(pChannelHandle);
    return false;
}

/**
 * @brief 특정 채널을 통해 데이터를 송신합니다.
 * @param channelId 송신할 대상의 채널 식별자
 * @param pData 송신할 데이터의 시작 포인터
 * @param dataSizeByte 송신할 데이터의 크기 (Byte)
 * @return bool 송신 성공 여부
 */
bool ChannelManager::SendDataToChannel(uint32_t channelId, const uint8_t* pData, uint32_t dataSizeByte) {
    auto iterator = m_channelMap.find(channelId);
    if (iterator != m_channelMap.end()) {
        return SendPacketData(iterator->second, pData, dataSizeByte);
    }
    return false;
}

/**
 * @brief 모든 활성화된 채널을 순회하며 수신 큐의 데이터를 확인합니다.
 * @param pCallback 데이터를 수신했을 때 호출할 콜백 함수 객체
 */
void ChannelManager::UpdateReceiveData(ReceiveCallback pCallback) {
    for (auto& pair : m_channelMap) {
        uint32_t currentChannelId = pair.first;
        void* pChannelHandle = pair.second;

        // 단일 채널에서 한 번의 루프에 처리할 최대 패킷 수 제한 (폭주 방지)
        int burstLimitCount = 1000;
        uint32_t receivedSizeByte = 0;

        while (burstLimitCount-- > 0 &&
            ReceivePacketData(pChannelHandle, m_receiveBuffer.data(), &receivedSizeByte)) {

            if (receivedSizeByte > 0 && pCallback) {
                pCallback(currentChannelId, m_receiveBuffer.data(), receivedSizeByte);
            }
        }
    }
}

/**
 * @brief 등록된 모든 채널의 자원을 안전하게 해제합니다.
 */
void ChannelManager::CloseAllChannels() {
    for (auto& pair : m_channelMap) {
        DestroyCommunicationChannel(pair.second);
    }
    m_channelMap.clear();
}