using CommStack.Client.Core;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CommStack.Client.Channels
{
    public class UdpChannel : ChannelBase
    {
        public UdpChannel(uint id) : base(ChannelType.Udp, id) { }
    }
}
