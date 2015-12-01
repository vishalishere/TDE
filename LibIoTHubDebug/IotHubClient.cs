using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.Devices.Client;
using Newtonsoft.Json;
using Windows.Storage.Streams;
using Windows.Storage;
using Windows.Foundation;

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
            public ulong beat;
            
            public void Write(DataWriter writer)
            {
                writer.WriteInt32((int)t);
                writer.WriteDateTime(time);
                writer.WriteInt32(cc);
                writer.WriteInt32(asdf);
                writer.WriteInt32(peak);
                writer.WriteUInt64(threshold);
                writer.WriteUInt64(volume);
                writer.WriteUInt64(beat);          
            }
            
            public void Read(DataReader reader)
            {
                t = (LibAudio.HeartBeatType) reader.ReadInt32();
                time = reader.ReadDateTime().DateTime;
                cc = reader.ReadInt32();
                asdf = reader.ReadInt32();
                peak = reader.ReadInt32();
                threshold = reader.ReadUInt64();
                volume = reader.ReadUInt64();
                beat = reader.ReadUInt64();           
            }
        } 

        private int MAX_MESSAGES = 10000;

        private object thisLock = new object();
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

        public IAsyncAction SaveQueueAsync()
        {
            Func<Task> action = async () =>
            {
                await SaveQueueInternalAsync(this);
            };
            return action().AsAsyncAction();
        }

        public IAsyncAction LoadQueueAsync()
        {
            Func<Task> action = async () =>
            {
                await LoadQueueInternalAsync(this);
            };
            return action().AsAsyncAction();
        }

        private static async Task SaveQueueInternalAsync(IotHubClient client)
        {
            try
            {
                System.Collections.Generic.Queue<DataPoint> tmp = null;
                lock (client.thisLock)
                {
                    tmp = new System.Collections.Generic.Queue<DataPoint>(client.queue);
                }
                StorageFolder storageFolder = ApplicationData.Current.LocalFolder;
                StorageFile file = await storageFolder.CreateFileAsync("queue.txt", CreationCollisionOption.ReplaceExisting);
                var stream = await file.OpenAsync(Windows.Storage.FileAccessMode.ReadWrite);

                using (var outputStream = stream.GetOutputStreamAt(0))
                {
                    using (var dataWriter = new DataWriter(outputStream))
                    {
                        dataWriter.WriteInt32(tmp.Count);
                        foreach (var item in tmp)
                        {
                            item.Write(dataWriter);
                        }
                        await dataWriter.StoreAsync();
                        await outputStream.FlushAsync();

                    }
                }
                stream.Dispose();
            }
            catch (Exception) {}
        }

        private static async Task LoadQueueInternalAsync(IotHubClient client)
        {
            System.Collections.Generic.Queue<DataPoint> tmp = new System.Collections.Generic.Queue<DataPoint>();
            try
            {
                StorageFolder storageFolder = ApplicationData.Current.LocalFolder;
                StorageFile file = await storageFolder.GetFileAsync("queue.txt");

                var stream = await file.OpenAsync(FileAccessMode.ReadWrite);
                ulong size = stream.Size;

                using (var inputStream = stream.GetInputStreamAt(0))
                {
                    using (var dataReader = new DataReader(inputStream))
                    {
                        uint numBytesLoaded = await dataReader.LoadAsync((uint)size);
                        int count = dataReader.ReadInt32();
                        for (int i=0; i< count; i++)
                        {
                            DataPoint p = new DataPoint();
                            p.Read(dataReader);
                            tmp.Enqueue(p);
                        }
                    }

                }
                stream.Dispose();
                lock (client.thisLock)
                {
                    client.queue.Clear();
                    client.queue = null;
                    client.queue = new System.Collections.Generic.Queue<DataPoint>(tmp);
                }
            }
            catch (Exception) {}
        }

        public void AddMessage(LibAudio.HeartBeatType t, int cc, int asdf, int peak, ulong threshold, ulong volume, ulong beat)
        {
            DataPoint p = new DataPoint();
            p.time = DateTime.UtcNow;
            p.t = t;
            p.cc = cc;
            p.asdf = asdf;
            p.peak = peak;
            p.threshold = threshold;
            p.volume = volume;
            p.beat = beat;

            lock (thisLock)
            {
                if (t == LibAudio.HeartBeatType.DATA)
                {
                    if (queue.Count == MAX_MESSAGES) queue.Dequeue();
                    queue.Enqueue(p);
                }
                else
                {
                    if (queue.Count < MAX_MESSAGES)
                    {
                        queue.Enqueue(p);
                    }
                }
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
                                VOL = p.volume,
                                BEAT = p.beat
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
