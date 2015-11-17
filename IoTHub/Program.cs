﻿using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.Devices;
using Microsoft.Azure.Devices.Client;
using Microsoft.Azure.Devices.Common.Exceptions;
using Microsoft.ServiceBus.Messaging;

namespace IoTHub
{
    class Program
    {
        static RegistryManager registryManager;
        static string connectionString = "HostName=AudioTestHub.azure-devices.net;SharedAccessKeyName=iothubowner;SharedAccessKey=1PovEgY26ig4f2JOrffNPXfQ0cZHRUMOmAXMHqldlA4=";

        static string iotHubD2cEndpoint = "messages/events";
        static EventHubClient eventHubClient;

        static void Main(string[] args)
        {
            //registryManager = RegistryManager.CreateFromConnectionString(connectionString);
            //AddDeviceAsync().Wait();
            //Console.ReadLine();

            Console.WriteLine("Receive messages\n");
            eventHubClient = EventHubClient.CreateFromConnectionString(connectionString, iotHubD2cEndpoint);

            var d2cPartitions = eventHubClient.GetRuntimeInformation().PartitionIds;

            foreach (string partition in d2cPartitions)
            {
                ReceiveMessagesFromDeviceAsync(partition);
            }
            Console.ReadLine();
        }

        private async static Task ReceiveMessagesFromDeviceAsync(string partition)
        {
            var eventHubReceiver = eventHubClient.GetDefaultConsumerGroup().CreateReceiver(partition, DateTime.Now);
            while (true)
            {
                EventData eventData = await eventHubReceiver.ReceiveAsync();
                if (eventData == null) continue;

                string data = Encoding.UTF8.GetString(eventData.GetBytes());
                //Console.WriteLine(string.Format("Message received. Partition: {0} Data: '{1}'", partition, data));
                Console.WriteLine(string.Format("'{0}'", data));
            }
        }

        private async static Task AddDeviceAsync()
        {
            string deviceId = "myFirstDevice";
            Device device;
            try
            {
                device = await registryManager.AddDeviceAsync(new Device(deviceId));
            }
            catch (DeviceAlreadyExistsException)
            {
                device = await registryManager.GetDeviceAsync(deviceId);
            }
            Console.WriteLine("Generated device key: {0}", device.Authentication.SymmetricKey.PrimaryKey);
        }
    }
}
