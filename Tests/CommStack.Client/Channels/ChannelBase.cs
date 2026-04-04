using CommStack.Client.Core;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CommStack.Client.Channels
{
    /// <summary>
    /// 네이티브 통신 채널의 기본 기능을 구현하는 추상 클래스입니다.
    /// </summary>
    public abstract class ChannelBase : IChannel
    {
        protected IntPtr _channelHandle;
        private bool _isDisposed;

        public uint ChannelId { get; private set; }

        protected ChannelBase(ChannelType type, uint id)
        {
            ChannelId = id;
            _channelHandle = NativeMethods.CreateCommunicationChannel((int)type);

            if (_channelHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException($"{type} 채널 생성에 실패했습니다.");
            }
        }

        /// <summary>
        /// 채널의 상세 설정을 초기화하고 네이티브 스레드를 시작합니다.
        /// </summary>
        public virtual bool Initialize(string ip, ushort remotePort, ushort localPort, uint coreIndex)
        {
            var config = new NativeMethods.ChannelConfig
            {
                ChannelId = this.ChannelId,
                TargetIp = ip,
                TargetPort = remotePort,
                LocalPort = localPort,
                BufferSize = 65536, // 2의 거듭제곱 (네이티브 SPSC 큐 크기)
                CpuCoreIndex = coreIndex
            };

            return NativeMethods.OpenCommunicationChannel(_channelHandle, ref config);
        }

        public bool Send(byte[] data)
        {
            if (data == null || data.Length == 0) return false;
            return NativeMethods.SendPacketData(_channelHandle, data, (uint)data.Length);
        }

        public bool Receive(byte[] buffer, out uint receivedSize)
        {
            receivedSize = 0;
            return NativeMethods.ReceivePacketData(_channelHandle, buffer, ref receivedSize);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_isDisposed)
            {
                if (_channelHandle != IntPtr.Zero)
                {
                    NativeMethods.DestroyCommunicationChannel(_channelHandle);
                    _channelHandle = IntPtr.Zero;
                }
                _isDisposed = true;
            }
        }

        ~ChannelBase() => Dispose(false);
    }
}
