using System;
using System.Runtime.InteropServices;

namespace CommStack.Client.Core
{
    internal static class NativeMethods
    {
        private const string DllName = "CommStack.dll";

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct ChannelConfig
        {
            public uint ChannelId;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)]
            public string TargetIp;
            public ushort TargetPort;
            public ushort LocalPort;
            public uint BufferSize;
            public uint CpuCoreIndex;
        }

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr CreateCommunicationChannel(int channelType);

        // 반환값이 bool인 모든 함수에 마샬링 지시자 추가
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool OpenCommunicationChannel(IntPtr channelHandle, ref ChannelConfig config);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void DestroyCommunicationChannel(IntPtr channelHandle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool SendPacketData(IntPtr channelHandle, byte[] data, uint dataSizeByte);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool ReceivePacketData(IntPtr channelHandle, byte[] outBuffer, ref uint outSizeByte);
    }
}