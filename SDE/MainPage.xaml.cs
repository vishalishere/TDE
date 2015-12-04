#define _PI2_1

using System;
using Windows.System;
using Windows.UI;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Shapes;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Windows.Devices.WiFi;
using Windows.Devices.Enumeration;
using Windows.Foundation;
using Windows.Security.Credentials;

using AccessData;
using LibAudio;
using Windows.Storage;
using System.Threading.Tasks;
using Windows.Storage.Streams;
using Windows.System.Diagnostics;
using System.Collections.Generic;

namespace SDE
{
    public sealed partial class MainPage : Page
    {
        private object thisLock = new object();
        WiFiAdapter wifi = null;
#if _DUMMY
        private DispatcherTimer dummyTimer = null;
#else
        private WASAPIEngine engine;
#endif
        private LibIoTHubDebug.IotHubClient client = new LibIoTHubDebug.IotHubClient();

        private DispatcherTimer startTimer = new DispatcherTimer();
        private DispatcherTimer sendDelayTimer = new DispatcherTimer();
        private DispatcherTimer connectionTimer = new DispatcherTimer();
        private DispatcherTimer statusTimer = new DispatcherTimer();

        private uint bufferingCount = 0;
        private uint startCounter = 10;
        private uint sampleCount = 0;
        private uint networkError = 0;
        private uint beat = 0;
        private string status = "";

        private bool reboot = false;
        private bool rebootPending = false;

        public MainPage()
        {
            this.InitializeComponent();

            connectionTimer.Interval = new TimeSpan(0, 3, 0);
            connectionTimer.Tick += TestSend;
            connectionTimer.Start();

            sendDelayTimer.Interval = new TimeSpan(0, 0, 10); 
            sendDelayTimer.Tick += Send;

            statusTimer.Interval = new TimeSpan(0, 0, 1);
            statusTimer.Tick += (object sender, object e) => {
                lock(thisLock)
                {
                    label0.Text = status;
                    AppStatus.Text = bufferingCount.ToString() + " " + networkError.ToString() + " " + rebootPending.ToString() + " " + reboot.ToString() + " " + connectionTimer.IsEnabled.ToString();
                }
            };

            startTimer.Interval = new TimeSpan(0, 0, 1);
            startTimer.Tick += Tick;
            startTimer.Start();

            SetLabels(HeartBeatType.INVALID);
            text1.Text = "STARTING";
        }

        ~MainPage()
        {
#if _DUMMY
#else
            engine.Finish();
#endif
        }

        private void ThreadDelegate(HeartBeatType t, int i0, int i1, int i2, int i3, ulong i4, ulong i5, uint i6)
        {
            var ignored1 = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
            {
                UpdateUI(t, i0, i1, i2, i3, i4, i5, i6);
            });

            var ignored2 = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
            {
                try
                {
                    int ii1 = 0, ii2 = 0, ii3 = 0;
                    if (t==HeartBeatType.SILENCE)
                    {
                        IReadOnlyList<ProcessDiagnosticInfo> list = ProcessDiagnosticInfo.GetForProcesses();
                        if ( list?.Count > 0)
                        {
                            ii1 = (int)list[0].CpuUsage.GetReport().KernelTime.TotalSeconds;
                            ii2 = (int)list[0].CpuUsage.GetReport().UserTime.TotalSeconds;
                            ii3 = (int)list[0].MemoryUsage.GetReport().PeakVirtualMemorySizeInBytes;
                        }
                        client.AddMessage(t, ii1, ii2, ii3, MemoryManager.AppMemoryUsage, MemoryManager.GetProcessMemoryReport().TotalWorkingSetUsage, beat);
                    }
                    else client.AddMessage(t, i0, i1, i2, i4, i5, beat); 
                }
                catch (Exception ex)
                {
                    Error("ThreadDelegate: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
                }
            });
        }

        private void UpdateUI(HeartBeatType t, int i0, int i1, int i2, int i3, ulong i4, ulong i5, uint i6)
        {
            try
            {
                SetLabels(t);
                text1.Text = client.HeartBeatText(t);
                text2.Text =(beat++).ToString();

                text3.Text = t == HeartBeatType.SILENCE ? "" : i0.ToString();
                text4.Text = t == HeartBeatType.SILENCE ? "" : i1.ToString();
                text5.Text = t == HeartBeatType.SILENCE ? "" : i2.ToString();
                text6.Text = (t == HeartBeatType.SILENCE || t == HeartBeatType.BUFFERING) ? "" : i3.ToString();
                text7.Text = t == HeartBeatType.SILENCE ? "" : i4.ToString();
                text8.Text = t == HeartBeatType.SILENCE ? "" : i5.ToString();          

                switch (t)
                {
                    case HeartBeatType.DATA:
                        bufferingCount = 0;
                        rebootPending = false;
                        break;
                    case HeartBeatType.SILENCE:
                        text8.Text = MemoryManager.AppMemoryUsage.ToString();
                        rebootPending = false;
                        bufferingCount = 0;
                        break;
                    case HeartBeatType.BUFFERING:
                        bufferingCount++;
                        break;
                    case HeartBeatType.DEVICE_ERROR:
                        rebootPending = true;
                        ResetEngine(10, "ERROR");
                        break;
                    case HeartBeatType.NODEVICE:
                        rebootPending = true;
                        break;
                }

                if (bufferingCount == 60) { rebootPending = true; }
                else if (bufferingCount >= 10 && bufferingCount % 10 == 0) { ResetEngine(10, "RESET"); }
                
                if (t != HeartBeatType.BUFFERING) canvas.Children.Clear();
                if (t == HeartBeatType.DATA)
                {
                    ulong vol = i4 / 20;
                    if (vol > 800) vol = 800;

                    SoundDirection(i6, 0.4, -1 * i0, 800, (int)vol, new SolidColorBrush(Colors.Red), 4);
                    SoundDirection(i6, 0.4, -1 * i1, 800, (int)vol, new SolidColorBrush(Colors.Green), 2);
                    SoundDirection(i6, 0.4, -1 * i2, 800, (int)vol, new SolidColorBrush(Colors.Blue), 1);
                    sampleCount++;       
                }
                text9.Text = sampleCount.ToString();
                AppStatus.Text = bufferingCount.ToString() + " " + networkError.ToString() + " " + rebootPending.ToString() + " " 
                    + connectionTimer.IsEnabled.ToString() + " " + client.Messages().ToString();
            }
            catch (Exception ex)
            {
                Error("UpdateUI: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
            }
        }

        void SoundDirection(double rate, double dist, int delay, int x, int length, SolidColorBrush color, int thickness)
        {
            double val = (((double)delay / rate) * 343.2) / dist;
            if (val > 1) val = 1;
            if (val < -1) val = -1;
            double ang = Math.Asin(val);

            Line line = new Line();
            line.X1 = x;
            line.Y1 = 0;
            line.X2 = line.X1 + Math.Sin(ang) * length;
            line.Y2 = line.Y1 + Math.Cos(ang) * length;
            line.Stroke = color;
            line.StrokeThickness = thickness;
            canvas.Children.Add(line);
        }

        private async void Tick(object sender, object e)
        {
            try
            {
                if (startCounter == 10)
                {
                    startCounter--;
                    startTimer.Stop();
                    if (beat == 0)
                    {
                        reboot = await ReadStatus();
                        if (reboot) await WriteStatus(false);
                        await client.LoadQueueAsync();
                    }
                    AudioDeviceStatus();
                    startTimer.Start();
                    text2.Text = startCounter.ToString();     
                }
                else if (startCounter == 0)
                {
                    if (reboot)
                    {
                        startTimer.Stop();
                        await client.SaveQueueAsync();
                        reboot = false;
                        LibRPi.HelperClass hc = new LibRPi.HelperClass();
                        hc.Reboot();
                    }
                    else
                    {
                        text1.Text = "STARTED";
                        errorText.Text = "";    
                        startTimer.Stop();

#if _DUMMY
                        dummyTimer = new DispatcherTimer();
                        dummyTimer.Interval = new TimeSpan(0, 0, 1); // 1 s
                        dummyTimer.Tick += (object ss, object ee) =>
                        {
                            Random r = new Random();
                            int cc = (r.Next() % 80) - 40;
                            ulong len = (ulong)(r.Next() % 32000);
                            int diff = r.Next() % 3;
                            ThreadDelegate(HeartBeatType.DATA, cc, cc + diff, cc - diff, 0, len, MemoryManager.AppMemoryUsage, 44100);
                        };
                        dummyTimer.Start();
#else
                        engine = new WASAPIEngine();
#if _WIN64
                        TDEParameters param = new TDEParameters(1, 0, 0, 0, 1);
#else
#if _PI2_1
                        TDEParameters param = new TDEParameters(2, 0, 1, 0, 0, 44100, 300, 50, 500, 32000, -60, false, "");
#elif _PI2_2
                        TDEParameters param = new TDEParameters(1, 0, 0, 3, 0, 44100, 300, 50, 2000, 32000, -60, false, "");
#else
                        TDEParameters param = new TDEParameters(2, 0, 1, 0, 0, 44100, 300, 50, 2000, 32000, -60, false, "");
#endif
#endif
                        await engine.InitializeAsync(ThreadDelegate, param);
#endif
                    }
                }
                else
                {
                    text2.Text = startCounter.ToString();
                    startCounter--;
                }
            }
            catch (Exception ex)
            {
                Error("Tick: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
            }
        }

        private void TestSend(object sender, object e)
        {
            if (client.Messages() > 0)
            {
                ResetEngine(120, "SENDING");
                sendDelayTimer.Start();
                connectionTimer.Stop();
            }
        }

        private void SetStatus(string str)
        {
            lock(thisLock)
            {
                status = str;
            }
        }

        private async void Send(object sender, object e)
        {
            sendDelayTimer.Stop();
            statusTimer.Start();
            var access = await WiFiAdapter.RequestAccessAsync();
            if (access == WiFiAccessStatus.Allowed)
            {
                var wifiDevices = await DeviceInformation.FindAllAsync(WiFiAdapter.GetDeviceSelector());
                if (wifiDevices?.Count > 0)
                {
                    wifi = await WiFiAdapter.FromIdAsync(wifiDevices[0].Id);

                    await WriteStatus(true);

                    SetStatus("ScanAsync");
                    IAsyncAction a = wifi?.ScanAsync();
                    await a;

                    await WriteStatus(false);

                    if (a.Status == AsyncStatus.Completed && wifi?.NetworkReport?.AvailableNetworks?.Count > 0)
                    {
                        foreach (var network in wifi.NetworkReport.AvailableNetworks)
                        {
                            bool found = false;
                            uint wlan = 0;
                            for (uint i = 0; i < Access.Networks; i++)
                            {
                                if (network.Ssid == Access.SSID(i))
                                {
                                    wlan = i;   
                                    found = true;
                                    break;
                                }
                            }
                            if (found)
                            {
                                var passwordCredential = new PasswordCredential();
                                passwordCredential.Password = Access.WIFI_Password(wlan);

                                SetStatus("ConnectAsync");
                                var result = await wifi.ConnectAsync(network, WiFiReconnectionKind.Automatic, passwordCredential);

                                if (result.ConnectionStatus.Equals(WiFiConnectionStatus.Success))
                                {
                                    try
                                    {
                                        AsyncActionCompletedHandler handler = (IAsyncAction asyncInfo, AsyncStatus asyncStatus) =>
                                        {
                                            var ignored = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
                                            {
                                                switch (asyncStatus)
                                                {
                                                    case AsyncStatus.Completed:
                                                        errorText.Text = client.Sent() + " Messages sent";
                                                        label0.Text = "";
                                                        networkError = 0;
                                                        if (startCounter > 2) startCounter = 2;
                                                        break;
                                                    case AsyncStatus.Canceled: errorText.Text = "Canceled " + asyncInfo.ErrorCode; break;
                                                    case AsyncStatus.Error:
                                                        errorText.Text = "0: Error: " + asyncInfo.ErrorCode;
                                                        SetStatus("Error");
                                                        if (startCounter > 20) startCounter = 20;
                                                        break;
                                                }
                                                if (rebootPending) reboot = true;
                                                wifi.Disconnect();
                                                wifi = null;
                                                statusTimer.Stop();
                                            });
                                        };   
                                        connectionTimer.Start();
                                        client.SendMessagesAsync(handler);
                                        SetStatus("Sending messages ...");
                                    }
                                    catch (Exception ex)
                                    {
                                        statusTimer.Stop();
                                        Error("Send: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
                                        connectionTimer.Start();
                                    }
                                }
                                else NoConnection(false, 9, result.ConnectionStatus.ToString());
                                return;
                            }
                        }
                        NoConnection(rebootPending  || (++networkError == 5), 9, Access.Ssid + " not found.");
                    }
                    else NoConnection(rebootPending || (++networkError == 5), 9, "No wifi networks found " + a.Status.ToString());
                }   
                else NoConnection(true, 15, "No wifi adapter found - rebooting");                    
            }
            else NoConnection(true, 15, "Wifi access denied - rebooting" + access.ToString());
        }

        private void NoConnection(bool boot, uint counter, string str)
        {
            if (!reboot) reboot = boot;
            errorText.Text = str;
            statusTimer.Stop();
            if (startCounter > counter) startCounter = counter;
            if (!reboot) connectionTimer.Start();
        }

        private void ResetEngine(uint counter, string str)
        {
            if (startTimer.IsEnabled) startTimer.Stop();
#if _DUMMY
            if (dummyTimer != null) dummyTimer.Stop();
            dummyTimer = null;
#else
            if (engine != null) engine.Finish();
            engine = null;            
#endif
            EmptyTexts(str);
            startCounter = counter;
            startTimer.Start();
            GC.Collect();
        }

        private void Error(string err)
        {
            errorText.Text = err;
            label0.Text = label1.Text = label2.Text = label3.Text = label4.Text = label5.Text = label6.Text = label7.Text = label8.Text = label9.Text = "";
            EmptyTexts("");
        }

        private void EmptyTexts(string status)
        {
            text1.Text = status;
            text2.Text = text3.Text = text4.Text = text5.Text = text6.Text = text7.Text = text8.Text = text9.Text = "";
            canvas.Children.Clear();
        }

        private void SetLabels(HeartBeatType t)
        {
            label1.Text = "STATUS";
            switch (t)
            {
                case HeartBeatType.DEVICE_ERROR:
                case HeartBeatType.NODEVICE:
                    {
                        label2.Text = "BEAT";
                        label3.Text = "";
                        label4.Text = "";
                        label5.Text = "";
                        label6.Text = "";
                        label7.Text = "";
                        label8.Text = "";
                        label9.Text = "";
                        break;
                    };
                case HeartBeatType.INVALID:
                case HeartBeatType.DATA:
                    {
                        label2.Text = "BEAT";
                        label3.Text = "CC";
                        label4.Text = "ASDF";
                        label5.Text = "PEAK";
                        label6.Text = "ALIGN";
                        label7.Text = "THRESHOLD";
                        label8.Text = "VOLUME";
                        label9.Text = "SAMPLES";
                        break;
                    };
                case HeartBeatType.SILENCE:
                    {
                        label2.Text = "BEAT";
                        label3.Text = "";
                        label4.Text = "";
                        label5.Text = "";
                        label6.Text = "";
                        label7.Text = "";
                        label8.Text = "MEMORY";
                        label9.Text = "SAMPLES";
                        break;
                    };
                case HeartBeatType.BUFFERING:
                    {
                        label2.Text = "BEAT";
                        label3.Text = "PAC";
                        label4.Text = "DIS";
                        label5.Text = "REM";
                        label6.Text = "";
                        label7.Text = "TIMESTAMP 0";
                        label8.Text = "TIMESTAMP 1";
                        label9.Text = "SAMPLES";
                        break;
                    }
            }
        }

        private async Task WriteStatus(bool state)
        {
            StorageFolder storageFolder = ApplicationData.Current.LocalFolder;
            StorageFile file = await storageFolder.CreateFileAsync("status.txt", CreationCollisionOption.ReplaceExisting);
            var stream = await file.OpenAsync(Windows.Storage.FileAccessMode.ReadWrite);

            using (var outputStream = stream.GetOutputStreamAt(0))
            {
                using (var dataWriter = new DataWriter(outputStream))
                {
                    dataWriter.WriteBoolean(state);
                    await dataWriter.StoreAsync();
                    await outputStream.FlushAsync();

                }
            }
            stream.Dispose();
        }

        private async Task<bool> ReadStatus()
        {
            bool state = false;
            try
            {
                StorageFolder storageFolder = ApplicationData.Current.LocalFolder;
                StorageFile file = await storageFolder.GetFileAsync("status.txt");

                var stream = await file.OpenAsync(FileAccessMode.ReadWrite);
                ulong size = stream.Size;

                using (var inputStream = stream.GetInputStreamAt(0))
                {
                    using (var dataReader = new DataReader(inputStream))
                    {
                        uint numBytesLoaded = await dataReader.LoadAsync((uint)size);
                        state = dataReader.ReadBoolean();
                    }

                }
                stream.Dispose();
            }
            catch (Exception) {}
            return state;
        }

        private async void AudioDeviceStatus()
        {
            string s = "";
            var dis1 = await DeviceInformation.FindAllAsync(DeviceClass.AudioCapture);
            foreach (var item in dis1)
            {
                object o1, o2;
                System.Collections.Generic.IReadOnlyDictionary<string,object> p = item.Properties;
                p.TryGetValue("System.ItemNameDisplay", out o1);
                p.TryGetValue("System.Devices.InterfaceEnabled", out o2);
                s += (o1 != null ? o1.ToString() : "") + " " + (o2 != null ? o2.ToString() : "") + "\n";
            }
            errorText.Text = s;
        }
    }
}
