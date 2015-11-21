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
        private Object thisLock = new Object();
        private int msgCount = 0;

        System.Collections.Generic.Queue<Message> queue;
        Task task = null;

        public IotHubClient()
        {
            queue = new System.Collections.Generic.Queue<Message>();
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

        public int Send()
        {
            int tmp = msgCount;
            msgCount = 0;
            return tmp;
        }

        public void AddMessage(int msg, int cc, int asdf, int peak, System.UInt64 threshold, System.UInt64 volume)
        {
            var telemetryDataPoint = new
            {
                ID = AccessData.Access.DeviceID,
                TIME = DateTime.UtcNow,
                MSG = msg,
                CC = cc,
                ADSF = asdf,
                PEAK = peak,
                THRES = threshold,
                VOL = volume
            };

            var messageString = JsonConvert.SerializeObject(telemetryDataPoint);
            var message = new Message(Encoding.ASCII.GetBytes(messageString));
            lock (thisLock)
            {
                queue.Enqueue(message);
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
                    await SendMessagesInternalAsync(this, handler);
                };
                task = Task.Factory.StartNew(action);
                return true;
            }
            else return false;
        }

        private static async Task SendMessagesInternalAsync(IotHubClient client, Windows.Foundation.AsyncActionCompletedHandler handler)
        {
            System.Collections.Generic.Queue<Message> tmp;
            lock (client.thisLock)
            {
                tmp = new System.Collections.Generic.Queue<Message>(client.queue);
                client.msgCount = client.queue.Count;
                client.queue.Clear();
            }
            var auth = new DeviceAuthenticationWithRegistrySymmetricKey(AccessData.Access.DeviceID, AccessData.Access.DeviceKey);
            DeviceClient _deviceClient = DeviceClient.Create(AccessData.Access.IoTHubUri, auth, TransportType.Http1);
            Windows.Foundation.IAsyncAction ac = _deviceClient.SendEventBatchAsync(tmp);
            ac.Completed = handler;
            await ac;                
        }
    }
}
