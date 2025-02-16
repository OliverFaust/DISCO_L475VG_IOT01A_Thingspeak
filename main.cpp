/* DISCO_L475VG_IOT01A_Thingspeak
 * Creator: Dr Oliver Faust
 * Year: 2025
  * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "mbed.h"
#include "NTPClient.h"
#include "NetworkInterface.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"

static constexpr size_t REMOTE_PORT = 80; // standard HTTP port

NetworkInterface*   net;
WiFiInterface *wifi;
SocketAddress address;
TCPSocket socket;

nsapi_error_t response;
char sbuffer[256];
char message[64];
float captX;
float captY;
float captZ;

const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

int scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\n");

    int count = wifi->scan(NULL,0);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];
    count = wifi->scan(ap, count);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\n", count);

    delete[] ap;
    return count;
}

bool resolve_hostname(char *hostname)
{
    /* get the host address */
    printf("\nResolve hostname %s\r\n", hostname);
    nsapi_size_or_error_t result = net->gethostbyname(hostname, &address);
    if (result != 0) {
        printf("Error! gethostbyname(%s) returned: %d\r\n", hostname, result);
        return false;
    }
    printf("%s address is %s\r\n", hostname, (address.get_ip_address() ? address.get_ip_address() : "None") );
    return true;
}

void floatToStr(char *buffer, float value) {
    int intPart = (int)value;
    int decimalPart = (int)((value - intPart) * 100); // 2 decimal places
    sprintf(buffer, "%d.%02d", intPart, decimalPart);
}

int main()
{
    int result = 0;
    printf("WiFi example\n");

#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

    printf("Connecting to network...\n\n");

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return NULL;
    }

    int count = scan_demo(wifi);
    if (count == 0) {
        printf("No WIFI APs found - can't continue further.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return NULL;
    }
    
    net = wifi->wifiInterface();
    
    printf("\nConnecting to NTP server.\n");  

    NTPClient ntp(net);
    ntp.set_server("0.uk.pool.ntp.org", 123);
    time_t seconds = ntp.get_timestamp(1000);
    printf("System time set by NTP: %s\n\n", ctime(&seconds));

    // on verifie si on trouve l'hote sur le réseau
    char hostname[]="api.thingspeak.com";
    if (!resolve_hostname(hostname)) {
        return -1;
    }
    address.set_port(REMOTE_PORT);

    BSP_TSENSOR_Init();
    BSP_HSENSOR_Init();
    BSP_PSENSOR_Init();
    
    while(true)
    {
        result = socket.open(net);
        if (result != 0) {
            printf("Error! socket.open() returned: %d\r\n", result);
            return -1;
        }

        printf("\r\n\r\nOpening connection to remote port %d\r\n", REMOTE_PORT);

        result = socket.connect(address);
        if (result != 0) {
            printf("Error! socket.connect() returned: %d\r\n", result);
            return -1;
        }

        char temp[10], humidity[10], pressure[10];

        floatToStr(temp, BSP_TSENSOR_ReadTemp());
        floatToStr(humidity, BSP_HSENSOR_ReadHumidity());
        floatToStr(pressure, BSP_PSENSOR_ReadPressure());
        sprintf(message, "{\"field1\": %s, \"field2\": %s, \"field3\": %s}", temp, humidity, pressure);
        printf("Message = %s\r\n", (message));

        sprintf(sbuffer, "GET /update?api_key=%s HTTP/1.1\r\nHost: api.thingspeak.com\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", MBED_CONF_APP_THINGSPEAK_API_KEY_WRITE, (int)strlen(message),message);

        printf("HTTP command %s\r\n", sbuffer);

        printf("Sending HTTP command to thingspeak.com...\n");//
        nsapi_size_t size = strlen(sbuffer);
        response = 0;   
        while(size)
        {
            response = socket.send(sbuffer+response, size);
            if (response < 0)
            {
                printf("Error sending data: %d\n", response);
                socket.close();
                return -1;
            }
            else
            {
            size -= response;
            }
        }
        printf("Request sent to thingspeak.com \n");//

        char rbuffer[64];
        response = socket.recv(rbuffer, sizeof rbuffer);
        if (response < 0)
        {
            printf("Error receiving data: %d\n", response);
        }
        else
        {
            printf("Received response from Thingspeak:  %d [%.*s]\n", response, strstr(rbuffer, "\r\n")-rbuffer, rbuffer);
        }

        socket.close();
        ThisThread::sleep_for(60s); 
    }
    net->disconnect();
    return 0;
}
