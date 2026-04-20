/**
 * @file PacketMemoryPool.hpp
 * @brief ���� �Ҵ��� �����ϱ� ���� ���� ũ�� ��Ŷ �޸� Ǯ
 * @details ���������� SPSC ť�� ����Ͽ� �� ������ �ε����� ������ ������� �����մϴ�.
 */

#pragma once
#include <cstdint>
#include "SpscQueue.hpp"

class PacketMemoryPool {
public:
    /**
     * @brief �޸� Ǯ �ʱ�ȭ �� �ϰ� �Ҵ�
     * @param slotCount �� ���� ���� (2�� �ŵ����� ����)
     * @param slotSizeByte ���� ������ ũ�� (Byte ����)
     */
    PacketMemoryPool(size_t slotCount, size_t slotSizeByte)
        : m_slotSizeByte(slotSizeByte),
        m_freeSlotQueue(slotCount + 1)  // [Fix #3] SpscQueue�� ���ó���(capacity-1)���ε�
                                        // slotCount���� ��ü ������ ȹ���Ϸ��� +1 ������
    {
        // ��ü ���۸� �� ���� �Ҵ��Ͽ� ������ �޸� ���Ӽ� Ȯ��
        m_pBaseBuffer = new uint8_t[slotCount * slotSizeByte];

        // �ʱ� ����: ��� ���� �ε����� ��� ������ ���·� ť�� ����
        for (size_t i = 0; i < slotCount; ++i) {
            m_freeSlotQueue.PushData(i);
        }
    }

    ~PacketMemoryPool() {
        delete[] m_pBaseBuffer;
    }

    /**
     * @brief ��� ������ �� ������ �ε��� ȹ��
     * @param outSlotIndex ȹ���� ���� �ε����� ������ ����
     * @return true ȹ�� ����, false ��� ������ ���� ���� ����
     */
    bool AcquireSlot(size_t& outSlotIndex) {
        return m_freeSlotQueue.PopData(outSlotIndex);
    }

    /**
     * @brief ����� �Ϸ�� ������ Ǯ�� ��ȯ
     * @param slotIndex ��ȯ�� ���� �ε���
     * @return true ��ȯ ����, false ť ���۵� ������
     */
    bool ReleaseSlot(size_t slotIndex) {
        return m_freeSlotQueue.PushData(slotIndex);
    }

    /**
     * @brief �ε����� ���εǴ� ���� �޸� ���� ������ ȹ��
     * @param slotIndex ���� �ε���
     * @return uint8_t* ���� �޸� �ּ�
     */
    uint8_t* GetBufferPointer(size_t slotIndex) const {
        return m_pBaseBuffer + (slotIndex * m_slotSizeByte);
    }

private:
    size_t m_slotSizeByte;
    uint8_t* m_pBaseBuffer;

    // �� ������ �ε����� �����ϴ� ������ ť
    SpscQueue<size_t> m_freeSlotQueue;
};