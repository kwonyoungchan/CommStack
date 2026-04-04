#ifndef PACKET_INFO_DEFINED
#define PACKET_INFO_DEFINED

#include <cstdint>

/**
 * @struct PacketInfo
 * @brief 수신된 패킷의 메타 데이터를 큐에 전달하기 위한 구조체
 */
struct PacketInfo {
    size_t m_nSlotIndex;         ///< 메모리 풀에서 할당받은 버퍼의 인덱스
    uint32_t m_nDataSizeByte;    ///< 실제 수신된 데이터의 크기 (Byte)
};
#endif