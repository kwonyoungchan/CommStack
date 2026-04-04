/**
 * @file CommApi.cpp
 * @brief 외부 노출용 C-Style API 구현부
 */
#include "pch.h"
#include "../Include/CommApi.hpp"
#include "../Include/UdpChannel.hpp"
#include "../Include/TcpChannel.hpp"

#include <winsock2.h>
#include <mutex> // std::call_once 사용을 위해 추가

#pragma comment(lib, "ws2_32.lib")

 // Winsock 1회 초기화를 보장하기 위한 플래그
static std::once_flag g_winsockInitFlag;

extern "C" {

    COMMSTACK_API void* CreateCommunicationChannel(int channelType) {
        // C# 등 외부 프로세스에서 DLL 함수를 최초로 호출할 때, 안전하게 Winsock을 1회 초기화합니다.
        std::call_once(g_winsockInitFlag, []() {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
            });

        switch (channelType) {
        case 1:
            return new UdpChannel();
        case 2:
            return new TcpChannel();
        default:
            return nullptr;
        }
    }

    COMMSTACK_API bool OpenCommunicationChannel(void* pChannelHandle, ChannelConfig* pConfig) {
        ICommunicationChannel* pChannel = static_cast<ICommunicationChannel*>(pChannelHandle);
        if (pChannel != nullptr && pConfig != nullptr) {
            return pChannel->Open(*pConfig);
        }
        return false;
    }

    COMMSTACK_API void DestroyCommunicationChannel(void* pChannelHandle) {
        ICommunicationChannel* pChannel = static_cast<ICommunicationChannel*>(pChannelHandle);
        if (pChannel != nullptr) {
            delete pChannel;
        }
    }

    COMMSTACK_API bool SendPacketData(void* pChannelHandle, const uint8_t* pData, uint32_t dataSizeByte) {
        ICommunicationChannel* pChannel = static_cast<ICommunicationChannel*>(pChannelHandle);
        if (pChannel != nullptr) {
            return pChannel->SendPacket(pData, dataSizeByte);
        }
        return false;
    }

    COMMSTACK_API bool ReceivePacketData(void* pChannelHandle, uint8_t* pOutBuffer, uint32_t* pOutSizeByte) {
        ICommunicationChannel* pChannel = static_cast<ICommunicationChannel*>(pChannelHandle);
        if (pChannel != nullptr && pOutSizeByte != nullptr) {
            return pChannel->ReceivePacket(pOutBuffer, *pOutSizeByte);
        }
        return false;
    }

}