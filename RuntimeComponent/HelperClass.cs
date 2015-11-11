using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net; 

namespace RuntimeComponent
{
    public sealed class HelperClass
    {
        public async void ShutdownComputer()
        {

            String URL = "http://localhost:8080/api/control/shutdown";
            System.Diagnostics.Debug.WriteLine(URL);
            StreamReader SR = await PostJsonStreamData(URL);

        }

        public async void RebootComputer()
        {
            String URL = "http://localhost:8080/api/control/reboot";
            System.Diagnostics.Debug.WriteLine(URL);
            StreamReader SR = await PostJsonStreamData(URL);
        }

        private async Task<StreamReader> PostJsonStreamData(String URL)
        {
            HttpWebRequest wrGETURL = null;
            Stream objStream = null;
            StreamReader objReader = null;

            try
            {
                wrGETURL = (HttpWebRequest)WebRequest.Create(URL);
                wrGETURL.Method = "POST";
                wrGETURL.Credentials = new NetworkCredential("Administrator", "p@ssw0rd");

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
