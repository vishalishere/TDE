namespace AccessData
{
    public sealed class Access
    {
        static string user = "Administrator";
        static string password = "p@ssw0rd";

        static string iotHubUri = "AudioHub.azure-devices.net";
        static string deviceKey = "I+Ak2nEjd9O9yzgWA7S8Ibd2W2WqTuKcrJu+DlY8vYc=";

        static string connectionString = "HostName=AudioHub.azure-devices.net;SharedAccessKeyName=iothubowner;SharedAccessKey=JbdA3RxBVepiJ5cITBdrt5SmDD2UVcJjQHDBuVO7uu0=";
        static string deviceId = "RaspberryPi2";

        public static string User { get { return user; } }
        public static string Password { get { return password; } }
        public static string IoTHubUri { get { return iotHubUri; } }
        public static string DeviceKey { get { return deviceKey; } }
        public static string DeviceID { get { return deviceId; } }
        public static string ConnectionString { get { return connectionString; } }
    }
}
