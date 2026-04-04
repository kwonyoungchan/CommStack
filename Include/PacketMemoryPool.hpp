/**
 * @file PacketMemoryPool.hpp
 * @brief 동적 할당을 방지하기 위한 고정 크기 패킷 메모리 풀
 * @details 내부적으로 SPSC 큐를 사용하여 빈 슬롯의 인덱스를 락프리 방식으로 관리합니다.
 */

#pragma once
#include <cstdint>
#include "SpscQueue.hpp"

class PacketMemoryPool {
public:
    /**
     * @brief 메모리 풀 초기화 및 일괄 할당
     * @param slotCount 총 슬롯 개수 (2의 거듭제곱 권장)
     * @param slotSizeByte 단일 슬롯의 크기 (Byte 단위)
     */
    PacketMemoryPool(size_t slotCount, size_t slotSizeByte)
        : m_slotSizeByte(slotSizeByte),
        m_freeSlotQueue(slotCount)
    {
        // 전체 버퍼를 한 번에 할당하여 물리적 메모리 연속성 확보
        m_pBaseBuffer = new uint8_t[slotCount * slotSizeByte];

        // 초기 상태: 모든 슬롯 인덱스를 사용 가능한 상태로 큐에 삽입
        for (size_t i = 0; i < slotCount; ++i) {
            m_freeSlotQueue.PushData(i);
        }
    }

    ~PacketMemoryPool() {
        delete[] m_pBaseBuffer;
    }

    /**
     * @brief 사용 가능한 빈 슬롯의 인덱스 획득
     * @param outSlotIndex 획득한 슬롯 인덱스를 저장할 변수
     * @return true 획득 성공, false 사용 가능한 남은 슬롯 없음
     */
    bool AcquireSlot(size_t& outSlotIndex) {
        return m_freeSlotQueue.PopData(outSlotIndex);
    }

    /**
     * @brief 사용이 완료된 슬롯을 풀에 반환
     * @param slotIndex 반환할 슬롯 인덱스
     * @return true 반환 성공, false 큐 오작동 방지용
     */
    bool ReleaseSlot(size_t slotIndex) {
        return m_freeSlotQueue.PushData(slotIndex);
    }

    /**
     * @brief 인덱스에 매핑되는 실제 메모리 버퍼 포인터 획득
     * @param slotIndex 버퍼 인덱스
     * @return uint8_t* 실제 메모리 주소
     */
    uint8_t* GetBufferPointer(size_t slotIndex) const {
        return m_pBaseBuffer + (slotIndex * m_slotSizeByte);
    }

private:
    size_t m_slotSizeByte;
    uint8_t* m_pBaseBuffer;

    // 빈 슬롯의 인덱스를 보관하는 락프리 큐
    SpscQueue<size_t> m_freeSlotQueue;
};