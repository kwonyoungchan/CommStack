using System.Collections.Concurrent;
using CommStack.Client.Core;
using CommStack.Client.Channels;

namespace CommStack.Client.Management;

/// <summary>
/// 다수의 통신 채널을 관리하고 데이터를 라우팅하는 매니저 클래스입니다.
/// </summary>
public class ChannelManager : IDisposable
{
    private readonly ConcurrentDictionary<uint, IChannel> _channels = new();
    private readonly byte[] _receiveSharedBuffer = new byte[65536]; // GC 부하 방지용 공유 버퍼

    /// <summary>
    /// 새로운 채널을 생성하고 관리 목록에 추가합니다.
    /// </summary>
    public bool AddChannel(ChannelType type, uint id, string ip, ushort remotePort, ushort localPort, uint coreIndex)
    {
        IChannel channel = type switch
        {
            ChannelType.Udp => new UdpChannel(id),
            ChannelType.Tcp => new TcpChannel(id),
            _ => throw new NotSupportedException("지원되지 않는 프로토콜 타입입니다.")
        };

        if (channel.Initialize(ip, remotePort, localPort, coreIndex))
        {
            return _channels.TryAdd(id, channel);
        }

        channel.Dispose();
        return false;
    }

    /// <summary>
    /// 특정 채널 ID를 사용하여 유니캐스트 방식으로 데이터를 송신합니다.
    /// </summary>
    public bool SendTo(uint channelId, byte[] data)
    {
        if (_channels.TryGetValue(channelId, out var channel))
        {
            return channel.Send(data);
        }
        return false;
    }

    /// <summary>
    /// 모든 활성 채널로부터 데이터를 폴링하여 콜백으로 전달합니다.
    /// </summary>
    public void UpdateReceive(Action<uint, byte[], uint> onDataAction)
    {
        foreach (var channel in _channels.Values)
        {
            // 한 프레임(Tick)당 단일 채널에서 최대 처리할 패킷 수 제한 (예: 1000개)
            // 이를 통해 특정 채널의 폭주로 인해 시뮬레이터 전체가 멈추는 현상을 방지합니다.
            int burstLimit = 1000;

            while (burstLimit-- > 0 && channel.Receive(_receiveSharedBuffer, out uint size))
            {
                // size가 0보다 클 때만 콜백 호출 (방어적 프로그래밍)
                if (size > 0)
                {
                    onDataAction?.Invoke(channel.ChannelId, _receiveSharedBuffer, size);
                }
            }
        }
    }

    public void Dispose()
    {
        foreach (var channel in _channels.Values)
        {
            channel.Dispose();
        }
        _channels.Clear();
    }
}