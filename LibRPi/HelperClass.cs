using System;
using System.IO;
using System.Threading.Tasks;
using System.Net;
using Windows.Data.Json;

namespace LibRPi
{
    public sealed class HelperClass
    {
        public async void Shutdown()
        {
            String URL = "http://localhost:8080/api/control/shutdown";
            System.Diagnostics.Debug.WriteLine(URL);
            StreamReader SR = await PostJsonStreamData(URL);
        }

        public async void Reboot()
        {
            String URL = "http://localhost:8080/api/control/reboot";
            System.Diagnostics.Debug.WriteLine(URL);
            StreamReader SR = await PostJsonStreamData(URL);
        }

        private async void StartApp(string appName)
        {
            String RelativeID = await GetPackageRelativeId(appName);
            byte[] toEncodeAsBytes = System.Text.ASCIIEncoding.ASCII.GetBytes(RelativeID);
            string appName64 = System.Convert.ToBase64String(toEncodeAsBytes);
            System.Diagnostics.Debug.WriteLine("http://localhost:8080/api/taskmanager/start?appid=" + appName64);
            StreamReader SR = await PostJsonStreamData("http://localhost:8080/api/taskmanager/start?appid=" + appName64);
        }

        private async Task<StreamReader> PostJsonStreamData(string URL)
        {
            HttpWebRequest wrGETURL = null;
            Stream objStream = null;
            StreamReader objReader = null;

            try
            {
                wrGETURL = (HttpWebRequest)WebRequest.Create(URL);
                wrGETURL.Method = "POST";
                wrGETURL.Credentials = new NetworkCredential(AccessData.Access.User, AccessData.Access.Password);

                HttpWebResponse Response = (HttpWebResponse)(await wrGETURL.GetResponseAsync());
                if (Response.StatusCode == HttpStatusCode.OK)
                {
                    objStream = Response.GetResponseStream();
                    objReader = new StreamReader(objStream);
                }
            }
            catch (Exception e)
            {
                System.Diagnostics.Debug.WriteLine("GetData " + e.Message);
            }
            return objReader;
        }

        private async Task<string> GetPackageRelativeId(String PackageName)
        {
            return await GetPackageNamedName(PackageName, "PackageRelativeId");
        }

        private async Task<string> GetPackageNamedName(string PackageName, string NamedName)
        {
            String PackageRelativeId = null;
            StreamReader SR = await GetJsonStreamData("http://localhost:8080/api/appx/installed");
            JsonObject ResultData = null;
            try
            {
                String JSONData;
                JSONData = SR.ReadToEnd();
                ResultData = (JsonObject)JsonObject.Parse(JSONData);
                JsonArray InstalledPackages = ResultData.GetNamedArray("InstalledPackages");

                //foreach (JsonObject Package in InstalledPackages)
                for (uint index = 0; index < InstalledPackages.Count; index++)
                {
                    JsonObject Package = InstalledPackages.GetObjectAt(index).GetObject();
                    String Name = Package.GetNamedString("Name");
                    if (Name.ToLower().CompareTo(PackageName.ToLower()) == 0)
                    {
                        PackageRelativeId = ((JsonObject)Package).GetNamedString(NamedName);
                        break;
                    }
                }
            }
            catch (Exception E)
            {
                System.Diagnostics.Debug.WriteLine(E.Message);
            }

            return PackageRelativeId;
        }

        private async Task<StreamReader> GetJsonStreamData(string URL)
        {
            HttpWebRequest wrGETURL = null;
            Stream objStream = null;
            StreamReader objReader = null;

            try
            {
                wrGETURL = (HttpWebRequest)WebRequest.Create(URL);
                wrGETURL.Credentials = new NetworkCredential(AccessData.Access.User, AccessData.Access.Password);
                HttpWebResponse Response = (HttpWebResponse)(await wrGETURL.GetResponseAsync());
                if (Response.StatusCode == HttpStatusCode.OK)
                {
                    objStream = Response.GetResponseStream();
                    objReader = new StreamReader(objStream);
                }
            }
            catch (Exception e)
            {
                System.Diagnostics.Debug.WriteLine("GetData " + e.Message);
            }
            return objReader;
        }
    }
}
