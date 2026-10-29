/*

Copyright (c) 2026 Bournemouth International Neurodiverse Friends

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

IRAM_ATTR void restart() {
    ESP.restart();
}

class WiFiConfig
{
public:
    WiFiConfig();
    void loop();
    void setup();

private:
    const String APName   = "BINF Link";
    const String Filename = "wifi.conf";
    const String Form = ""
    "<html><head><meta charset='UTF-8'><title>BINF Link</title></head>"
    "<body><h1>Bournemouth International Neurodiverse Friends</h1>"
        "<h1>This portal simply creates a SSID for raising awareness. This Access Point is not a Gateway to the Internet.</h1>"
        "<p>MSc Internet of Things: Project Demonstration</p>"
        "<p>By Bournemouth University Student ID No: 5653311</p>"
        "<p>info@binf.uk</p>"
    "</body></html>";
    const uint8_t WiFiPin = 0U;

    uint32_t m_accessCnt[256];
    DNSServer m_dnsServer;
    String m_hostname;
    String m_page;
    String m_password;
    String m_pwpage;
    uint8_t m_reset_cnt;
    String m_ssid;
    enum {
        INIT,
        RESET_CNT,
        AP,
        STA
    } m_state;
    ESP8266WebServer m_webServer;

    void edit();
    bool loadConfig();
    void loadPage();
    void loadSSID();
    void resetConfig();
    bool saveConfig();
    bool savePage();
    void signIn();
    void startAP();
    void stats();
    void startStation();
};

WiFiConfig::WiFiConfig() : m_state(INIT), m_webServer(80)
{
    for (uint8_t n = 0U; n < 255U; n++) m_accessCnt[n] = 0U;
}

void WiFiConfig::edit()
{
    const String html = "<html><head><meta charset='UTF-8'><title>Edit SSID and page.</title></head>"
    "<body><center><h1>Edit SSID and page</h1>"
        "<form action='/savePage'>"
            "<p><input type='text' name='ssid' placeholder='SSID'>"
            "<p><input type='text' name='pass' placeholder='Password'>"
            "<p><input type='text' name='newp' placeholder='New Password'>"
            "<p><input type='text' name='page' placeholder='<html><body></body></html>'>"
            "<p><button type='submit'>Save</button>"
        "</form>"
    "</center></body></html>";
    m_webServer.send(200, "text/html", html);
}

bool WiFiConfig::loadConfig()
{
    m_ssid = "";
    m_password = "";
    m_hostname = "";
    if (LittleFS.exists(Filename))
    {
        File f = LittleFS.open(Filename, "r");
        while (f.available())
        {
            String line = f.readStringUntil('\n');
            Serial.println("<" + line + ">");
            if (line.startsWith("SSID: ")) m_ssid = line.substring(6);
            if (line.startsWith("Password: ")) m_password = line.substring(10);
            if (line.startsWith("Hostname: ")) m_hostname = line.substring(10);
        }
        f.close();
        Serial.println(m_ssid);
        Serial.println(m_password);
        Serial.println(m_hostname);
    }
    return !(m_ssid.isEmpty() || m_password.isEmpty() || m_hostname.isEmpty());
}

void WiFiConfig::loadPage()
{
    const String filename = "page.html";
    m_page = Form;
    if (LittleFS.exists(filename))
    {
        File f = LittleFS.open(filename, "r");
        m_page = "";
        while (f.available())
        {
            String line = f.readStringUntil('\n');
            m_page += line;
        }
        f.close();
    }
}

void WiFiConfig::loadSSID()
{
    const String filename = "ssid.txt";
    char id[10];
    sprintf(id, "%08X", ESP.getChipId());
    m_ssid = APName + " " + id;
    m_pwpage = "789thing";
    if (LittleFS.exists(filename))
    {
        File f = LittleFS.open(filename, "r");
        while (f.available())
        {
            String line = f.readStringUntil('\n');
            Serial.println("<" + line + ">");
            if (line.startsWith("SSID: ")) m_ssid = line.substring(6);
            if (line.startsWith("Password: ")) m_pwpage = line.substring(10);
        }
        f.close();
    }
}

void WiFiConfig::loop()
{
    switch(m_state)
    {
        case INIT:
        {
            if (digitalRead(WiFiPin) == 0)
            {
                // WiFi reset pin low during initialisation - start counting.
                m_reset_cnt = 0U;
                m_state = RESET_CNT;
            }
            else
            {
                // WiFi reset pin restarts the ESP when pin goes low.
                attachInterrupt(WiFiPin, restart, FALLING);

                if(loadConfig())
                {
                    // Start the WiFi station.
                    startStation();
                    m_state = STA;
                }
                else
                {
                    // Could not load config, start the Access Point and Configuration Portal.
                    startAP();
                    m_state = AP;
                }
            }
            break;
        }

        case RESET_CNT:
        {
            if (digitalRead(WiFiPin) != 0)
            {
                // If WiFi reset pin goes high after timeout, reset config.
                if (m_reset_cnt >= 30U) resetConfig();

                // WiFi reset pin high, go back to initial state.
                m_reset_cnt = 0U;
                m_state = INIT;
            }
            else
            {
                // Reset when WiFi reset pin low for 3 seconds.
                delay(100U);
                Serial.printf("%d\r\n", m_reset_cnt);
                if (m_reset_cnt < 30U) m_reset_cnt++;
            }
            break;
        }

        case AP:
        {
            m_dnsServer.processNextRequest();
            m_webServer.handleClient();
            break;
        }

        case STA:
        {
            MDNS.update();
            break;
        }

        default:
        {
            m_state = INIT;
        }
    }
}

void WiFiConfig::resetConfig()
{
    Serial.println("Configuration reset.");
    File f = LittleFS.open(Filename, "w");
    // Leave file empty.
    f.close();
}

bool WiFiConfig::saveConfig()
{
    // Iterate through form data.
    int num_args = m_webServer.args();
    for (int n = 0; n < num_args ; n++){
        const String& name = m_webServer.argName(n);
        const String& val  = m_webServer.arg(n);
        Serial.println(name + ": " + val);
        if (name.equals("ssid")) m_ssid = val;
        if (name.equals("pass")) m_password = val;
        if (name.equals("hostname")) m_hostname = val;
    }

    // Save to file.
    Serial.println("Saving config.");
    File f = LittleFS.open(Filename, "w");
    f.printf("SSID: %s\n", m_ssid.c_str());
    f.printf("Password: %s\n", m_password.c_str());
    f.printf("Hostname: %s\n", m_hostname.c_str());
    f.close();

    // Send response.
    const String html = "<html><head><title>WiFi Configuration Saved.</title></head>"
    "<body><center><h5>WiFi Configuration Saved.</h5>"
    "</center></body></html>";
    m_webServer.send(200, "text/html", html);

    return true;
}

bool WiFiConfig::savePage()
{
    // Iterate through form data.
    int num_args = m_webServer.args();
    String page;
    String pass;
    String pwnew;
    String ssid;
    for (int n = 0; n < num_args ; n++){
        const String& name = m_webServer.argName(n);
        const String& val  = m_webServer.arg(n);
        Serial.println(name + ": " + val);
        if (name.equals("ssid")) ssid = val;
        if (name.equals("page")) page = val;
        if (name.equals("pass")) pass = val;
        if (name.equals("newp")) pwnew = val;
    }

    // Save to file.
    if (pass.equals(m_pwpage))
    {
        if (pwnew.length() > 5U) m_pwpage = pwnew;
        m_page = page;
        m_ssid = ssid;
        Serial.println("Saving config.");
        File f = LittleFS.open("ssid.txt", "w");
        f.printf("SSID: %s\n", m_ssid.c_str());
        f.printf("Password: %s\n", m_pwpage.c_str());
        f.close();
        File fp = LittleFS.open("page.html", "w");
        fp.printf("%s", m_page.c_str());
        fp.close();
    }

    // Send response.
    const String html = "<html><head><title>Page Saved.</title></head>"
    "<body><h1>Page Saved.</h1>"
    "</body></html>";
    m_webServer.send(200, "text/html", html);

    return true;
}

void WiFiConfig::setup()
{
    pinMode(WiFiPin, INPUT_PULLUP);
    m_state = INIT;
}

void WiFiConfig::signIn()
{
    uint8_t ipadd = m_webServer.client().remoteIP()[3];
    m_accessCnt[ipadd]++;
    m_webServer.send(200, "text/html", m_page);
}

void WiFiConfig::startAP()
{
    // Setup the Access Point.
    Serial.println("Starting AP.");
    WiFi.mode(WIFI_AP);
    const IPAddress ip(192, 168, 1, 1);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
    char id[10];
    sprintf(id, "%08X", ESP.getChipId());
    loadSSID();
    WiFi.softAP(m_ssid);

    // Setup the captive DNS.
    const uint16_t port(53U);
    m_dnsServer.start(port, "*", ip);

    // Setup the web server.
    loadPage();
    m_webServer.onNotFound([this](){
        this->signIn();
    });
    m_webServer.on("/edit", [this](){
        this->edit();
    });
    m_webServer.on("/save", [this](){
        this->saveConfig();
    });
    m_webServer.on("/savePage", [this](){
        this->savePage();
    });
    m_webServer.on("/stats", [this](){
        this->stats();
    });
    m_webServer.begin();

    Serial.println("Captive WiFi configuration portal started.");
}

void WiFiConfig::startStation()
{
    WiFi.mode(WIFI_STA);
    WiFi.hostname(m_hostname);
    WiFi.begin(m_ssid, m_password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100U);
        Serial.print(".");
    }
    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin(m_hostname)) {
        Serial.println("mDNS started");
    } 
}

void WiFiConfig::stats()
{
    String html = "<html><body>";
    char id[10];
    sprintf(id, "%08X", ESP.getChipId());
    html += id;
    html += "<br>";
    for (uint8_t n = 1U; n < 255U; n++)
    {
        uint32_t cnt = m_accessCnt[n];
        if (cnt > 0U)
        {
            html += n;
            html += ": "; 
            html += cnt;
            html += "<br>";
        }
    }
    html += "</body></html>";
    m_webServer.send(200, "text/html", html);
}

WiFiConfig wifi;

void setup() {
    Serial.begin(115200);
    Serial.println("");
    LittleFS.begin();
    wifi.setup();
}

void loop() {
    wifi.loop();
}
