using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.Devices.Client;
using Newtonsoft.Json;
using Windows.Foundation;

namespace IoTHubLibrary
{
    public delegate void MsgHandler(Object o, String s);

    public sealed class IotHubClient
    { 
        static DeviceClient _deviceClient;
        static string iotHubUri = "AudioTestHub.azure-devices.net";
        static string deviceKey ="ePeF5AXJRqDCoWXBniAXJh4TInVDy8s+59JRQIBRPFw=";

        public IotHubClient()
        {
            var auth = new DeviceAuthenticationWithRegistrySymmetricKey("myFirstDevice", deviceKey);
            _deviceClient = DeviceClient.Create(iotHubUri, auth, TransportType.Http1);
        }

        public IAsyncAction SendDeviceToCloudMessagesAsync(int CC, int ASDF, int PEAK, System.UInt64 volume)
        {
            Func<Task> action = async () =>
            {
                await SendDeviceToCloudMessagesInternalAsync(CC, ASDF, PEAK, volume);
            };
            return action().AsAsyncAction();
        }


        public async void ReceiveAsync(object o, MsgHandler m)
        {
            await _deviceClient.OpenAsync();
            Message x = await _deviceClient.ReceiveAsync();
            if (x != null)
                m(o, x.ToString());
            else
                m(o, "No Messages");
        }

        private static async Task SendDeviceToCloudMessagesInternalAsync(int CC, int ASDF, int PEAK, System.UInt64 volume)
        {
            var telemetryDataPoint = new
            {
                DeviceId = "myFirstDevice",
                Direction_CC = CC,
                Direction_ADSF = ASDF,
                Direction_PEAK = PEAK,
                Volume = volume
            };

            var messageString = JsonConvert.SerializeObject(telemetryDataPoint);
            var message = new Message(Encoding.ASCII.GetBytes(messageString));

            await _deviceClient.SendEventAsync(message);
        }
    }
}
