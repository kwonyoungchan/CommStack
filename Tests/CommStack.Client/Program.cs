using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using CommStack.Client.Core;
using CommStack.Client.Management;

namespace CommStack.Client
{
    internal class Program
    {
        private static ChannelManager _channelManager;

        // 멀티스레드 환경에서 캐시 최적화를 방지하고 즉각적인 상태 갱신을 보장합니다.
        private static volatile bool _isRunning;

        public static async Task Main(string[] arguments)
        {
            Console.WriteLine("=== 시뮬레이터 다중 채널 통신 시스템 초기화 ===\n");

            using (_channelManager = new ChannelManager())
            {
                InitializeCommunicationChannels();

                _isRunning = true;

                // 백그라운드 태스크 구동
                Task simulatorLoopTask = Task.Run(RunSimulatorMainLoop);

                Console.WriteLine("\n[System] 정상 가동 중... 종료하려면 [Enter] 키를 누르세요.\n");
                Console.ReadLine(); // 사용자의 엔터 입력 대기

                // 종료 시퀀스 시작 (로그 추가)
                Console.WriteLine("\n[System] 종료 요청이 접수되었습니다.");
                Console.WriteLine("[System] 메인 시뮬레이션 루프 중지 신호 전송...");
                _isRunning = false;

                Console.WriteLine("[System] 시뮬레이션 태스크가 안전하게 완료되기를 대기합니다...");
                await simulatorLoopTask;
                Console.WriteLine("[System] 시뮬레이션 태스크 종료 완료.");

            } // 여기서 ChannelManager의 Dispose()가 호출되며 네이티브 C++ 스레드가 종료됩니다.

            Console.WriteLine("\n=== 시스템이 안전하게 종료되었습니다 ===");
            Console.WriteLine("프로그램을 완전히 종료하려면 아무 키나 누르세요...");
            Console.ReadKey(); // 콘솔 창이 즉시 닫히는 현상 방지
        }

        private static void InitializeCommunicationChannels()
        {
            // UDP 비행 채널
            bool isFlightChannelAdded = _channelManager.AddChannel(
                type: ChannelType.Udp, id: 101, ip: "127.0.0.1", remotePort: 8001, localPort: 8001, coreIndex: 4);

            // TCP 무장 채널 (현재 수신 대기 중인 서버가 없으므로 연결 실패(False)가 정상입니다)
            bool isWeaponChannelAdded = _channelManager.AddChannel(
                type: ChannelType.Tcp, id: 201, ip: "127.0.0.1", remotePort: 9001, localPort: 9001, coreIndex: 8);

            // UDP 기상 채널
            bool isWeatherChannelAdded = _channelManager.AddChannel(
                type: ChannelType.Udp, id: 301, ip: "127.0.0.1", remotePort: 8002, localPort: 8002, coreIndex: 6);

            Console.WriteLine($"비행 채널(ID:101) 초기화: {isFlightChannelAdded}");
            Console.WriteLine($"무장 채널(ID:201) 초기화: {isWeaponChannelAdded}");
            Console.WriteLine($"기상 채널(ID:301) 초기화: {isWeatherChannelAdded}");
        }

        private static async Task RunSimulatorMainLoop()
        {
            uint frameCount = 0;
            byte[] dummyPayload = Encoding.UTF8.GetBytes("Simulation_Tick_Data");

            while (_isRunning)
            {
                if (frameCount % 60 == 0)
                {
                    _channelManager.SendTo(101, dummyPayload);
                }

                _channelManager.UpdateReceive(ProcessReceivedData);

                frameCount++;
                await Task.Delay(16);
            }
        }

        private static void ProcessReceivedData(uint sourceChannelId, byte[] dataBuffer, uint dataSize)
        {
            if (sourceChannelId == 101)
            {
                string message = Encoding.UTF8.GetString(dataBuffer, 0, (int)dataSize);
                Console.WriteLine($"[{DateTime.Now:HH:mm:ss.fff}] 채널 {sourceChannelId} 수신 - {message}");
            }
        }
    }
}