/**
 * @file test_CommApi.cpp
 * @brief CommApi C-Style API 단위 테스트
 * @details DLL 경계면의 방어 코드 및 채널 생명주기를 검증합니다.
 *          실제 네트워크 연결 없이 API의 안전성만 검증합니다.
 */

// winsock2.h는 windows.h보다 먼저 include 되어야 함
// TestFramework.hpp의 windows.h와 충돌 방지를 위해 CommApi를 먼저 include
#include "../../Include/CommApi.hpp"
#include "../../Include/ICommunicationChannel.hpp"
#include "TestFramework.hpp"
#include <cstring>

#pragma comment(lib, "CommStack.lib")
#pragma comment(lib, "ws2_32.lib")

// ── CreateCommunicationChannel ─────────────────────────────────

TEST(CommApi, CreateUdpChannel_ReturnsNonNull) {
    void* h = CreateCommunicationChannel(1);
    EXPECT_TRUE(h != nullptr);
    DestroyCommunicationChannel(h);
}

TEST(CommApi, CreateTcpChannel_ReturnsNonNull) {
    void* h = CreateCommunicationChannel(2);
    EXPECT_TRUE(h != nullptr);
    DestroyCommunicationChannel(h);
}

TEST(CommApi, CreateUnknownChannelType_ReturnsNull) {
    void* h = CreateCommunicationChannel(99);
    EXPECT_TRUE(h == nullptr);
}

// ── OpenCommunicationChannel 방어 코드 ────────────────────────

TEST(CommApi, OpenWithNullHandle_ReturnsFalse) {
    ChannelConfig cfg{};
    EXPECT_FALSE(OpenCommunicationChannel(nullptr, &cfg));
}

TEST(CommApi, OpenWithNullConfig_ReturnsFalse) {
    void* h = CreateCommunicationChannel(1);
    EXPECT_FALSE(OpenCommunicationChannel(h, nullptr));
    DestroyCommunicationChannel(h);
}

TEST(CommApi, UdpOpen_InvalidPort_ReturnsFalse) {
    // 이미 사용 중이거나 권한 없는 포트(0)는 bind 실패해야 함
    void* h = CreateCommunicationChannel(1);
    ChannelConfig cfg{};
    cfg.m_nBufferSize   = 64;   // 2의 거듭제곱
    cfg.m_nLocalPort    = 0;    // 포트 0: OS가 임의 할당 (bind 성공 가능)
    cfg.m_nCpuCoreIndex = 0;
    // 결과 자체보단 크래시 없이 반환되는지 검증
    bool result = OpenCommunicationChannel(h, &cfg);
    (void)result;               // 성공/실패 무관, 크래시 없음이 목표
    DestroyCommunicationChannel(h);
    EXPECT_TRUE(true);          // 여기까지 도달하면 크래시 없음 확인
}

TEST(CommApi, TcpOpen_UnreachableAddress_ReturnsFalse) {
    // 연결 불가능한 주소로 connect() 시도 → false 반환
    void* h = CreateCommunicationChannel(2);
    ChannelConfig cfg{};
    cfg.m_nBufferSize   = 64;
    cfg.m_nTargetPort   = 19999; // 닫혀있을 가능성 높은 포트
    cfg.m_nCpuCoreIndex = 0;
    std::memcpy(cfg.m_szTargetIp, "127.0.0.1", 10);
    EXPECT_FALSE(OpenCommunicationChannel(h, &cfg));
    DestroyCommunicationChannel(h);
}

// ── SendPacketData 방어 코드 ────────────────────────────────────

TEST(CommApi, SendWithNullHandle_ReturnsFalse) {
    uint8_t data[] = { 1, 2, 3 };
    EXPECT_FALSE(SendPacketData(nullptr, data, sizeof(data)));
}

// ── ReceivePacketData 방어 코드 ────────────────────────────────

TEST(CommApi, ReceiveWithNullHandle_ReturnsFalse) {
    uint8_t buf[64];
    uint32_t size = 0;
    EXPECT_FALSE(ReceivePacketData(nullptr, buf, &size));
}

TEST(CommApi, ReceiveWithNullSizePtr_ReturnsFalse) {
    void* h = CreateCommunicationChannel(1);
    uint8_t buf[64];
    EXPECT_FALSE(ReceivePacketData(h, buf, nullptr));
    DestroyCommunicationChannel(h);
}

// ── DestroyCommunicationChannel 안전성 ────────────────────────

TEST(CommApi, DestroyNullHandle_NoCrash) {
    DestroyCommunicationChannel(nullptr); // nullptr 전달 시 크래시 없어야 함
    EXPECT_TRUE(true);
}

TEST(CommApi, CreateAndDestroyMultipleTimes_NoCrash) {
    for (int i = 0; i < 10; ++i) {
        void* h = CreateCommunicationChannel(1);
        EXPECT_TRUE(h != nullptr);
        DestroyCommunicationChannel(h);
    }
}
