#define _PI2_1

namespace AccessData
{
    public sealed class Access
    {
        static string ssid0 = "{XXX}";
        static string ssid1 = "{XXX}";
        static string ssid2 = "{XXX}";
        static string ssid3 = "{XXX}";
        static string ssid4 = "{XXX}";

        static string wifi_password0 = "{XXX}";
        static string wifi_password1 = "{XXX}";
        static string wifi_password2 = "{XXX}";
        static string wifi_password3 = "{XXX}";
        static string wifi_password4 = "{XXX}";

        static string user = "{XXX}";
        static string password = "{XXX}";

        static string iotHubUri = "{XXX}";
        static string connectionString = "{XXX}";

#if _WIN64
        static string deviceId = "{XXX}";
        static string deviceKey = "{XXX}";
#else

#if _PI2_1
        static string deviceId = "{XXX}";
        static string deviceKey = "{XXX}";
#elif _PI2_2
        static string deviceId = "{XXX}";
        static string deviceKey = "{XXX}";
#else
        static string deviceId = "{XXX}";
        static string deviceKey = "{XXX}";
#endif
#endif
        public static string Ssid { get { return ssid0; } }

        public static uint Networks { get { return 5; } }

        public static string SSID(uint index)
        {
            switch (index)
            {
                case 1: return ssid1;
                case 2: return ssid2;
                case 3: return ssid3;
                case 4: return ssid4;
                default: return ssid0;
            }
        }
        public static string Wifi_Password { get { return wifi_password0; } }
        public static string WIFI_Password(uint index)
        {
            switch (index)
            {
                case 1: return wifi_password1;
                case 2: return wifi_password2;
                case 3: return wifi_password3;
                case 4: return wifi_password4;
                default: return wifi_password0;
            }
        }
        public static string User { get { return user; } }
        public static string Password { get { return password; } }
        public static string IoTHubUri { get { return iotHubUri; } }
        public static string DeviceKey { get { return deviceKey; } }
        public static string DeviceID { get { return deviceId; } }
        public static string ConnectionString { get { return connectionString; } }
    }
}
