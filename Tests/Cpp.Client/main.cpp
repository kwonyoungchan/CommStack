/**
 * @file main.cpp
 * @brief 다중 채널 통신 프레임워크를 활용하는 C++ 시뮬레이터 구동 환경
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>

#include "ChannelManager.hpp"

 // C++ 시뮬레이터 루프 제어용 원자적(Atomic) 상태 변수 (캐시 최적화 우회 및 스레드 안전성 보장)
std::atomic<bool> g_bIsSimulatorRunning(false);
ChannelManager g_channelManager;

/**
 * @brief 채널에서 수신된 데이터를 처리하는 전역 콜백 함수
 * @param channelId 패킷이 수신된 채널 고유 식별자
 * @param pData 수신된 버퍼의 포인터
 * @param dataSizeByte 유효한 데이터 크기 (Byte)
 */
void ProcessReceivedDataCallback(uint32_t channelId, const uint8_t* pData, uint32_t dataSizeByte) {
    if (channelId == 101) {
        std::string message(reinterpret_cast<const char*>(pData), dataSizeByte);

        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTime;
        localtime_s(&localTime, &currentTime);

        std::cout << "[" << std::put_time(&localTime, "%H:%M:%S") << "] 채널 "
            << channelId << " 수신 - " << message << "\n";
    }
}

/**
 * @brief 시뮬레이터 메인 틱(Tick) 루프를 구동하는 함수
 */
void RunSimulatorMainLoop() {
    uint32_t frameCount = 0;
    std::string dummyPayload = "Simulation_Tick_Data";

    while (g_bIsSimulatorRunning.load()) {
        if (frameCount % 60 == 0) {
            g_channelManager.SendDataToChannel(
                101,
                reinterpret_cast<const uint8_t*>(dummyPayload.c_str()),
                static_cast<uint32_t>(dummyPayload.size())
            );
        }

        g_channelManager.UpdateReceiveData(ProcessReceivedDataCallback);

        frameCount++;

        // 60Hz 주기를 모사하기 위한 16ms 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

/**
 * @brief 애플리케이션 메인 진입점
 */
int main() {
    std::cout << "=== C++ 시뮬레이터 다중 채널 통신 시스템 초기화 ===\n\n";

    // 1. 비행 동역학 통신 채널 초기화 (UDP)
    ChannelCreationInfo flightChannelInfo = { 101, 1, "127.0.0.1", 8001, 8001, 4 };
    bool bFlightSuccess = g_channelManager.AddCommunicationChannel(flightChannelInfo);

    // 2. 무장 시스템 제어 채널 초기화 (TCP)
    ChannelCreationInfo weaponChannelInfo = { 201, 2, "127.0.0.1", 9001, 9001, 5 };
    bool bWeaponSuccess = g_channelManager.AddCommunicationChannel(weaponChannelInfo);

    // 3. 기상/환경 데이터 채널 초기화 (UDP)
    ChannelCreationInfo weatherChannelInfo = { 301, 1, "127.0.0.1", 8002, 8002, 6 };
    bool bWeatherSuccess = g_channelManager.AddCommunicationChannel(weatherChannelInfo);

    std::cout << std::boolalpha;
    std::cout << "비행 채널(ID:101) 초기화: " << bFlightSuccess << "\n";
    std::cout << "무장 채널(ID:201) 초기화: " << bWeaponSuccess << "\n";
    std::cout << "기상 채널(ID:301) 초기화: " << bWeatherSuccess << "\n\n";

    g_bIsSimulatorRunning.store(true);
    std::thread simulatorThread(RunSimulatorMainLoop);

    std::cout << "[System] 정상 가동 중... 종료하려면 [Enter] 키를 누르세요.\n\n";
    std::cin.get();

    std::cout << "[System] 종료 요청이 접수되었습니다.\n";
    std::cout << "[System] 메인 시뮬레이션 루프 중지 신호 전송...\n";
    g_bIsSimulatorRunning.store(false);

    std::cout << "[System] 시뮬레이션 태스크가 안전하게 완료되기를 대기합니다...\n";
    if (simulatorThread.joinable()) {
        simulatorThread.join();
    }
    std::cout << "[System] 시뮬레이션 태스크 종료 완료.\n";

    // 스코프가 종료되면 전역 객체인 g_channelManager의 소멸자가 호출되며 CloseAllChannels()가 수행됨.

    std::cout << "\n=== 시스템이 안전하게 종료되었습니다 ===\n";
    return 0;
}