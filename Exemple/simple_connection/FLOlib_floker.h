#include <Arduino.h>

#define DEBUG_FLOKER_LIB false
String FLOKER_DATA;

// Device type detection call associated libraries
#ifdef ESP8266_ENABLED
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif
#ifdef ESP32_ENABLED
#include <WiFi.h>
#include <HTTPClient.h>
#endif

#define HTTPS_REQUEST "https://"
#define HTTP_REQUEST "http://"
#define HTTPS_PORT 443
#define HTTP_PORT 80

#define DEFAULT_START_POLLING_PATH "devices/"
#define DEFAULT_START_IOT_PATH "iot/"
#define DEFAULT_SERIAL_BAUDRATE 9600

class Floker
{
private:
    // WiFi
    const char *ssid;
    const char *password;

    // Floker server connection
    String request_type;
    String server;
    unsigned short port = 0;
    String root_path;
    String token;

    // WiFi and HTTP client object
    WiFiClient wifi_client;
    HTTPClient http_client;

    // Channels variables
    typedef struct
    {
        String topic_path;
        String state;
        void (*function)();
    } channel;

    channel *channels;
    unsigned short nb_channels;
    unsigned short channel_index = 0;

    // Auto pathing device
    String device_name;
    String device_path;

    // Is connected polling
    bool is_connected_polling = false;
    unsigned long last_connection_update = 0;
    unsigned long connection_update_interval = 10000;
    String connection_state_topic_path;

    // Tools
    void alloc_channels_memory(unsigned short nb_channels)
    {
        this->channels = (channel *)malloc(nb_channels * sizeof(channel));
    }

    // Making uri for request
    String make_uri(String topic, String data_to_write = String(""))
    {
        String uri = this->request_type + this->server + String(":") + String(this->port) + this->root_path;

        // Write Mode
        if (data_to_write != String(""))
        {
            uri += String("write?");
            uri += String("token=") + this->token;
            uri += String("&topic=") + topic;
            uri += String("&state=") + data_to_write;
        }

        // Read Mode
        else
        {
            uri += String("read?");
            uri += String("token=") + this->token;
            uri += String("&topic=") + topic;
            uri += String("&parse=state");
        }

        return uri;
    }

    // Check all the channels
    void check_all_channels()
    {
        for (byte k = 0; k < this->channel_index; k++)
        {
            if (DEBUG_FLOKER_LIB)
            {
                Serial.println();
                Serial.print("Topic path: ");
                Serial.println(this->channels[k].topic_path);
            }

            int http_code;
            bool read_success = this->read(&http_code, &FLOKER_DATA, this->channels[k].topic_path);

            if (read_success)
            {
                if (DEBUG_FLOKER_LIB)
                {
                    Serial.print("State ------> ");
                    Serial.println(FLOKER_DATA);
                    Serial.print("Old state --> ");
                    Serial.println(this->channels[k].state);
                }

                // Check if the state have changed
                if (this->channels[k].state != FLOKER_DATA)
                {
                    if (DEBUG_FLOKER_LIB)
                    {
                        Serial.println("The state have changed, let's execute the callback function !");
                    }
                    this->channels[k].function();
                    this->channels[k].state = FLOKER_DATA;
                }
                else if (DEBUG_FLOKER_LIB)
                {
                    Serial.println("The state have not changed.");
                }
            }
        }
    }

public:
    // Natural Constructor
    Floker(
        const char *ssid,
        const char *password,
        bool secure_connection,
        String server,
        String root_path,
        String token,
        unsigned short nb_channels,
        String device_path = String(""))
    {
        this->ssid = ssid;
        this->password = password;
        this->request_type = secure_connection ? String(HTTPS_REQUEST) : String(HTTP_REQUEST);
        this->server = server;
        if (port == 0)
        {
            this->port = secure_connection ? HTTPS_PORT : HTTP_PORT;
        }
        else
        {
            this->port = port;
        }
        this->root_path = root_path;
        this->token = token;
        this->nb_channels = nb_channels;
        this->device_path = device_path;
    }

    // Choose a custom server port
    void set_port(unsigned short port)
    {
        this->port = port;
    }

    // Start the WiFi connection
    void begin()
    {
        // Init channels
        this->alloc_channels_memory(this->nb_channels);

        // Init Serial
        if (DEBUG_FLOKER_LIB && Serial)
        {
            Serial.begin(DEFAULT_SERIAL_BAUDRATE);
        }

        // Init WiFi connection
        WiFi.begin(this->ssid, this->password);
        if (DEBUG_FLOKER_LIB)
        {
            Serial.print("Try to connect to ");
            Serial.println(this->ssid);
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

    // Set connection polling update
    void set_connection_polling(
        unsigned long connection_update_interval,
        String device_name,
        String no_default_device_path = String(""),
        String start_connection_path = DEFAULT_START_POLLING_PATH)
    {
        this->is_connected_polling = true;
        this->connection_update_interval = connection_update_interval;
        this->device_name = device_name;

        // Create connection state topic
        this->connection_state_topic_path = start_connection_path;
        if (no_default_device_path != String(""))
            this->connection_state_topic_path += no_default_device_path + String("/");
        else if (this->device_path != String(""))
            this->connection_state_topic_path += this->device_path + String("/");
        this->connection_state_topic_path += this->device_name;
    }

    void connection_polling_handle()
    {
        if (this->is_connected_polling && millis() - this->last_connection_update > this->connection_update_interval)
        {
            this->last_connection_update = millis();
            this->write(this->connection_state_topic_path, "connected", false);
        }
    }

    // Create channel callback function link
    void subscribe(String topic_path, void (*function)())
    {
        // Patern device path is set
        if (this->device_path != String(""))
        {
            topic_path = DEFAULT_START_IOT_PATH + this->device_path + topic_path;
        }
        this->channels[this->channel_index] = channel{
            topic_path,
            String("default value"),
            (*function)};
        this->channel_index++;
    }

    // Read a topic
    bool read(int *http_code, String *get_data, String topic_path)
    {
        bool success = false;
        String uri = make_uri(topic_path);

        // Opening the connection
        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("Open read request: ");
            Serial.print("uri: ");
            Serial.println(uri);
        }

        // Send the request
        this->http_client.begin(this->wifi_client, uri);
        *http_code = this->http_client.GET();

        // Get the http code response
        if (DEBUG_FLOKER_LIB)
        {
            Serial.print("Response code: ");
            Serial.println(*http_code);
        }

        // Get the data
        if (*http_code == 200)
        {
            *get_data = this->http_client.getString();
            success = true;
        }

        if (DEBUG_FLOKER_LIB && success)
        {
            Serial.println("The request was a success, the data is: ");
            Serial.println(*get_data);
        }
        else if (DEBUG_FLOKER_LIB)
        {
            Serial.println("The request was a failure !");
            Serial.println("The error response is :");
            Serial.println(this->http_client.getString());
        }

        // Close the connection
        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("End of the read request close the connection.");
        }
        this->http_client.end();

        return success;
    }

    // Write a topic
    void write(String topic_path, String data_to_write, bool autocomplete_topic = true)
    {
        bool success = false;
        // Patern device path is set
        if (autocomplete_topic && this->device_path != String(""))
        {
            topic_path = DEFAULT_START_IOT_PATH + this->device_path + topic_path;
        }

        String uri = make_uri(topic_path, data_to_write);

        while (!success)
        {
            // Opening the connection
            if (DEBUG_FLOKER_LIB)
            {
                Serial.print("Open write request: ");
                Serial.print("uri: ");
                Serial.println(uri);

                Serial.println();
                Serial.print("Write [");
                Serial.print(data_to_write);
                Serial.print("] on the topic [");
                Serial.print(topic_path);
                Serial.println("] : ");
            }

            // Send the request
            this->http_client.begin(this->wifi_client, uri);
            int http_code = this->http_client.GET();

            // Get the http code response
            success = (http_code == 200);
            if (DEBUG_FLOKER_LIB)
            {
                Serial.print("Response code: ");
                Serial.println(http_code);
            }

            if (DEBUG_FLOKER_LIB && success)
            {
                Serial.println("The request was a success, the response is: ");
                Serial.println(this->http_client.getString());
            }
            else if (DEBUG_FLOKER_LIB)
            {
                Serial.println("The request was a failure !");
                Serial.println("The error response is :");
                Serial.println(this->http_client.getString());
            }

            // Close the connection
            if (DEBUG_FLOKER_LIB)
            {
                Serial.println("End of the read request close the connection.");
            }
            this->http_client.end();
        }
    }

    // Check the channel state
    void handle()
    {
        // Check WiFi connection status
        if (WiFi.status() == WL_CONNECTED)
        {
            connection_polling_handle();
            check_all_channels();
        }
    }
};
