using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.Devices.Client;
using Newtonsoft.Json;

namespace IoTHubLibrary
{
    public delegate void MsgHandler(Object o, String s);

    public sealed class IotHubClient
    {
        static DeviceClient _deviceClient;   

        Task task = null;

        public IotHubClient()
        {
            var auth = new DeviceAuthenticationWithRegistrySymmetricKey(AccessData.Access.DeviceID, AccessData.Access.DeviceKey);
            _deviceClient = DeviceClient.Create(AccessData.Access.IoTHubUri, auth, TransportType.Http1);
        }

        public bool SendDeviceToCloudMessagesAsync(int msg, int cc, int asdf, int peak, System.UInt64 threshold, System.UInt64 volume)
        {
            if (task != null && (
                task.Status == TaskStatus.Running || 
                task.Status == TaskStatus.WaitingForActivation || 
                task.Status == TaskStatus.WaitingToRun ||
                task.Status == TaskStatus.Created)) return true;
            Func<Task> action = async () =>
            {
                await SendDeviceToCloudMessagesInternalAsync(msg, cc, asdf, peak, threshold, volume);
            };
            task = Task.Factory.StartNew(action);
            return false;
        }

        private static async Task SendDeviceToCloudMessagesInternalAsync(int msg, int cc, int asdf, int peak, System.UInt64 threshold, System.UInt64 volume)
        {
            var telemetryDataPoint = new
            {
                DeviceId = AccessData.Access.DeviceID,
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

            await _deviceClient.SendEventAsync(message);
        }
    }
}
