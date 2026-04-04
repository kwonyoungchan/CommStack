/**
 * @file BenchmarkNetworkDllMain.cpp
 * @brief CommStack.dll의 외부 C-API를 이용한 UDP/TCP 수신 성능 통합 벤치마크 (진행률 표시 포함)
 */

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../../Include/CommApi.hpp"

#pragma comment(lib, "ws2_32.lib")

constexpr uint32_t TOTAL_TEST_BYTES = 1024 * 1024 * 500; // 500 MB 테스트
constexpr uint32_t SEND_CHUNK_SIZE_BYTE = 1024;          // 1 KB 단위 송신
constexpr uint16_t UDP_TEST_PORT = 7000;
constexpr uint16_t TCP_TEST_PORT = 7001;
constexpr const char* LOOPBACK_IP = "127.0.0.1";

void InitializeWinsock() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void RunUdpBenchmarkDll() {
    std::cout << "[UDP DLL Benchmark] 초기화 중...\n";

    void* pChannel = CreateCommunicationChannel(1);
    if (pChannel == nullptr) {
        std::cerr << "DLL 채널 생성 실패\n";
        return;
    }

    ChannelConfig config = {};
    config.m_nChannelId = 1;
    config.m_nLocalPort = UDP_TEST_PORT;
    config.m_nTargetPort = 0;
    config.m_nBufferSize = 65536;
    config.m_nCpuCoreIndex = 4;
    strncpy_s(config.m_szTargetIp, LOOPBACK_IP, sizeof(config.m_szTargetIp));

    if (!OpenCommunicationChannel(pChannel, &config)) {
        std::cerr << "DLL 채널 오픈 실패\n";
        DestroyCommunicationChannel(pChannel);
        return;
    }

    SOCKET senderSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in targetAddress = {};
    targetAddress.sin_family = AF_INET;
    targetAddress.sin_port = htons(UDP_TEST_PORT);
    inet_pton(AF_INET, LOOPBACK_IP, &targetAddress.sin_addr);

    std::vector<uint8_t> dummyData(SEND_CHUNK_SIZE_BYTE, 0xAA);
    auto startTime = std::chrono::high_resolution_clock::now();

    std::thread senderThread([&]() {
        uint32_t sentBytes = 0;
        while (sentBytes < TOTAL_TEST_BYTES) {
            sendto(senderSocket, reinterpret_cast<const char*>(dummyData.data()), SEND_CHUNK_SIZE_BYTE, 0,
                reinterpret_cast<sockaddr*>(&targetAddress), sizeof(targetAddress));
            sentBytes += SEND_CHUNK_SIZE_BYTE;

            if (sentBytes % (SEND_CHUNK_SIZE_BYTE * 1000) == 0) {
                std::this_thread::yield();
            }
        }
        });

    uint32_t totalReceivedBytes = 0;
    std::vector<uint8_t> receiveBuffer(2048);
    uint32_t lastPrintedPercent = 0;

    std::cout << "[UDP DLL Benchmark] 수신 시작...\n";

    // 타임아웃 측정을 위한 시간 기록 변수 추가
    auto lastReceiveTime = std::chrono::high_resolution_clock::now();

    while (totalReceivedBytes < TOTAL_TEST_BYTES) {
        uint32_t receivedChunkSize = 0;

        // 큐에서 수신 성공 시
        if (ReceivePacketData(pChannel, receiveBuffer.data(), &receivedChunkSize)) {
            totalReceivedBytes += receivedChunkSize;

            // 패킷을 받았으므로 마지막 수신 시간을 갱신
            lastReceiveTime = std::chrono::high_resolution_clock::now();

            uint32_t currentPercent = static_cast<uint32_t>((static_cast<uint64_t>(totalReceivedBytes) * 100) / TOTAL_TEST_BYTES);
            if (currentPercent > lastPrintedPercent) {
                std::cout << "\r  -> 진행률: " << currentPercent << "% ("
                    << (totalReceivedBytes / (1024 * 1024)) << " MB / 500 MB)" << std::flush;
                lastPrintedPercent = currentPercent;
            }
        }
        // 큐가 비어서 수신 실패 시 (대기 상태)
        else {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastReceiveTime).count();

            // 1000ms(1초) 동안 아무 패킷도 들어오지 않았다면 타임아웃 처리
            if (elapsedMs > 1000) {
                std::cout << "\n\n[Warning] 패킷 유실로 인한 타임아웃 발생! (OS 버퍼 초과)\n";
                std::cout << "최종 수신량: " << (totalReceivedBytes / (1024 * 1024)) << " MB (유실률: "
                    << 100 - static_cast<uint32_t>((static_cast<uint64_t>(totalReceivedBytes) * 100) / TOTAL_TEST_BYTES)
                    << "%)\n";
                break; // 무한 루프 탈출
            }
        }
    }
    std::cout << "\n";
    senderThread.join();
    closesocket(senderSocket);
    DestroyCommunicationChannel(pChannel);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "[UDP DLL Benchmark] 완료 - 소요 시간: " << durationMs << " ms ("
        << (500000.0 / durationMs) << " MB/s)\n\n";
}

void RunTcpBenchmarkDll() {
    std::cout << "[TCP DLL Benchmark] 더미 서버 기동 및 대기 중...\n";

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(TCP_TEST_PORT);
    inet_pton(AF_INET, LOOPBACK_IP, &serverAddress.sin_addr);

    bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress));
    listen(serverSocket, SOMAXCONN);

    std::thread serverThread([&]() {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        std::vector<uint8_t> dummyData(SEND_CHUNK_SIZE_BYTE, 0xBB);
        uint32_t sentBytes = 0;

        while (sentBytes < TOTAL_TEST_BYTES) {
            int result = send(clientSocket, reinterpret_cast<const char*>(dummyData.data()), SEND_CHUNK_SIZE_BYTE, 0);
            if (result > 0) {
                sentBytes += result;
            }
        }
        closesocket(clientSocket);
        });

    void* pChannel = CreateCommunicationChannel(2);
    if (pChannel == nullptr) {
        std::cerr << "DLL 채널 생성 실패\n";
        serverThread.join();
        closesocket(serverSocket);
        return;
    }

    ChannelConfig config = {};
    config.m_nChannelId = 2;
    config.m_nTargetPort = TCP_TEST_PORT;
    config.m_nBufferSize = 65536;
    config.m_nCpuCoreIndex = 5;
    strncpy_s(config.m_szTargetIp, LOOPBACK_IP, sizeof(config.m_szTargetIp));

    if (!OpenCommunicationChannel(pChannel, &config)) {
        std::cerr << "DLL 채널 오픈 실패\n";
        DestroyCommunicationChannel(pChannel);
        serverThread.join();
        closesocket(serverSocket);
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    uint32_t totalReceivedBytes = 0;
    std::vector<uint8_t> receiveBuffer(2048);
    uint32_t lastPrintedPercent = 0;

    std::cout << "[TCP DLL Benchmark] 수신 시작...\n";

    while (totalReceivedBytes < TOTAL_TEST_BYTES) {
        uint32_t receivedChunkSize = 0;
        if (ReceivePacketData(pChannel, receiveBuffer.data(), &receivedChunkSize)) {
            totalReceivedBytes += receivedChunkSize;

            // 진행률 계산 및 1% 단위 갱신
            uint32_t currentPercent = static_cast<uint32_t>((static_cast<uint64_t>(totalReceivedBytes) * 100) / TOTAL_TEST_BYTES);
            if (currentPercent > lastPrintedPercent) {
                std::cout << "\r  -> 진행률: " << currentPercent << "% ("
                    << (totalReceivedBytes / (1024 * 1024)) << " MB / 500 MB)" << std::flush;
                lastPrintedPercent = currentPercent;
            }
        }
    }
    std::cout << "\n"; // 진행률 표시 종료 후 줄바꿈

    auto endTime = std::chrono::high_resolution_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    serverThread.join();
    closesocket(serverSocket);
    DestroyCommunicationChannel(pChannel);

    std::cout << "[TCP DLL Benchmark] 완료 - 소요 시간: " << durationMs << " ms ("
        << (500000.0 / durationMs) << " MB/s)\n\n";
}

int main() {
    InitializeWinsock();

    std::cout << "=== CommStack.dll 네트워크 수신(Throughput) 통합 벤치마크 ===\n\n";

    RunUdpBenchmarkDll();
    Sleep(1000);
    RunTcpBenchmarkDll();

    std::cout << "=== 벤치마크 종료 ===\n";
    WSACleanup();
    return 0;
}