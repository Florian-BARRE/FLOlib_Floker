#define ESP8266_ENABLED

#include <Arduino.h>
#include <ArduinoJson.h>

#define FLOKER_DEBUG_MODE
#define FLOKER_DEBUG_LEVEL 100

#define FLOLIB_FLOKER_VERSION "3.0.1"

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

#define DEFAULT_UNDER_REQUEST_SIZE 512
#define DEFAULT_UNDER_RESPONSE_SIZE 512

#define DEFAULT_SERIAL_BAUDRATE 115200

static unsigned long global_connection_update_interval = 10000;

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

#pragma region Json Tools
class Json_tools
{
public:
    static void merge_json(JsonObject dest, JsonObject src);

    static DynamicJsonDocument make_task_json(String type, String topic, DynamicJsonDocument *params = NULL);

    static DynamicJsonDocument make_read_json(String topic);
    static DynamicJsonDocument make_write_json(String topic, String state);
};
#pragma endregion

#pragma region Channel
class Channel
{
public:
    // Attributes
    String topic_path;
    String state;
    void (*function)(String data);

    // Constructor
    Channel(String topic_path, void (*function)(String data), String state = String("default value"));

    // Alloc memory to add a new channel to the pointer
    static Channel deep_copy(Channel chennl_to_copy);
    static Channel *push_channel_to_array(Channel *old_ptr, Channel channel_to_push, unsigned short new_size);
};
#pragma endregion

#pragma region Server
class Server_Manager
{
private:
    // WiFi
    const char *ssid;
    const char *password;

    // Server connection
    String request_type;
    String server;
    String root_path;
    String token;

    // WiFi and HTTP client object
    WiFiClient wifi_client;
    HTTPClient http_client;

    // Tools
    inline String start_url() { return this->request_type + this->server + String(":") + String(this->port) + this->root_path; }
    String make_uri(String topic = String(""), String data_to_write = String(""));
    bool get_request(String uri, String *response, bool force_request = false);
    bool post_request(String uri, String request, String *response, bool force_request = false);

public:
    // Attributes
    String device_type = FLOKER_DEVICE_TYPE;
    String ip;
    unsigned short port;

    // Auto pathing device
    String device_path;

    // Constructor
    Server_Manager(
        const char *ssid,
        const char *password,
        String request_type,
        String server,
        unsigned short port,
        String root_path,
        String token,
        String device_path = String(""));

    // Start the server connection
    void begin();

    // Interact with the server
    bool read(String topic_path, String *get_data, bool force = false);
    bool write(String topic_path, String data_to_write, bool force = false);
    bool multi_tasks(String request, String *response, bool force = false);
};
#pragma endregion

#pragma region Software_polling
class Software_polling
{
private:
    // Connected polling and static information
    bool static_information_pushed = false;
    unsigned long last_connection_update = 0;

    String connection_state_topic_path;
    String connection_interval_topic_path;
    String connection_type_topic_path;
    String connection_version_topic_path;
    String connection_ip_topic_path;

    // Connection interval
    static void update_polling_interval(String data)
    {
        global_connection_update_interval = data.toInt();
    }

public:
    Software_polling(
        String state_topic_path,
        String interval_topic_path,
        String type_topic_path,
        String version_topic_path,
        String ip_topic_path);
    Channel create_interval_channel();
    void handle(Server_Manager *server_ptr);
};
#pragma endregion

#pragma region Floker
class Floker
{
private:
    // Tools pointers
    Server_Manager *server_ptr;
    Channel *channels_ptr;
    Software_polling *software_polling_ptr;

    // Attributes
    bool enable_software_polling = false;

    // Tools
    String get_path(String path, bool autocomplete = true);

    // Handle functions
    bool enable_multi_handle = true;

    void subscribed_channels_handle();
    void classic_subscribed_channels_handle();
    void multi_subscribed_channels_handle();

public:
    // Attributes
    unsigned short nb_channels = 0;

    // Constructor
    Floker(const char *ssid,
           const char *password,
           bool secure_connection,
           String server,
           String root_path,
           String token,
           String device_path = String(""));

    // Class properties
    void set_port(unsigned short port);

    void set_multi_handle(bool enable_multi_handle);

    // Set the polling connection(connected state and static information)
    void set_connection_polling(
        String no_default_device_path = String(""),
        String device_type = String(""),
        String start_connection_path = DEFAULT_START_POLLING_PATH,
        String state_connection_path = DEFAULT_STATE_POLLING_PATH,
        String state_interval_path = DEFAULT_INTERVAL_POLLING_PATH,
        String state_type_path = DEFAULT_TYPE_POLLING_PATH,
        String state_version_path = DEFAULT_VERSION_POLLING_PATH,
        String state_ip_path = DEFAULT_IP_POLLING_PATH);

    // Methods
    void begin();
    void handle();

    // Interact with the high level interaction with the server
    void subscribe(String topic_path, void (*function)(String data), bool autocomplete_topic = true);

    bool read(String topic_path, String *get_data, bool autocomplete_topic = true, bool force_request = false);
    bool write(String topic_path, String data_to_write, bool autocomplete_topic = true, bool force_request = false);
    bool multi_tasks(DynamicJsonDocument request, DynamicJsonDocument *response, bool force_request = false);
};
#pragma endregion

#pragma region Debug
class Debug
{
public:
    // Tools
    static bool check_auth_to_print(word level = 1);
    static String end_printed_function();

    // Channel
    static void channel_deep_copy(Channel *channel, word level = 1);

    static void channel_push_channel_to_array_alloc_memory(Channel *channel, unsigned short new_size, word level = 1);
    static void channel_push_channel_to_array_start_deep_copy(word level = 1);
    static void channel_push_channel_to_array_end_deep_copy(word level = 1);
    static void channel_push_channel_to_array_show_adress(Channel *old_ptr, Channel *new_ptr, word level = 1);

    // Server
    static void server_make_uri(String *topic, String *data_to_write, String *uri, word level = 1);

    static void server_request_open_connection(String type, String *uri, word level = 1);
    static void server_request_analyse_http_code(HTTPClient *client, int *code, String *reponse, word level = 1);
    static void server_request_close_connection(String type, word level = 1);

    static void server_begin_wifi_connection(String *ssid, word level = 1);
    static void server_begin_wifi_connection_waiting(word level = 1);
    static void server_begin_wifi_connection_done(String *ip, word level = 1);

    static void server_request(String type, String *topic_path_or_request, String *data, bool *force, word level = 1);
    /*
    // Software polling
    static void software_polling_handle(Software_polling *obj);
    static void create_interval_channel(Channel *channel);

    // Floker
    static void floker_get_path(String *path, bool *autocomplete);
    */
};
#pragma endregion
