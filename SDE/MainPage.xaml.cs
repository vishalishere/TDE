// #define KINECT

using System;
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
        private AudioEngine.WASAPIEngine engine;
        private IoTHubLibrary.IotHubClient client;
        private DispatcherTimer timer;
        private uint bufferingCount;
        private uint startCounter = 10;
        private uint sampleCount = 0;
        private uint messageCounter = 0;

        public MainPage()
        {
            this.InitializeComponent();

            client = new IoTHubLibrary.IotHubClient();

            TimeSpan ts = new TimeSpan(10000000);
            timer = new DispatcherTimer();
            timer.Interval = ts;
            timer.Tick += Tick;
            timer.Start();
            SetLabels(AudioEngine.HeartBeatType.INVALID);
            text1.Text = "STARTING";
        }

        ~MainPage()
        {
            engine.Finish();
        }

        private void ThreadDelegate(System.UInt32 i0, int i1, int i2, int i3, int i4, int i5, System.UInt64 i6, System.UInt64 i7, System.UInt32 i8)
        {
            var ignored1 = this.Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
            {
                UpdateUI(i0, i1, i2, i3, i4, i5, i6, i7, i8);
            });

            var ignored2 = this.Dispatcher.RunAsync(CoreDispatcherPriority.Low, () =>
            {
                try
                {
                    if ((AudioEngine.HeartBeatType)i1 == AudioEngine.HeartBeatType.DATA || messageCounter == 100)
                    {
                        messageCounter = 0;
                        if (client.SendDeviceToCloudMessagesAsync(i1, i2, i3, i4, i6, i7))
                        {
                            label0.Text = "Active";
                        }
                        else
                        {
                            label0.Text = "New message";
                        }
                    }
                    else
                    {
                        messageCounter++;
                        label0.Text = "";
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
                SetLabels((AudioEngine.HeartBeatType)i1);

                switch ((AudioEngine.HeartBeatType)i1)
                {
                    case AudioEngine.HeartBeatType.DATA:
                        text1.Text = "DATA";
                        bufferingCount = 0;
                        break;
                    case AudioEngine.HeartBeatType.INVALID:
                        text1.Text = "INVALID";
                        break;
                    case AudioEngine.HeartBeatType.SILENCE:
                        text1.Text = "SILENCE";
                        bufferingCount = 0;
                        break;
                    case AudioEngine.HeartBeatType.BUFFERING:
                        text1.Text = "BUFFERING";
                        bufferingCount++;
                        break;
                    case AudioEngine.HeartBeatType.DEVICE_ERROR:
                        text1.Text = "ERROR";
                        ResetEngine(10);
                        break;
                    case AudioEngine.HeartBeatType.NODEVICE:
                        text1.Text = "NO DEVICES";
                        Reboot();
                        break;
                }

                if (bufferingCount == 5) ResetEngine(5);
                if (bufferingCount == 10) Reboot();

                text3.Text = i2.ToString();
                text4.Text = i3.ToString();
                text5.Text = i4.ToString();
                text6.Text = i5.ToString();
                text7.Text = i6.ToString();
                text8.Text = i7.ToString();

                if ((AudioEngine.HeartBeatType)i1 != AudioEngine.HeartBeatType.BUFFERING)
                {
                    canvas.Children.Clear();
                }

                if ((AudioEngine.HeartBeatType)i1 == AudioEngine.HeartBeatType.DATA)
                {
                    System.UInt64 vol = i6 / 20;
                    if (vol > 800) vol = 800;

                    Direction(i8, 0.4, -1 * i2, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Red), 4);
                    Direction(i8, 0.4, -1 * i3, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Green), 2);
                    Direction(i8, 0.4, -1 * i4, 800, (int)vol, new SolidColorBrush(Windows.UI.Colors.Blue), 1);
                    sampleCount++;
                    text9.Text = sampleCount.ToString();
                }
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
                    timer.Stop();
                    engine = new AudioEngine.WASAPIEngine();
#if KINECT
                    AudioEngine.TDEParameters param = new AudioEngine.TDEParameters(1, 0, 0, 0, 3);
#else
                    AudioEngine.TDEParameters param = new AudioEngine.TDEParameters(2, 0, 1, 0, 0);
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

        private void ResetEngine(uint counter)
        {
            engine.Finish();
            engine = null;
            EmptyTexts("RESETTING");
            startCounter = counter;
            timer.Start();
        }

        private void Reboot()
        {
            engine.Finish();
            engine = null;
            EmptyTexts("REBOOTING");
            RPi_Helper.HelperClass hc = new RPi_Helper.HelperClass();
            hc.RebootComputer();
        }

        private void Error(string err)
        {
            label1.Text = err;
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

        private void SetLabels(AudioEngine.HeartBeatType type)
        {
            label1.Text = "STATUS";

            switch (type)
            {
                case AudioEngine.HeartBeatType.INVALID:
                case AudioEngine.HeartBeatType.DEVICE_ERROR:
                case AudioEngine.HeartBeatType.NODEVICE:
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
                case AudioEngine.HeartBeatType.DATA:
                case AudioEngine.HeartBeatType.SILENCE:
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
                case AudioEngine.HeartBeatType.BUFFERING:
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
