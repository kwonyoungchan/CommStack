using System;

/// <summary>
/// 네이티브 UDP 채널을 캡슐화하여 시뮬레이터 로직에 제공하는 통신 래퍼 클래스
/// </summary>
public class UdpChannelWrapper : IDisposable
{
    private IntPtr _channelHandle;
    private bool _isDisposed;

    /// <summary>
    /// UdpChannelWrapper의 인스턴스를 초기화하고 네이티브 UDP 채널을 생성합니다.
    /// </summary>
    public UdpChannelWrapper()
    {
        // 1번 타입(UDP) 채널 생성
        _channelHandle = NativeMethods.CreateCommunicationChannel(1);
        if (_channelHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException("네이티브 통신 채널 생성에 실패했습니다.");
        }
    }

    /// <summary>
    /// 채널을 설정하고 내부 통신 스레드를 시작합니다.
    /// </summary>
    /// <param name="channelId">채널 고유 식별자</param>
    /// <param name="targetIp">목적지 IP 주소</param>
    /// <param name="targetPort">목적지 포트 번호</param>
    /// <param name="localPort">로컬 수신 포트 번호</param>
    /// <param name="cpuCoreIndex">수신 스레드를 할당할 논리 코어 인덱스</param>
    /// <returns>채널 오픈 성공 여부</returns>
    public bool Initialize(uint channelId, string targetIp, ushort targetPort, ushort localPort, uint cpuCoreIndex)
    {
        var config = new NativeMethods.ChannelConfig
        {
            ChannelId = channelId,
            TargetIp = targetIp,
            TargetPort = targetPort,
            LocalPort = localPort,
            BufferSize = 2048, // 2의 거듭제곱 유지
            CpuCoreIndex = cpuCoreIndex
        };

        return NativeMethods.OpenCommunicationChannel(_channelHandle, ref config);
    }

    /// <summary>
    /// 데이터를 네이티브 송신 큐(SPSC)에 밀어넣습니다.
    /// </summary>
    /// <param name="data">송신할 데이터 배열</param>
    /// <returns>송신 큐 삽입 성공 여부</returns>
    public bool Send(byte[] data)
    {
        if (_isDisposed || _channelHandle == IntPtr.Zero || data == null || data.Length == 0)
        {
            return false;
        }

        return NativeMethods.SendPacketData(_channelHandle, data, (uint)data.Length);
    }

    /// <summary>
    /// 네이티브 수신 큐(SPSC)에서 대기 중인 패킷을 꺼내옵니다.
    /// </summary>
    /// <param name="buffer">데이터를 담을 버퍼</param>
    /// <param name="receivedSize">실제 수신된 데이터 크기</param>
    /// <returns>수신된 데이터 존재 여부</returns>
    public bool Receive(byte[] buffer, out uint receivedSize)
    {
        receivedSize = 0;

        if (_isDisposed || _channelHandle == IntPtr.Zero || buffer == null)
        {
            return false;
        }

        return NativeMethods.ReceivePacketData(_channelHandle, buffer, ref receivedSize);
    }

    /// <summary>
    /// 네이티브 리소스를 안전하게 해제합니다.
    /// </summary>
    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    /// <summary>
    /// 실제 리소스 해제 로직을 수행합니다.
    /// </summary>
    /// <param name="disposing">관리되는 리소스 해제 여부</param>
    protected virtual void Dispose(bool disposing)
    {
        if (!_isDisposed)
        {
            if (_channelHandle != IntPtr.Zero)
            {
                // C++ 내부의 스레드 종료 및 메모리 해제 호출
                NativeMethods.DestroyCommunicationChannel(_channelHandle);
                _channelHandle = IntPtr.Zero;
            }
            _isDisposed = true;
        }
    }

    ~UdpChannelWrapper()
    {
        Dispose(false);
    }
}