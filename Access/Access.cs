namespace Access
{
    public sealed class Access
    {
        static string iotHubUri = "{XXX}";
        static string connectionString = "{XXX}";
        static string deviceId = "{XXX}";

        public static string IoTHubUri { get { return iotHubUri; }}
        public static string DeviceID { get { return deviceId; }}
        public static string ConnectionString { get { return connectionString; }}
    }
}
