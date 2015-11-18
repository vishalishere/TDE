namespace AccessData
{
    public sealed class Access
    {
        static string user = "{XXXX}";
        static string password = "{XXXX}";

        static string iotHubUri = "{XXXX}";
        static string deviceKey = "{XXXX}";

        static string connectionString = "{XXXX}";
        static string deviceId = "{XXXX}";

        public static string User { get { return user; } }
        public static string Password { get { return password; } }
        public static string IoTHubUri { get { return iotHubUri; } }
        public static string DeviceKey { get { return deviceKey; } }
        public static string DeviceID { get { return deviceId; } }
        public static string ConnectionString { get { return connectionString; } }
    }
}
