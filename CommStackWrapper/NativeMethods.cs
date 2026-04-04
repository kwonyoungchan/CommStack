using System;
using System.Runtime.InteropServices;

/// <summary>
/// CommStack.dll의 네이티브 C++ API를 호출하기 위한 P/Invoke 인터페이스 클래스
/// </summary>
internal static class NativeMethods
{
    private const string DllName = "CommStack.dll";

    /// <summary>
    /// 네이티브 통신 채널 설정 구조체 (C++의 ChannelConfig와 1:1 매핑)
    /// </summary>
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

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern bool OpenCommunicationChannel(IntPtr channelHandle, ref ChannelConfig config);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void DestroyCommunicationChannel(IntPtr channelHandle);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern bool SendPacketData(IntPtr channelHandle, byte[] data, uint dataSizeByte);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern bool ReceivePacketData(IntPtr channelHandle, byte[] outBuffer, ref uint outSizeByte);
}