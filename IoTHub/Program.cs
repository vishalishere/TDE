//#define CREATE_NEW_DEVICE

using System;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using Microsoft.ServiceBus.Messaging;

namespace IoTHub
{
    public sealed class Access
    {
        static string iotHubUri = "{XXX}";
        static string connectionString = "{XXX}";
        static string deviceId = "{XXX}";

        public static string IoTHubUri { get { return iotHubUri; } }
        public static string DeviceID { get { return deviceId; } }
        public static string ConnectionString { get { return connectionString; } }
    }

    class Program
    {
        static string path = @"c:\temp\audiodata.txt";
#if CREATE_NEW_DEVICE
        static RegistryManager registryManager;
#else
        static string iotHubD2cEndpoint = "messages/events";
        static EventHubClient eventHubClient;
#endif

        private static void SaveQueue(System.Collections.Generic.Queue<string> queue)
        {
            using (StreamWriter sw = File.AppendText(path))
            {
                while (queue.Count > 0)
                { 
                    sw.WriteLine(queue.Dequeue());
                }
            }
        }

        static void Main(string[] args)
        {
#if CREATE_NEW_DEVICE
            registryManager = RegistryManager.CreateFromConnectionString(Access.Access.ConnectionString);
            AddDeviceAsync().Wait();
            Console.ReadLine();
#else
            Console.WriteLine("Receive messages\n");

            if (!File.Exists(path))
            {
                using (StreamWriter sw = File.CreateText(path))
                {
                    sw.WriteLine(DateTime.UtcNow.ToString());
                }
            }

            eventHubClient = EventHubClient.CreateFromConnectionString(Access.ConnectionString, iotHubD2cEndpoint);

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
            System.Collections.Generic.Queue<string> queue = new System.Collections.Generic.Queue<string>();

            var eventHubReceiver = eventHubClient.GetDefaultConsumerGroup().CreateReceiver(partition, DateTime.Now);
            while (true)
            {
                EventData eventData = await eventHubReceiver.ReceiveAsync();
                if (eventData == null) continue;

                string data = Encoding.UTF8.GetString(eventData.GetBytes());
                queue.Enqueue(data);
                if (queue.Count > 100) SaveQueue(queue);
                Console.WriteLine(string.Format("Message: '{0}'", data));
            }
        }
#endif
    }
}
