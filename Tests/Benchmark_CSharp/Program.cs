using System;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;

/// <summary>
/// C#과 C++ 네이티브(CommStack.dll) 간의 P/Invoke 성능 벤치마크 프로그램
/// </summary>
class Program
{
    private const int TotalPacketCount = 1_000_000; // 100만 개 패킷 테스트
    private const int PacketSizeByte = 1024;        // 1KB 패킷

    static void Main(string[] args)
    {
        Console.WriteLine($"=== C# P/Invoke 통합 통신 벤치마크 시작 (패킷: {TotalPacketCount}개) ===\n");

        using (var udpChannel = new UdpChannelWrapper())
        {
            // 루프백(127.0.0.1)으로 자기 자신에게 송수신하도록 설정 (동일 포트 5000 사용)
            // C++ 내부 수신 스레드는 4번 논리 코어에 고정(Affinity)됩니다.
            if (!udpChannel.Initialize(1, "127.0.0.1", 5000, 5000, 4))
            {
                Console.WriteLine("채널 초기화에 실패했습니다.");
                return;
            }

            Console.WriteLine("네이티브 채널이 성공적으로 열렸습니다. 벤치마크를 시작합니다...\n");

            // 송수신 대기를 위한 카운트다운 이벤트
            var startSignal = new ManualResetEventSlim(false);

            // 1. 수신(Consumer) 태스크 (C#에서 P/Invoke로 C++ 락프리 큐를 Polling)
            var consumerTask = Task.Run(() =>
            {
                byte[] receiveBuffer = new byte[PacketSizeByte];
                uint receivedCount = 0;

                startSignal.Wait(); // 시작 신호 대기

                while (receivedCount < TotalPacketCount)
                {
                    // 네이티브 큐에서 데이터를 꺼내옵니다.
                    if (udpChannel.Receive(receiveBuffer, out uint actualSize))
                    {
                        receivedCount++;
                    }
                }
            });

            // 2. 송신(Producer) 태스크 (C#에서 P/Invoke로 데이터 밀어넣기)
            var producerTask = Task.Run(() =>
            {
                byte[] sendData = new byte[PacketSizeByte];
                // 더미 데이터 채우기
                Array.Fill(sendData, (byte)0xFF);

                startSignal.Wait(); // 시작 신호 대기

                for (int i = 0; i < TotalPacketCount; i++)
                {
                    // C++ DLL로 데이터 전송 (루프백 네트워크를 타고 다시 C++ 수신 스레드로 들어감)
                    while (!udpChannel.Send(sendData))
                    {
                        // 송신 버퍼가 가득 찼다면 잠시 대기
                        Thread.Yield();
                    }
                }
            });

            // 타이머 시작 및 스레드 동시 구동
            var stopwatch = Stopwatch.StartNew();
            startSignal.Set();

            // 두 태스크가 모두 완료될 때까지 대기
            Task.WaitAll(producerTask, consumerTask);

            stopwatch.Stop();

            Console.WriteLine($"[C# <-> C++] 벤치마크 완료: {stopwatch.ElapsedMilliseconds} ms");
            Console.WriteLine($"초당 처리량(TPS): {(TotalPacketCount / stopwatch.Elapsed.TotalSeconds):N0} Packets/sec");
        }
    }
}