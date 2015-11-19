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

        static DeviceClient _deviceClient;
        System.Collections.Generic.Queue<Message> queue;
        Task task = null;

        public IotHubClient()
        {
            var auth = new DeviceAuthenticationWithRegistrySymmetricKey(AccessData.Access.DeviceID, AccessData.Access.DeviceKey);
            _deviceClient = DeviceClient.Create(AccessData.Access.IoTHubUri, auth, TransportType.Http1);
            queue = new System.Collections.Generic.Queue<Message>();
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

        public void SendMessagesAsync(Windows.Foundation.AsyncActionCompletedHandler handler)
        {
            Func<Task> action = async () =>
            {
                await SendMessagesInternalAsync(this, handler);
            };
            task = Task.Factory.StartNew(action);
        }

        private static async Task SendMessagesInternalAsync(IotHubClient client, Windows.Foundation.AsyncActionCompletedHandler handler)
        {
            System.Collections.Generic.Queue<Message> tmp;
            lock (client.thisLock)
            {
                tmp = new System.Collections.Generic.Queue<Message>(client.queue);
                client.queue.Clear();
            }
            Windows.Foundation.IAsyncAction ac = _deviceClient.SendEventBatchAsync(tmp);
            ac.Completed += handler;
            await ac;                
        }
    }
}
