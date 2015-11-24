#define _PI2_1

using System;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Shapes;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using System.Threading.Tasks;

namespace SDE
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        private LibAudio.WASAPIEngine engine;
        private LibIoTHubDebug.IotHubClient client = new LibIoTHubDebug.IotHubClient();

        private DispatcherTimer startTimer = new DispatcherTimer();
        private DispatcherTimer sendTimer = new DispatcherTimer();
        private DispatcherTimer testTimer = new DispatcherTimer();

        private uint bufferingCount = 0;
        private uint startCounter = 10;
        private uint sampleCount = 0;
        private uint messageCount = 0;
        private uint errorCount = 0;
        private uint count = 0;

        private bool activeTask = false;
        private bool reboot = false;

        public MainPage()
        {
            this.InitializeComponent();

            testTimer.Interval = new TimeSpan(0, 1, 0); // 1 minute
            testTimer.Tick += TestSend;
            testTimer.Start();

            sendTimer.Interval = new TimeSpan(0, 0, 5); // 5 s
            sendTimer.Tick += Send;
             
            startTimer.Interval = new TimeSpan(0, 0, 1); // 1 s
            startTimer.Tick += Tick;
            startTimer.Start();

            SetLabels(LibAudio.HeartBeatType.INVALID);
            text1.Text = "STARTING";
        }

        ~MainPage()
        {
            engine.Finish();
        }

        private void ThreadDelegate(LibAudio.HeartBeatType t, int i0, int i1, int i2, int i3, ulong i4, ulong i5, uint i6, ulong i7)
        {
            var ignored1 = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
            {
                UpdateUI(t, i0, i1, i2, i3, i4, i5, i6, i7);
            });

            var ignored2 = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
            {
                try
                {
                    if (t == LibAudio.HeartBeatType.DATA || messageCount == 10)
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

        private void UpdateUI(LibAudio.HeartBeatType t, int i0, int i1, int i2, int i3, ulong i4, ulong i5, uint i6, ulong i7)
        {
            try
            {
                text2.Text =(count++).ToString();
                text3.Text = i0.ToString();
                text4.Text = i1.ToString();
                text5.Text = i2.ToString();
                text6.Text = i3.ToString();
                text7.Text = i4.ToString();
                text8.Text = i5.ToString();
                text0.Text = i7.ToString();

                SetLabels(t);
                text1.Text = client.HeartBeatText(t);

                switch (t)
                {
                    case LibAudio.HeartBeatType.DATA:
                    case LibAudio.HeartBeatType.SILENCE:
                        text8.Text = Windows.System.MemoryManager.AppMemoryUsage.ToString();
                        bufferingCount = 0;
                        break;
                    case LibAudio.HeartBeatType.BUFFERING:
                        bufferingCount++;
                        break;
                    case LibAudio.HeartBeatType.DEVICE_ERROR:
                        ResetEngine(10, "ERROR");
                        return;
                    case LibAudio.HeartBeatType.NODEVICE:
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
                
                if (t != LibAudio.HeartBeatType.BUFFERING) canvas.Children.Clear();
                if (t == LibAudio.HeartBeatType.DATA)
                {
                    ulong vol = i4 / 20;
                    if (vol > 800) vol = 800;

                    Direction(i6, 0.4, -1 * i0, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Red), 4);
                    Direction(i6, 0.4, -1 * i1, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Green), 2);
                    Direction(i6, 0.4, -1 * i2, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Blue), 1);
                    sampleCount++;       
                }
                text9.Text = sampleCount.ToString();
            }
            catch (Exception ex)
            {
                Error("2: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
            }
        }

        void Direction(double rate, double dist, int delay, int x, int length, SolidColorBrush color, int thickness)
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
                    DeviceStatus();
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
                        exception.Text = "";
                        exception.FontSize = 14;
                        startTimer.Stop();
                        engine = new LibAudio.WASAPIEngine();
#if _WIN64
                        LibAudio.TDEParameters param = new LibAudio.TDEParameters(1, 0, 0, 0, 3);
#else
#if _PI2_1
                        LibAudio.TDEParameters param = new LibAudio.TDEParameters(2, 0, 1, 0, 0, 44100, 300, 50, 2000, 32000, -60, false, "");
#elif _PI2_2
                        LibAudio.TDEParameters param = new LibAudio.TDEParameters(1, 0, 0, 3, 0, 44100, 300, 50, 2000, 32000, -60, false, "");
#else
                        LibAudio.TDEParameters param = new LibAudio.TDEParameters(2, 0, 1, 0, 0, 44100, 300, 50, 2000, 32000, -60, false, "");
#endif
#endif
                        await engine.InitializeAsync(ThreadDelegate, param);
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
            if (errorCount == 5) ResetEngine(10, "REBOOTING", true);
            if (client.Messages() > 0)
            {
                ResetEngine(60, "SENDING");
                sendTimer.Start();
            }
        }

        private void Send(object sender, object e)
        {
            sendTimer.Stop();
            try
            {
                Windows.Foundation.AsyncActionCompletedHandler handler = (Windows.Foundation.IAsyncAction asyncInfo, Windows.Foundation.AsyncStatus asyncStatus) =>
                {
                    var ignored = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
                    {
                        switch (asyncStatus)
                        {
                            case Windows.Foundation.AsyncStatus.Completed:
                                exception.Text = client.Sent() + " Messages sent";
                                errorCount = 0;
                                if (startCounter > 2) startCounter = 2;
                                break;
                            case Windows.Foundation.AsyncStatus.Canceled: exception.Text = "Canceled " + asyncInfo.ErrorCode; break;
                            case Windows.Foundation.AsyncStatus.Error:
                                exception.Text = "0: Error: " + asyncInfo.ErrorCode;
                                label0.Text = "Error";
                                errorCount++;
                                if (startCounter > 2) startCounter = 2;
                                break;
                        }
                        activeTask = false;
                    });
                };

                if (!activeTask)
                {
                    activeTask = client.SendMessagesAsync(handler);
                    label0.Text = activeTask ? "Sending messages ..." : "No messages pending";
                }
                else label0.Text = "Active task";

            }
            catch (Exception ex)
            {
                Error("4: " + ex.ToString() + " " + ex.Message + " " + ex.HResult);
            }
        }

        private void ResetEngine(uint counter, string str, bool boot = false)
        {
            if (engine != null) engine.Finish();
            engine = null;
            EmptyTexts(str);
            reboot = boot;
            startCounter = counter;
            startTimer.Start();
        }

        private void Error(string err)
        {
            exception.Text = err;

            label1.Text = "";
            label2.Text = "";
            label3.Text = "";
            label4.Text = "";
            label5.Text = "";
            label6.Text = "";
            label7.Text = "";
            label8.Text = "";
            label9.Text = "";
            label0.Text = "";

            text1.Text = "";
            text1.Text = "";
            text2.Text = "";
            text3.Text = "";
            text4.Text = "";
            text5.Text = "";
            text6.Text = "";
            text7.Text = "";
            text8.Text = "";
            text9.Text = "";

            canvas.Children.Clear();
        }

        private void EmptyTexts(string status)
        {
            text1.Text = status;
            text2.Text = "";
            text3.Text = "";
            text4.Text = "";
            text5.Text = "";
            text6.Text = "";
            text7.Text = "";
            text8.Text = "";
            text9.Text = "";
            exception.Text = "";
            canvas.Children.Clear();
        }

        private void SetLabels(LibAudio.HeartBeatType t)
        {
            label1.Text = "STATUS";

            switch (t)
            {
                case LibAudio.HeartBeatType.INVALID:
                case LibAudio.HeartBeatType.DEVICE_ERROR:
                case LibAudio.HeartBeatType.NODEVICE:
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
                case LibAudio.HeartBeatType.DATA:
                case LibAudio.HeartBeatType.SILENCE:
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
                case LibAudio.HeartBeatType.BUFFERING:
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

        async void DeviceStatus()
        {
            string s = "";
            var dis = await Windows.Devices.Enumeration.DeviceInformation.FindAllAsync();
            foreach (var item in dis)
            {
                object o1, o2;
                System.Collections.Generic.IReadOnlyDictionary<string,object> p = item.Properties;
                p.TryGetValue("System.ItemNameDisplay", out o1);
                p.TryGetValue("System.Devices.InterfaceEnabled", out o2);
                s += o1.ToString() + " " + o2.ToString() + "\n";
                //foreach (var prop in p)
                //{
                //    s += prop.Key != null ? prop.Key.ToString() + " : " : "";
                //    s += prop.Value != null ? prop.Value.ToString() + "\n" : "\n";
                //}
            }
            exception.FontSize = 10;
            exception.Text = s;
        }
    }
}
