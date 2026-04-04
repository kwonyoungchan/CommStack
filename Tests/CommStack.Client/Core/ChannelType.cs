using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CommStack.Client.Core
{
    /// <summary>
    /// 통신 프로토콜 타입을 정의합니다.
    /// </summary>
    public enum ChannelType : int
    {
        Udp = 1,
        Tcp = 2
    }
}
