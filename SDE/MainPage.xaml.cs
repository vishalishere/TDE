﻿#define _PI2_1

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

namespace SDE
{
    public sealed partial class MainPage : Page
    {
        WiFiAdapter wifi = null;
#if _DUMMY
        private DispatcherTimer dummyTimer = null;
#else
        private WASAPIEngine engine;
#endif
        private LibIoTHubDebug.IotHubClient client = new LibIoTHubDebug.IotHubClient();

        private DispatcherTimer startTimer = new DispatcherTimer();
        private DispatcherTimer sendTimer = new DispatcherTimer();
        private DispatcherTimer testTimer = new DispatcherTimer();

        private uint bufferingCount = 0;
        private uint startCounter = 10;
        private uint sampleCount = 0;
        private uint messageCount = 0;
        private uint count = 0;

        private bool reboot = false;

        public MainPage()
        {
            this.InitializeComponent();

            testTimer.Interval = new TimeSpan(0, 3, 0);
            testTimer.Tick += TestSend;
            testTimer.Start();

            sendTimer.Interval = new TimeSpan(0, 0, 10); 
            sendTimer.Tick += Send;
             
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
                    if (t == HeartBeatType.DATA || messageCount == 10)
                    {
                        messageCount = 0;
                        client.AddMessage(t, i0, i1, i2, i4, i5);
                        label0.Text = "Message added";
                    }
                    else
                    {
                        messageCount++;
                        label0.Text = messageCount.ToString();
                    }
                }
                catch (Exception ex)
                {
                    Error("1: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
                }
            });
        }

        private void UpdateUI(HeartBeatType t, int i0, int i1, int i2, int i3, ulong i4, ulong i5, uint i6)
        {
            try
            {
                SetLabels(t);
                text1.Text = client.HeartBeatText(t);
                text2.Text =(count++).ToString();

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
                        break;
                    case HeartBeatType.SILENCE:
                        text8.Text = MemoryManager.AppMemoryUsage.ToString();
                        bufferingCount = 0;
                        break;
                    case HeartBeatType.BUFFERING:
                        bufferingCount++;
                        break;
                    case HeartBeatType.DEVICE_ERROR:
                        ResetEngine(10, "ERROR");
                        return;
                    case HeartBeatType.NODEVICE:
                        ResetEngine(10, "REBOOTING", true);
                        return;
                }

                if (bufferingCount == 60)
                {
                    ResetEngine(10, "REBOOTING", true);
                    return;
                }
                if (bufferingCount >= 5 && bufferingCount % 5 == 0)
                {
                    ResetEngine(10, "RESET");
                    return;
                }
                
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
            }
            catch (Exception ex)
            {
                Error("2: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
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
                    AudioDeviceStatus();
                    text2.Text = startCounter.ToString();
                    startCounter--;
                }
                else if (startCounter == 0)
                {
                    if (reboot)
                    {
                        LibRPi.HelperClass hc = new LibRPi.HelperClass();
                        hc.Reboot();
                    }
                    else
                    {
                        text1.Text = "STARTED";
                        exception.FontSize = 14;
                        exception.Text = "";    
                        startTimer.Stop();

#if _DUMMY
                        dummyTimer = new DispatcherTimer();
                        dummyTimer.Interval = new TimeSpan(0, 0, 1); // 1 s
                        dummyTimer.Tick += (object ss, object ee) =>
                        {
                            ThreadDelegate(HeartBeatType.DATA, 5, 5, 5, 0, 0, MemoryManager.AppMemoryUsage, 44100, 0);
                        };
                        dummyTimer.Start();
#else
                        engine = new WASAPIEngine();
#if _WIN64
                        TDEParameters param = new TDEParameters(1, 0, 0, 0, 3);
#else
#if _PI2_1
                        TDEParameters param = new TDEParameters(2, 0, 1, 0, 0, 44100, 300, 50, 1000, 32000, -60, false, "");
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
                Error("3: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
            }
        }

        private void TestSend(object sender, object e)
        {
            if (client.Messages() > 0)
            {
                ResetEngine(120, "SENDING");
                sendTimer.Start();
                testTimer.Stop();
            }
        }

        private async void Send(object sender, object e)
        {
            sendTimer.Stop();
            var access = await WiFiAdapter.RequestAccessAsync();
            if (access == WiFiAccessStatus.Allowed)
            {
                var wifiDevices = await DeviceInformation.FindAllAsync(WiFiAdapter.GetDeviceSelector());
                if (wifiDevices?.Count > 0)
                {
                    wifi = await WiFiAdapter.FromIdAsync(wifiDevices[0].Id);
                    await wifi.ScanAsync();

                    foreach (var network in wifi.NetworkReport.AvailableNetworks)
                    {
                        if (network.Ssid == Access.Ssid)
                        {
                            var passwordCredential = new PasswordCredential();
                            passwordCredential.Password = Access.Wifi_Password;
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
                                                    exception.Text = client.Sent() + " Messages sent";
                                                    if (startCounter > 2) startCounter = 2;
                                                    break;
                                                case AsyncStatus.Canceled: exception.Text = "Canceled " + asyncInfo.ErrorCode; break;
                                                case AsyncStatus.Error:
                                                    exception.Text = "0: Error: " + asyncInfo.ErrorCode;
                                                    label0.Text = "Error";
                                                    if (startCounter > 20) startCounter = 20;
                                                    break;
                                            }
                                            wifi.Disconnect();
                                            wifi = null;
                                            testTimer.Start();
                                        });
                                    };
                                    client.SendMessagesAsync(handler);
                                    label0.Text = "Sending messages ...";
                                }
                                catch (Exception ex)
                                {
                                    Error("4: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
                                }
                            }
                            else
                            {
                                exception.Text = result.ConnectionStatus.ToString();
                                if (startCounter > 2) startCounter = 2;
                                testTimer.Start();
                            }
                            return;
                        }
                    } 
                    exception.Text = Access.Ssid + " not found.";
                    if (startCounter > 2) startCounter = 2;
                    testTimer.Start();
                }
                else
                {
                    exception.Text = "No wifi adapter found";
                    ResetEngine(10, "REBOOTING", true);
                }
            }
            else
            {
                exception.Text = "Wifi access denied";
                ResetEngine(10, "REBOOTING", true);
            }
        }

        private void ResetEngine(uint counter, string str, bool boot = false)
        {
#if _DUMMY
            if (dummyTimer != null) dummyTimer.Stop();
            dummyTimer = null;
#else
            if (engine != null) engine.Finish();
            engine = null;            
#endif
            EmptyTexts(str);
            reboot = boot;
            startCounter = counter;
            startTimer.Start();
            GC.Collect();
        }

        private void Error(string err)
        {
            exception.Text = err;
            label0.Text = label1.Text = label2.Text = label3.Text = label4.Text = label5.Text = label6.Text = label7.Text = label8.Text = label9.Text = "";
            text1.Text = text2.Text = text3.Text = text4.Text = text5.Text = text6.Text = text7.Text = text8.Text = text9.Text = "";
            canvas.Children.Clear();
        }

        private void EmptyTexts(string status)
        {
            text1.Text = status;
            text2.Text = text3.Text = text4.Text = text5.Text = text6.Text = text7.Text = text8.Text = text9.Text = "";
            exception.Text = "";
            canvas.Children.Clear();
        }

        private void SetLabels(HeartBeatType t)
        {
            label1.Text = "STATUS";
            switch (t)
            {
                case HeartBeatType.INVALID:
                case HeartBeatType.DEVICE_ERROR:
                case HeartBeatType.NODEVICE:
                    {
                        label2.Text = "";
                        label3.Text = "";
                        label4.Text = "";
                        label5.Text = "";
                        label6.Text = "";
                        label7.Text = "";
                        label8.Text = "";
                        label9.Text = "";
                        break;
                    };
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
            exception.FontSize = 9;
            exception.Text = s;
        }
    }
}
