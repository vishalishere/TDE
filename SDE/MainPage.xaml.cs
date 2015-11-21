﻿using System;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Shapes;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

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
        private DispatcherTimer SendTimer = new DispatcherTimer();
        private DispatcherTimer TestTimer = new DispatcherTimer();

        private uint bufferingCount = 0;
        private uint startCounter = 10;
        private uint sampleCount = 0;
        private uint messageCount = 0;
        private uint errorCount = 0;
        private bool activeTask = false;

        public MainPage()
        {
            this.InitializeComponent();

            TimeSpan ts = new TimeSpan(600000000); // 60 s

            TestTimer.Interval = ts;
            TestTimer.Tick += TestSend;
            TestTimer.Start();

            ts = new TimeSpan(50000000); // 5 s
            SendTimer.Interval = ts;
            SendTimer.Tick += Send;

            ts = new TimeSpan(10000000); // 1 s
            startTimer.Interval = ts;
            startTimer.Tick += Tick;
            startTimer.Start();

            SetLabels(LibAudio.HeartBeatType.INVALID);
            text1.Text = "STARTING";
        }

        ~MainPage()
        {
            engine.Finish();
        }

        private void ThreadDelegate(System.UInt32 i0, int i1, int i2, int i3, int i4, int i5, System.UInt64 i6, System.UInt64 i7, System.UInt32 i8)
        {
            var ignored1 = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
            {
                UpdateUI(i0, i1, i2, i3, i4, i5, i6, i7, i8);
            });

            var ignored2 = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
            {
                try
                {
                    if ((LibAudio.HeartBeatType)i1 == LibAudio.HeartBeatType.DATA || messageCount == 100)
                    {
                        messageCount = 0;
                        client.AddMessage(i1, i2, i3, i4, i6, i7);
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
                    Error(ex.ToString());
                }
            });
        }

        private void UpdateUI(System.UInt32 i0, int i1, int i2, int i3, int i4, int i5, System.UInt64 i6, System.UInt64 i7, System.UInt32 i8)
        {
            try
            {
                text2.Text = i0.ToString();
                SetLabels((LibAudio.HeartBeatType)i1);

                switch ((LibAudio.HeartBeatType)i1)
                {
                    case LibAudio.HeartBeatType.DATA:
                        text1.Text = "DATA";
                        bufferingCount = 0;
                        break;
                    case LibAudio.HeartBeatType.INVALID:
                        text1.Text = "INVALID";
                        break;
                    case LibAudio.HeartBeatType.SILENCE:
                        text1.Text = "SILENCE";
                        bufferingCount = 0;
                        break;
                    case LibAudio.HeartBeatType.BUFFERING:
                        text1.Text = "BUFFERING";
                        bufferingCount++;
                        break;
                    case LibAudio.HeartBeatType.DEVICE_ERROR:
                        text1.Text = "ERROR";
                        ResetEngine(10, "ERROR");
                        break;
                    case LibAudio.HeartBeatType.NODEVICE:
                        text1.Text = "NO DEVICES";
                        Reboot();
                        break;
                }

                if (bufferingCount == 60)
                {
                    Reboot();
                }

                if (bufferingCount  >= 5 && bufferingCount % 5 == 0)
                {
                    ResetEngine(5, "RESET");
                }
                
                text3.Text = i2.ToString();
                text4.Text = i3.ToString();
                text5.Text = i4.ToString();
                text6.Text = i5.ToString();
                text7.Text = i6.ToString();
                text8.Text = i7.ToString();

                if ((LibAudio.HeartBeatType)i1 != LibAudio.HeartBeatType.BUFFERING)
                {
                    canvas.Children.Clear();
                }

                if ((LibAudio.HeartBeatType)i1 == LibAudio.HeartBeatType.DATA)
                {
                    System.UInt64 vol = i6 / 20;
                    if (vol > 800) vol = 800;

                    Direction(i8, 0.4, -1 * i2, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Red), 4);
                    Direction(i8, 0.4, -1 * i3, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Green), 2);
                    Direction(i8, 0.4, -1 * i4, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Blue), 1);
                    sampleCount++;       
                }
                text9.Text = sampleCount.ToString();
            }
            catch (Exception ex)
            {
                Error(ex.Message);
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

        private async void Tick(Object sender, Object e)
        {
            try
            {
                if (startCounter == 0)
                {
                    text1.Text = "STARTED";
                    startTimer.Stop();
                    engine = new LibAudio.WASAPIEngine();
#if _WIN64
                    LibAudio.TDEParameters param = new LibAudio.TDEParameters(1, 0, 0, 0, 3);
#else
                    LibAudio.TDEParameters param = new LibAudio.TDEParameters(2, 0, 1, 0, 0, 44100, 300, 50, 2000, 32000, -60, false, "");
#endif
                    await engine.InitializeAsync(ThreadDelegate, param);
                }
                else
                {
                    text2.Text = startCounter.ToString();
                    startCounter--;
                }
            }
            catch (Exception ex)
            {
                Error(ex.Message);
            }
        }

        private void TestSend(object sender, object e)
        {
            if (errorCount == 5) Reboot();
            if (client.Messages() > 0)
            {
                ResetEngine(60, "SENDING");
                SendTimer.Start();
            }
        }

        private void Send(object sender, object e)
        {
            SendTimer.Stop();
            try
            {
                Windows.Foundation.AsyncActionCompletedHandler handler = (Windows.Foundation.IAsyncAction asyncInfo, Windows.Foundation.AsyncStatus asyncStatus) =>
                {
                    var ignored = Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
                    {
                        switch (asyncStatus)
                        {
                            case Windows.Foundation.AsyncStatus.Completed:
                                exception.Text = "Messages sent";
                                errorCount = 0;
                                if (startCounter > 2) startCounter = 2;
                                break;
                            case Windows.Foundation.AsyncStatus.Canceled: exception.Text = "Canceled " + asyncInfo.ErrorCode; break;
                            case Windows.Foundation.AsyncStatus.Error:
                                exception.Text = "Error: " + asyncInfo.ErrorCode;
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
                    if (activeTask)
                    {
                        label0.Text = "Sending messages ...";
                    }
                    else
                    {
                        label0.Text = "No messages pending";
                    }
                }
                else
                {
                    label0.Text = "Active task";
                }
            }
            catch (Exception ex)
            {
                Error(ex.ToString());
            }
        }

        private void ResetEngine(uint counter, string str)
        {
            engine.Finish();
            engine = null;
            EmptyTexts(str);
            startCounter = counter;
            startTimer.Start();
        }

        private void Reboot()
        {
            engine.Finish();
            engine = null;
            EmptyTexts("REBOOTING");
            LibRPi.HelperClass hc = new LibRPi.HelperClass();
            hc.RebootComputer();
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

        private void SetLabels(LibAudio.HeartBeatType type)
        {
            label1.Text = "STATUS";

            switch (type)
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
    }
}
