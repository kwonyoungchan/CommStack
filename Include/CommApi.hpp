/**
 * @file CommApi.hpp
 * @brief CommStack.dll의 외부 노출 C-Style API 선언부
 * @details C# 및 다른 언어(모듈)에서 내부의 C++ 통신 채널 객체를 제어하기 위한 인터페이스입니다.
 */

#pragma once

#include <cstdint>
#include "ICommunicationChannel.hpp"

 // DLL Export/Import 매크로 정의
#ifdef COMMSTACK_EXPORTS
#define COMMSTACK_API __declspec(dllexport)
#else
#define COMMSTACK_API __declspec(dllimport)
#endif

extern "C" {

    /**
     * @brief 통신 채널 인스턴스를 동적으로 생성합니다.
     * @param channelType 생성할 채널의 종류 (1: UDP, 2: TCP, 3: Serial)
     * @return void* 생성된 채널 객체의 포인터 (실패 시 nullptr)
     */
    COMMSTACK_API void* CreateCommunicationChannel(int channelType);

    /**
     * @brief 생성된 통신 채널을 설정하고 연결을 엽니다.
     * @param pChannelHandle CreateCommunicationChannel을 통해 반환받은 채널 포인터
     * @param pConfig 채널 설정 정보를 담은 구조체 포인터
     * @return bool 연결 성공 여부
     */
    COMMSTACK_API bool OpenCommunicationChannel(void* pChannelHandle,  ChannelConfig* pConfig);

    /**
     * @brief 통신 채널을 종료하고 할당된 메모리를 해제합니다.
     * @param pChannelHandle 해제할 채널의 포인터
     */
    COMMSTACK_API void DestroyCommunicationChannel(void* pChannelHandle);

    /**
     * @brief 연결된 통신 채널을 통해 패킷 데이터를 송신합니다.
     * @param pChannelHandle 채널 포인터
     * @param pData 송신할 데이터 버퍼의 포인터
     * @param dataSizeByte 송신할 데이터의 크기 (Byte 단위)
     * @return bool 송신 큐 삽입 성공 여부
     */
    COMMSTACK_API bool SendPacketData(void* pChannelHandle, const uint8_t* pData, uint32_t dataSizeByte);

    /**
     * @brief 수신 큐에 대기 중인 패킷 데이터를 가져옵니다.
     * @param pChannelHandle 채널 포인터
     * @param pOutBuffer 수신된 데이터를 복사할 대상 버퍼 포인터
     * @param pOutSizeByte 수신된 데이터의 실제 크기를 반환받을 포인터 (Byte 단위)
     * @return bool 수신 데이터 존재 및 복사 성공 여부
     */
    COMMSTACK_API bool ReceivePacketData(void* pChannelHandle, uint8_t* pOutBuffer, uint32_t* pOutSizeByte);

}