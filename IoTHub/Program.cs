//#define CREATE_NEW_DEVICE

using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.Devices;
using Microsoft.Azure.Devices.Common.Exceptions;
using Microsoft.ServiceBus.Messaging;

namespace IoTHub
{
    class Program
    {
#if CREATE_NEW_DEVICE
        static RegistryManager registryManager;
#else
        static string iotHubD2cEndpoint = "messages/events";
        static EventHubClient eventHubClient;
#endif

        static void Main(string[] args)
        {
#if CREATE_NEW_DEVICE
            registryManager = RegistryManager.CreateFromConnectionString(Access.Access.ConnectionString);
            AddDeviceAsync().Wait();
            Console.ReadLine();
#else
            Console.WriteLine("Receive messages\n");
            eventHubClient = EventHubClient.CreateFromConnectionString(Access.Access.ConnectionString, iotHubD2cEndpoint);

            var d2cPartitions = eventHubClient.GetRuntimeInformation().PartitionIds;

            foreach (string partition in d2cPartitions)
            {
                Func<Task> action = async () =>
                {
                    await ReceiveMessagesFromDeviceAsync(partition);
                };
                Task.Factory.StartNew(action);
            }
            Console.ReadLine();
#endif
        }

#if CREATE_NEW_DEVICE
        private async static Task AddDeviceAsync()
        {
            Device device;
            try
            {
                device = await registryManager.AddDeviceAsync(new Device(Access.Access.DeviceID));
            }
            catch (DeviceAlreadyExistsException)
            {
                device = await registryManager.GetDeviceAsync(Access.Access.DeviceID);
            }
            Console.WriteLine("Generated device key: {0}", device.Authentication.SymmetricKey.PrimaryKey);
        }
#else
        private async static Task ReceiveMessagesFromDeviceAsync(string partition)
        {
            var eventHubReceiver = eventHubClient.GetDefaultConsumerGroup().CreateReceiver(partition, DateTime.Now);
            while (true)
            {
                EventData eventData = await eventHubReceiver.ReceiveAsync();
                if (eventData == null) continue;

                string data = Encoding.UTF8.GetString(eventData.GetBytes());
                Console.WriteLine(string.Format("Message: '{0}'", data));
            }
        }
#endif
    }
}
