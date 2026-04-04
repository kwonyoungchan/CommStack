using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CommStack.Client.Core
{
    /// <summary>
    /// 모든 통신 채널이 준수해야 할 인터페이스입니다.
    /// </summary>
    public interface IChannel : IDisposable
    {
        uint ChannelId { get; }
        bool Initialize(string ip, ushort remotePort, ushort localPort, uint coreIndex);
        bool Send(byte[] data);
        bool Receive(byte[] buffer, out uint receivedSize);
    }
}
