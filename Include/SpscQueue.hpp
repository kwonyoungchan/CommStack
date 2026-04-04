/**
 * @file SpscQueue.hpp
 * @brief 단일 생산자-단일 소비자(SPSC) 구조의 락프리 큐
 * @details 캐시 라인 충돌을 방지하며, 메모리 배리어를 통해 스레드 안전성을 보장합니다.
 */

#pragma once
#include <atomic>
#include <stdexcept>

template<typename T>
class SpscQueue {
public:
    /**
     * @brief 큐 초기화
     * @param capacity 큐의 최대 크기 (반드시 2의 거듭제곱이어야 함)
     * @exception std::invalid_argument 크기가 2의 거듭제곱이 아닐 경우 발생
     */
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
     * @brief 데이터를 큐에 삽입 (생산자 전용)
     * @param data 삽입할 데이터
     * @return true 삽입 성공, false 큐가 가득 참
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
     * @brief 데이터를 큐에서 추출 (소비자 전용)
     * @param outData 추출된 데이터를 저장할 참조 변수
     * @return true 추출 성공, false 큐가 비어 있음
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

    // False Sharing 방지를 위한 메모리 정렬 (일반적인 L1 캐시 라인 크기: 64 바이트)
    alignas(64) std::atomic<size_t> m_writeIndex;
    alignas(64) std::atomic<size_t> m_readIndex;
};