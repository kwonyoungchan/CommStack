/**
 * @file test_PacketMemoryPool.cpp
 * @brief PacketMemoryPool 단위 테스트
 */
#include "TestFramework.hpp"
#include "../../Include/PacketMemoryPool.hpp"
#include <cstring>
#include <vector>

// ── 슬롯 Acquire / Release ─────────────────────────────────────

TEST(PacketMemoryPool, AcquireAllSlots) {
    constexpr size_t SLOT_COUNT = 4;
    PacketMemoryPool pool(SLOT_COUNT, 64);

    std::vector<size_t> acquired;
    size_t idx;
    while (pool.AcquireSlot(idx)) {
        acquired.push_back(idx);
    }
    // SLOT_COUNT개 슬롯을 모두 가져올 수 있어야 함
    EXPECT_EQ(static_cast<int>(acquired.size()), static_cast<int>(SLOT_COUNT));
}

TEST(PacketMemoryPool, AcquireOnExhaustedPoolReturnsFalse) {
    PacketMemoryPool pool(2, 64);
    size_t idx;
    pool.AcquireSlot(idx);
    pool.AcquireSlot(idx);
    // 풀이 소진된 후에는 false 반환
    EXPECT_FALSE(pool.AcquireSlot(idx));
}

TEST(PacketMemoryPool, ReleaseAndReacquire) {
    PacketMemoryPool pool(2, 64);
    size_t idx1, idx2;
    pool.AcquireSlot(idx1);
    pool.AcquireSlot(idx2);

    // 반환 후 다시 acquire 가능해야 함
    EXPECT_TRUE(pool.ReleaseSlot(idx1));
    size_t idxReused;
    EXPECT_TRUE(pool.AcquireSlot(idxReused));
    EXPECT_EQ(idxReused, idx1);
}

TEST(PacketMemoryPool, SlotIndicesAreUnique) {
    constexpr size_t SLOT_COUNT = 8;
    PacketMemoryPool pool(SLOT_COUNT, 32);

    std::vector<size_t> indices;
    size_t idx;
    while (pool.AcquireSlot(idx)) {
        indices.push_back(idx);
    }
    // 중복 인덱스가 없어야 함
    for (size_t i = 0; i < indices.size(); ++i) {
        for (size_t j = i + 1; j < indices.size(); ++j) {
            EXPECT_NE(indices[i], indices[j]);
        }
    }
}

// ── 버퍼 포인터 정확성 ─────────────────────────────────────────

TEST(PacketMemoryPool, BufferPointerIsNotNull) {
    PacketMemoryPool pool(4, 128);
    size_t idx;
    pool.AcquireSlot(idx);
    EXPECT_TRUE(pool.GetBufferPointer(idx) != nullptr);
}

TEST(PacketMemoryPool, BufferPointersAreContiguous) {
    // 슬롯 포인터 간격이 정확히 slotSizeByte 여야 함
    constexpr size_t SLOT_SIZE = 256;
    PacketMemoryPool pool(4, SLOT_SIZE);

    size_t idx0, idx1;
    pool.AcquireSlot(idx0);
    pool.AcquireSlot(idx1);

    uint8_t* p0 = pool.GetBufferPointer(0);
    uint8_t* p1 = pool.GetBufferPointer(1);

    ptrdiff_t diff = p1 - p0;
    EXPECT_EQ(static_cast<size_t>(diff), SLOT_SIZE);
}

TEST(PacketMemoryPool, WriteAndReadBack) {
    // 슬롯에 데이터를 쓰고 올바르게 읽혀야 함
    constexpr size_t SLOT_SIZE = 64;
    PacketMemoryPool pool(4, SLOT_SIZE);

    size_t idx;
    pool.AcquireSlot(idx);
    uint8_t* buf = pool.GetBufferPointer(idx);

    const char* msg = "CommStack";
    std::memcpy(buf, msg, std::strlen(msg) + 1);
    EXPECT_TRUE(std::memcmp(buf, msg, std::strlen(msg) + 1) == 0);
}

TEST(PacketMemoryPool, SlotsDoNotOverlap) {
    // 서로 다른 슬롯의 버퍼 영역이 겹치지 않아야 함
    constexpr size_t SLOT_SIZE  = 128;
    constexpr size_t SLOT_COUNT = 4;
    PacketMemoryPool pool(SLOT_COUNT, SLOT_SIZE);

    // 슬롯 0에 패턴 A 쓰기
    uint8_t* p0 = pool.GetBufferPointer(0);
    std::memset(p0, 0xAA, SLOT_SIZE);

    // 슬롯 1에 패턴 B 쓰기
    uint8_t* p1 = pool.GetBufferPointer(1);
    std::memset(p1, 0xBB, SLOT_SIZE);

    // 슬롯 0이 오염되지 않았는지 확인
    bool intact = true;
    for (size_t i = 0; i < SLOT_SIZE; ++i) {
        if (p0[i] != 0xAA) { intact = false; break; }
    }
    EXPECT_TRUE(intact);
}
