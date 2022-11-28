#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define DEBUG_FLOKER_LIB false

String HTTP_DATA;

class Floker
{
private:
    // WiFi
    const char *_ssid;
    const char *_password;

    // Floker server connection
    String _request_type = String("http://");
    String _server;
    short _port;
    String _root_path;
    String _token;

    // WiFi and HTTP client object
    WiFiClient _client;
    HTTPClient _http;

    // Channels variables
    typedef struct
    {
        char *topic_path;
        String state;
        void (*function)();
    } channel;

    static const int max_channels = 10;
    byte _channel_index = 0;
    channel _channels[max_channels];

public:
    // Natural Constructor
    Floker(const char *ssid, const char *password, String server, short port, String root_path, String token)
    {
        _ssid = ssid;
        _password = password;
        _server = server;
        _port = port;
        _root_path = root_path;
        _token = token;
    }

    // Create channel callback function link
    void AddChannel(char *topic_path, void (*function)())
    {
        _channels[_channel_index] = channel{
            topic_path,
            String("default value"),
            (*function)};

        _channel_index++;
    }

    // Start the WiFi connection
    void Begin()
    {
        WiFi.begin(_ssid, _password);
        if (DEBUG_FLOKER_LIB)
        {
            Serial.print("Try to connect to");
            Serial.println(_ssid);
            Serial.print("Connecting");
        }

        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            if (DEBUG_FLOKER_LIB)
            {
                Serial.print(".");
            }
        }
        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("");
            Serial.print("Connection is established ! Your ip is: ");
            Serial.println(WiFi.localIP());
        }
    }

    // Check the channel state
    void Loop()
    {
        // Check WiFi connection status
        if (WiFi.status() == WL_CONNECTED)
        {
            for (byte k = 0; k < _channel_index; k++)
            {
                if (DEBUG_FLOKER_LIB)
                {
                    Serial.println();
                    Serial.print("Topic path: ");
                    Serial.println(_channels[k].topic_path);
                }
                String request_uri = _request_type + _server + String(":") + String(_port) + _root_path + String("read?") + String("token=") + _token + String("&topic=") + String(_channels[k].topic_path) + String("&parse=state");

                if (DEBUG_FLOKER_LIB)
                {
                    Serial.print("Request: ");
                    Serial.println(request_uri);
                }

                _http.begin(_client, request_uri);
                int httpCode = _http.GET();

                if (DEBUG_FLOKER_LIB)
                {
                    Serial.print("Response code: ");
                    Serial.println(httpCode);
                }

                if (httpCode == 200)
                {
                    HTTP_DATA = _http.getString();

                    if (DEBUG_FLOKER_LIB)
                    {
                        Serial.print("State ------> ");
                        Serial.println(HTTP_DATA);
                        Serial.print("Old state --> ");
                        Serial.println(_channels[k].state);
                    }

                    // Check if the state have changed
                    if (_channels[k].state != HTTP_DATA)
                    {
                        if (DEBUG_FLOKER_LIB)
                        {
                            Serial.println("The state have changed, let's execute the callback function !");
                        }
                        _channels[k].function();
                        _channels[k].state = HTTP_DATA;
                    }
                }
                _http.end();
                if (DEBUG_FLOKER_LIB)
                {
                    Serial.println("Connection request is closed");
                }
            }
        }
    }

    // Write state on a Topic
    void Write(String topic_to_write, String state)
    {
        bool success = false;
        while (!success)
        {
            if (DEBUG_FLOKER_LIB)
            {
                Serial.println();
                Serial.print("Write [");
                Serial.print(state);
                Serial.print("] on the topic [");
                Serial.print(topic_to_write);
                Serial.println("] : ");
            }
            String request_uri = _request_type + _server + String(":") + String(_port) + _root_path + String("write?") + String("token=") + _token + String("&topic=") + topic_to_write + String("&state=") + state;

            if (DEBUG_FLOKER_LIB)
            {
                Serial.print("Request: ");
                Serial.println(request_uri);
            }

            _http.begin(_client, request_uri);
            int httpCode = _http.GET();

            if (DEBUG_FLOKER_LIB)
            {
                Serial.print("Response code: ");
                Serial.println(httpCode);
            }
            
            if (httpCode == 200)
            {
                success = true;
                if (DEBUG_FLOKER_LIB)
                {
                    Serial.print("Response: ");
                    Serial.println(_http.getString());
                }
            }

            _http.end();

            if (DEBUG_FLOKER_LIB)
            {
                Serial.println("Connection request is closed");
            }
        }
    }
};
