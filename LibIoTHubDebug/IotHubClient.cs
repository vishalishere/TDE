using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.Devices.Client;
using Newtonsoft.Json;

namespace LibIoTHubDebug
{
    public delegate void MsgHandler(Object o, String s);

    public sealed class IotHubClient
    {
        private sealed class DataPoint
        {
            public LibAudio.HeartBeatType t;
            public DateTime time;
            public int cc;
            public int asdf;
            public int peak;
            public ulong threshold;
            public ulong volume;
        }

        private int MAX_MESSAGES = 100;

        private Object thisLock = new Object();
        private int msgCount = 0;

        System.Collections.Generic.Queue<DataPoint> queue = new System.Collections.Generic.Queue<DataPoint>();
        Task task = null;

        public IotHubClient()
        {
        }

        public int Messages()
        {
            int count = 0;
            lock(thisLock)
            {
                count = queue.Count;
            }
            return count;
        }

        public int Sent()
        {
            int tmp;
            lock (thisLock)
            {
                for (int i = 0; i < msgCount; i++)
                {
                    queue.Dequeue();
                }
                tmp = msgCount;
                msgCount = 0;
            }    
            return tmp;
        }

        public void AddMessage(LibAudio.HeartBeatType t, int cc, int asdf, int peak, ulong threshold, ulong volume)
        {
            DataPoint p = new DataPoint();
            p.time = DateTime.UtcNow;
            p.t = t;
            p.cc = cc;
            p.asdf = asdf;
            p.peak = peak;
            p.threshold = threshold;
            p.volume = volume;

            lock (thisLock)
            {
                if (queue.Count == MAX_MESSAGES) queue.Dequeue();
                queue.Enqueue(p);
            }
        }

        public bool SendMessagesAsync(Windows.Foundation.AsyncActionCompletedHandler handler)
        {
            int count = 0;
            lock(thisLock)
            {
                count = queue.Count;
            }
            if (count > 0)
            {
                Func<Task> action = async () =>
                {
                    System.Collections.Generic.Queue<Message> q = new System.Collections.Generic.Queue<Message>();
                    lock (thisLock)
                    {
                        foreach (DataPoint p in queue)
                        {
                            string s = HeartBeatText(p.t);
                            var telemetryDataPoint = new
                            {
                                ID = AccessData.Access.DeviceID,
                                TIME = p.time,
                                MSG = s,
                                CC = p.cc,
                                ADSF = p.asdf,
                                PEAK = p.peak,
                                THRES = p.threshold,
                                VOL = p.volume
                            };
                            var messageString = JsonConvert.SerializeObject(telemetryDataPoint);
                            var message = new Message(Encoding.ASCII.GetBytes(messageString));
                            q.Enqueue(message);
                        }
                        msgCount = queue.Count;
                    }
                    var auth = new DeviceAuthenticationWithRegistrySymmetricKey(AccessData.Access.DeviceID, AccessData.Access.DeviceKey);
                    DeviceClient deviceClient = DeviceClient.Create(AccessData.Access.IoTHubUri, auth, TransportType.Http1);
                    await deviceClient.OpenAsync();
                    Windows.Foundation.IAsyncAction a = deviceClient.SendEventBatchAsync(q);
                    a.Completed = handler;
                    await a;
                    await deviceClient.CloseAsync();
                };
                task = Task.Factory.StartNew(action);
                return true;
            }
            else return false;
        }

        public string HeartBeatText(LibAudio.HeartBeatType t)
        {
            switch (t)
            {
                case LibAudio.HeartBeatType.DATA: return "DATA";
                case LibAudio.HeartBeatType.INVALID: return "INVALID";
                case LibAudio.HeartBeatType.SILENCE: return "SILENCE";
                case LibAudio.HeartBeatType.BUFFERING: return "BUFFERING";
                case LibAudio.HeartBeatType.DEVICE_ERROR: return "ERROR";
                case LibAudio.HeartBeatType.NODEVICE: return "NO DEVICES";
                default: return "";
            }
        }
    }
}
