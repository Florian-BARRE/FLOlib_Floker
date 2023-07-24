#include <Arduino.h>

#define DEBUG_FLOKER_LIB false
String FLOKER_DATA;

#define FLOLIB_FLOKER_VERSION "2.1.0"

// Device type detection call associated libraries
#ifdef ESP8266_ENABLED
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define FLOKER_DEVICE_TYPE "esp8266"
#endif
#ifdef ESP32_ENABLED
#include <WiFi.h>
#include <HTTPClient.h>
#define FLOKER_DEVICE_TYPE "esp32"
#endif

#define HTTPS_REQUEST "https://"
#define HTTP_REQUEST "http://"
#define HTTPS_PORT 443
#define HTTP_PORT 80

#define DEFAULT_START_POLLING_PATH "devices/"
#define DEFAULT_STATE_POLLING_PATH "/state"
#define DEFAULT_INTERVAL_POLLING_PATH "/interval"
#define DEFAULT_TYPE_POLLING_PATH "/type"
#define DEFAULT_VERSION_POLLING_PATH "/version"
#define DEFAULT_IP_POLLING_PATH "/ip"

#define DEFAULT_START_IOT_PATH "iot/"
#define DEFAULT_SERIAL_BAUDRATE 115200

static unsigned long global_connection_update_interval = 10000;

class Floker
{
private:
    // Device
    String device_type = FLOKER_DEVICE_TYPE;
    String ip;

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
        char *topic_path;
        String state;
        void (*function)();
    } channel;

    channel *channels;
    unsigned short nb_channels = 0;

    // Auto pathing device
    String device_name;
    String device_path;

    // Is connected polling
    bool is_connected_polling = false;
    bool static_information_pushed = false;
    unsigned long last_connection_update = 0;
    String connection_state_topic_path;
    String connection_interval_topic_path;
    String connection_type_topic_path;
    String connection_version_topic_path;
    String connection_ip_topic_path;

    // Tools
    // Deep copy of a channel
    channel deep_copy_channel(const channel &source)
    {
        channel copy;

        copy.topic_path = strdup(source.topic_path); // Deep copy for char *
        copy.state = source.state;
        copy.function = source.function;

        return copy;
    }

    // Fonction pour ajouter un élément vide au tableau
    void add_channel_memory()
    {
        this->nb_channels++;

        // If no element in the array
        if (this->nb_channels - 1 == 0)
        {
            if (DEBUG_FLOKER_LIB)
            {
                Serial.println("\nSubscribe to a new channel, this is the first one !");
                Serial.println("Let's alloc the memory.");
            }

            this->channels = (channel *)calloc(this->nb_channels, sizeof(channel));
        }

        else
        {
            if (DEBUG_FLOKER_LIB)
            {
                Serial.println("\nSubscribe to a new channel, this the seconde one or more !");
                Serial.println("Let's alloc the memory.");
            }

            channel *new_channels = (channel *)calloc(this->nb_channels, sizeof(channel));

            if (DEBUG_FLOKER_LIB)
            {
                Serial.println("New channels adress: " + String((unsigned long)new_channels));
                Serial.println("Current channels adress: " + String((unsigned long)this->channels));
            }

            for (unsigned short k = 0; k < this->nb_channels - 1; k++)
                new_channels[k] = deep_copy_channel(this->channels[k]);

            free(this->channels);
            this->channels = new_channels;

            if (DEBUG_FLOKER_LIB)
                Serial.println("The deep copy is done.");
        }
        Serial.println("The new current channels adress: " + String((unsigned long)this->channels));
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
    void subscribed_channels_handle()
    {
        for (byte k = 0; k < this->nb_channels; k++)
        {
            if (DEBUG_FLOKER_LIB)
            {
                Serial.println();
                Serial.print("Topic path: ");
                Serial.println(this->channels[k].topic_path);
            }

            int http_code;
            bool read_success = this->read(&http_code, &FLOKER_DATA, this->channels[k].topic_path, false);

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
                if (String(this->channels[k].state) != FLOKER_DATA)
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

    // Check connection interval and update if needed
    static void check_connection_interval()
    {
        global_connection_update_interval = FLOKER_DATA.toInt();
    }

    // Handle the connection polling
    void connection_polling_handle()
    {
        if (this->is_connected_polling && (millis() - this->last_connection_update > global_connection_update_interval || !this->static_information_pushed))
        {
            this->last_connection_update = millis();
            this->write(this->connection_state_topic_path, "connected", false);

            if (!this->static_information_pushed)
            {
                this->write(this->connection_type_topic_path, this->device_type, false);
                this->write(this->connection_version_topic_path, FLOLIB_FLOKER_VERSION, false);
                this->write(this->connection_ip_topic_path, this->ip, false);
                this->static_information_pushed = true;
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
        String device_path = String(""))
    {
        this->ssid = ssid;
        this->password = password;
        this->request_type = secure_connection ? String(HTTPS_REQUEST) : String(HTTP_REQUEST);
        this->server = server;
        if (port == 0)
            this->port = secure_connection ? HTTPS_PORT : HTTP_PORT;
        else
            this->port = port;
        this->root_path = root_path;
        this->token = token;
        this->device_path = device_path;
    }

    // Choose a custom server port
    void set_port(unsigned short port)
    {
        this->port = port;
    }

    // Set connection polling update
    void set_connection_polling(
        String no_default_device_path = String(""),
        String device_type = String(""),
        String start_connection_path = DEFAULT_START_POLLING_PATH,
        String state_connection_path = DEFAULT_STATE_POLLING_PATH,
        String state_interval_path = DEFAULT_INTERVAL_POLLING_PATH,
        String state_type_path = DEFAULT_TYPE_POLLING_PATH,
        String state_version_path = DEFAULT_VERSION_POLLING_PATH,
        String state_ip_path = DEFAULT_IP_POLLING_PATH)
    {
        this->is_connected_polling = true;

        // Create base path
        String base_path = start_connection_path;
        if (no_default_device_path != String(""))
            base_path += no_default_device_path;
        else if (this->device_path != String(""))
            base_path += this->device_path;

        // State topic
        this->connection_state_topic_path = base_path + state_connection_path;

        // Interval topic
        this->connection_interval_topic_path = base_path + state_interval_path;

        // Device type topic
        this->connection_type_topic_path = base_path + state_type_path;
        if (device_type != String(""))
            this->device_type = device_type;

        // Version topic
        this->connection_version_topic_path = base_path + state_version_path;

        // IP
        this->connection_ip_topic_path = base_path + state_ip_path;
    }

    // Start the WiFi connection
    void begin()
    {
        // Init Serial
        if (DEBUG_FLOKER_LIB && !Serial)
        {
            Serial.begin(DEFAULT_SERIAL_BAUDRATE);
        }

        // Init connection polling channel
        if (this->is_connected_polling)
            this->subscribe(this->connection_interval_topic_path, check_connection_interval, false);

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
        this->ip = WiFi.localIP().toString();
        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("");
            Serial.print("Connection is established ! Your ip is: ");
            Serial.println(this->ip);
        }
    }

    // Create channel callback function link
    void subscribe(String topic_path, void (*function)(), bool autocomplete_topic = true)
    {
        this->add_channel_memory();

        // Patern device path is set
        if (autocomplete_topic && this->device_path != String(""))
        {
            topic_path = DEFAULT_START_IOT_PATH + this->device_path + topic_path;
        }
        this->channels[this->nb_channels - 1] = channel{
            strdup(topic_path.c_str()),
            String("default value"),
            (*function)};
    }

    // Read a topic
    bool read(int *http_code, String *get_data, String topic_path, bool autocomplete_topic = true)
    {
        bool success = false;
        // Patern device path is set
        if (autocomplete_topic && this->device_path != String(""))
        {
            topic_path = DEFAULT_START_IOT_PATH + this->device_path + topic_path;
        }
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
            subscribed_channels_handle();
        }
    }
};