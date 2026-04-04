using CommStack.Client.Core;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CommStack.Client.Channels
{
    public class TcpChannel : ChannelBase
    {
        public TcpChannel(uint id) : base(ChannelType.Tcp, id) { }
    }
}
