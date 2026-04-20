/**
 * @file SpscQueue.hpp
 * @brief ���� ������-���� �Һ���(SPSC) ������ ������ ť
 * @details ĳ�� ���� �浹�� �����ϸ�, �޸� �踮� ���� ������ �������� �����մϴ�.
 */

#pragma once
#include <atomic>
#include <stdexcept>

template<typename T>
class SpscQueue {
public:
    /**
     * @brief ť �ʱ�ȭ
     * @param capacity ť�� �ִ� ũ�� (�ݵ�� 2�� �ŵ������̾�� ��)
     * @exception std::invalid_argument ũ�Ⱑ 2�� �ŵ������� �ƴ� ��� �߻�
     */
    // [Fix #5] SPSC ť�� ������ ���̹Ƿ� ���/�̵� ������ ������ ����
    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;
    SpscQueue(SpscQueue&&) = delete;
    SpscQueue& operator=(SpscQueue&&) = delete;

    explicit SpscQueue(size_t capacity) {
        if ((capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("Capacity must be a power of 2.");
        }

        m_capacityMask = capacity - 1;
        m_pBuffer = new T[capacity];

        m_writeIndex.store(0, std::memory_order_relaxed);
        m_readIndex.store(0, std::memory_order_relaxed);
    }

    ~SpscQueue() {
        delete[] m_pBuffer;
    }

    /**
     * @brief �����͸� ť�� ���� (������ ����)
     * @param data ������ ������
     * @return true ���� ����, false ť�� ���� ��
     */
    bool PushData(const T& data) {
        const size_t currentWriteIndex = m_writeIndex.load(std::memory_order_relaxed);
        const size_t nextWriteIndex = (currentWriteIndex + 1) & m_capacityMask;

        if (nextWriteIndex == m_readIndex.load(std::memory_order_acquire)) {
            return false;
        }

        m_pBuffer[currentWriteIndex] = data;
        m_writeIndex.store(nextWriteIndex, std::memory_order_release);
        return true;
    }

    /**
     * @brief �����͸� ť���� ���� (�Һ��� ����)
     * @param outData ����� �����͸� ������ ���� ����
     * @return true ���� ����, false ť�� ��� ����
     */
    bool PopData(T& outData) {
        const size_t currentReadIndex = m_readIndex.load(std::memory_order_relaxed);

        if (currentReadIndex == m_writeIndex.load(std::memory_order_acquire)) {
            return false;
        }

        outData = m_pBuffer[currentReadIndex];
        m_readIndex.store((currentReadIndex + 1) & m_capacityMask, std::memory_order_release);
        return true;
    }

private:
    size_t m_capacityMask;
    T* m_pBuffer;

    // False Sharing ������ ���� �޸� ���� (�Ϲ����� L1 ĳ�� ���� ũ��: 64 ����Ʈ)
    alignas(64) std::atomic<size_t> m_writeIndex;
    alignas(64) std::atomic<size_t> m_readIndex;
};