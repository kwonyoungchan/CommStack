#pragma once

#include <cstdint>

/**
 * @struct PacketInfo
 * @brief ���ŵ� ��Ŷ�� ��Ÿ �����͸� ť�� �����ϱ� ���� ����ü
 */
struct PacketInfo {
    size_t m_nSlotIndex;         ///< �޸� Ǯ���� �Ҵ���� ������ �ε���
    uint32_t m_nDataSizeByte;    ///< ���� ���ŵ� �������� ũ�� (Byte)
};