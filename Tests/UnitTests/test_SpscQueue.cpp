/**
 * @file test_SpscQueue.cpp
 * @brief SpscQueue 단위 테스트
 */
#include "../../Include/SpscQueue.hpp"
#include "TestFramework.hpp"
#include <stdexcept>
#include <thread>

// ── 생성 및 유효성 검사 ────────────────────────────────────────

TEST(SpscQueue, CapacityMustBePowerOfTwo_Valid) {
    // 2의 거듭제곱은 예외 없이 생성되어야 함
    EXPECT_TRUE(([]{ SpscQueue<int> q(2);    return true; })());
    EXPECT_TRUE(([]{ SpscQueue<int> q(4);    return true; })());
    EXPECT_TRUE(([]{ SpscQueue<int> q(1024); return true; })());
}

TEST(SpscQueue, CapacityMustBePowerOfTwo_Invalid) {
    // 2의 거듭제곱이 아니면 std::invalid_argument 발생
    EXPECT_THROW(SpscQueue<int> q(3),  std::invalid_argument);
    EXPECT_THROW(SpscQueue<int> q(5),  std::invalid_argument);
    EXPECT_THROW(SpscQueue<int> q(100),std::invalid_argument);
}

// ── 기본 Push / Pop ────────────────────────────────────────────

TEST(SpscQueue, PushAndPopSingleElement) {
    SpscQueue<int> q(4);
    EXPECT_TRUE(q.PushData(42));
    int out = 0;
    EXPECT_TRUE(q.PopData(out));
    EXPECT_EQ(out, 42);
}

TEST(SpscQueue, PopOnEmptyReturnsFalse) {
    SpscQueue<int> q(4);
    int out = 0;
    EXPECT_FALSE(q.PopData(out));
}

TEST(SpscQueue, PushOnFullReturnsFalse) {
    // capacity=4 → 실제 저장 가능 슬롯 = 3 (SPSC 특성상 1슬롯 희생)
    SpscQueue<int> q(4);
    EXPECT_TRUE(q.PushData(1));
    EXPECT_TRUE(q.PushData(2));
    EXPECT_TRUE(q.PushData(3));
    EXPECT_FALSE(q.PushData(4)); // 이 시점에 가득 참
}

TEST(SpscQueue, FifoOrder) {
    SpscQueue<int> q(8);
    for (int i = 0; i < 5; ++i) q.PushData(i);
    for (int i = 0; i < 5; ++i) {
        int out = -1;
        q.PopData(out);
        EXPECT_EQ(out, i);
    }
}

TEST(SpscQueue, ReleaseAndReuseSlots) {
    // pop 후 다시 push 가능한지 (슬롯 재활용)
    SpscQueue<int> q(4);
    q.PushData(1); q.PushData(2); q.PushData(3);
    int out;
    q.PopData(out); // 1 pop → 슬롯 1개 반환
    EXPECT_TRUE(q.PushData(4)); // 다시 push 가능해야 함
}

TEST(SpscQueue, StructElement) {
    struct Point { int x, y; };
    SpscQueue<Point> q(4);
    EXPECT_TRUE(q.PushData({ 10, 20 }));
    Point out{};
    EXPECT_TRUE(q.PopData(out));
    EXPECT_EQ(out.x, 10);
    EXPECT_EQ(out.y, 20);
}

// ── 멀티스레드: SPSC 정확성 검증 ──────────────────────────────

TEST(SpscQueue, SingleProducerSingleConsumer_NoDataLoss) {
    constexpr int COUNT    = 1000;
    constexpr int CAPACITY = 2048;
    SpscQueue<int> q(CAPACITY);

    std::vector<int> received;
    received.reserve(COUNT);

    // 생산자 스레드
    std::thread producer([&] {
        for (int i = 0; i < COUNT; ++i) {
            while (!q.PushData(i)) { /* busy wait */ }
        }
    });

    // 소비자 (메인 스레드)
    int count = 0;
    while (count < COUNT) {
        int val;
        if (q.PopData(val)) {
            received.push_back(val);
            ++count;
        }
    }
    producer.join();

    EXPECT_EQ(static_cast<int>(received.size()), COUNT);
    for (int i = 0; i < COUNT; ++i) {
        EXPECT_EQ(received[i], i);
    }
}
