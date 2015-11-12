using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.Devices.Client;
using Newtonsoft.Json;
using System.Threading;
using Windows.Foundation;

namespace IoTHubLibrary
{
    public sealed class IotHubClient
    {

        static DeviceClient _deviceClient;
        static string iotHubUri = "skynet1.azure-devices.net";
        static string deviceKey = "qj8Hjeyc/0wpQk3ffCVgqOEBA9xThsWYnWshOxibyGE=";

        public IotHubClient()
        {
            var auth = new DeviceAuthenticationWithRegistrySymmetricKey("myFirstDevice", deviceKey);
            _deviceClient = DeviceClient.Create(iotHubUri, auth, TransportType.Http1);
        }

        public IAsyncAction SendDeviceToCloudMessagesAsync(double direction, double power)
        {
            Func<Task> action = async () =>
            {
                await SendDeviceToCloudMessagesInternalAsync(direction, power);
            };

            return action().AsAsyncAction();
        }

        private static async Task SendDeviceToCloudMessagesInternalAsync(double direction, double power)
        {
            double avgWindSpeed = 10; // m/s
            Random rand = new Random();

            double currentWindSpeed = avgWindSpeed + rand.NextDouble() * 4 - 2;

                var telemetryDataPoint = new
                {
                    DeviceId = "myFirstDevice",
                    Direction = direction,
                    Power = power,
                };

                var messageString = JsonConvert.SerializeObject(telemetryDataPoint);
                var message = new Message(Encoding.ASCII.GetBytes(messageString));

                await _deviceClient.SendEventAsync(message);
        }
    }
}
