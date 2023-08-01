#include "FLOlib_floker.h"

#pragma region Json_tools
void Json_tools::merge_json(JsonObject dest, JsonObject src)
{
    for (JsonPair kvp : src)
        dest[kvp.key()] = kvp.value();
}

DynamicJsonDocument Json_tools::make_task_json(String type, String topic, DynamicJsonDocument *params)
{
    DynamicJsonDocument json(DEFAULT_UNDER_REQUEST_SIZE);

    json["type"] = type;
    json["topic"] = topic;

    if (params != NULL)
        merge_json(json.as<JsonObject>(), params->as<JsonObject>());

    return json;
}

DynamicJsonDocument Json_tools::make_read_json(String topic)
{
    DynamicJsonDocument json_params(256);
    json_params["parse"] = "state";
    return make_task_json("read", topic, &json_params);
}

DynamicJsonDocument Json_tools::make_write_json(String topic, String state)
{
    DynamicJsonDocument json_params(256);
    json_params["state"] = state;
    return make_task_json("write", topic, &json_params);
}

#pragma endregion

#pragma region Channel
// Constructor
Channel::Channel(String topic_path, void (*function)(String data), String state)
{
    this->topic_path = topic_path;
    this->function = function;
    this->state = state;
}

// Static: Method(s)
Channel Channel::deep_copy(Channel channel_to_copy)
{
    return Channel(channel_to_copy.topic_path, channel_to_copy.function, channel_to_copy.state);
}

Channel *Channel::push_channel_to_array(Channel *old_ptr, Channel channel_to_push, unsigned short new_size)
{
    Channel *new_ptr;

    // If no element in the array
    if (new_size - 1 == 0)
    {
        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("\nSubscribe to a new channel, this is the first one !");
            Serial.println("Let's alloc the memory.");
        }

        new_ptr = (Channel *)calloc(new_size, sizeof(Channel));
    }
    else
    {
        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("\nSubscribe to a new channel, this the seconde one or more !");
            Serial.println("Let's alloc the memory.");
        }

        new_ptr = (Channel *)calloc(new_size, sizeof(Channel));

        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("New channels adress: " + String((unsigned long)new_ptr));
            Serial.println("Current channels adress: " + String((unsigned long)old_ptr));
        }

        for (unsigned short k = 0; k < new_size - 1; k++)
            new_ptr[k] = Channel::deep_copy(old_ptr[k]);

        free(old_ptr);

        if (DEBUG_FLOKER_LIB)
            Serial.println("The deep copy is done.");
    }
    Serial.println("The new current channels adress: " + String((unsigned long)new_ptr));

    // Add the new channel to the new_ptr
    new_ptr[new_size - 1] = Channel::deep_copy(channel_to_push);

    return new_ptr;
}
#pragma endregion

// Server
#pragma region Server
// Constructor
Server_Manager::Server_Manager(
    const char *ssid,
    const char *password,
    String request_type,
    String server,
    unsigned short port,
    String root_path,
    String token,
    String device_path)
{
    this->ssid = ssid;
    this->password = password;
    this->request_type = request_type;
    this->server = server;
    this->port = port;
    this->root_path = root_path;
    this->token = token;
    this->device_path = device_path;
}

// Private method(s)
String Server_Manager::make_uri(String topic, String data_to_write)
{
    String uri = this->start_url();

    // Write Mode
    if (topic != String("") && data_to_write != String(""))
    {
        uri += String("write?");
        uri += String("token=") + this->token;
        uri += String("&topic=") + topic;
        uri += String("&state=") + data_to_write;
    }

    // Read Mode
    else if (topic != String(""))
    {
        uri += String("read?");
        uri += String("token=") + this->token;
        uri += String("&topic=") + topic;
        uri += String("&parse=state");
    }
    // Multi action request mode
    else
    {
        uri += String("multi?");
        uri += String("token=") + this->token;
        uri += String("&parse=response");
    }

    return uri;
}

bool Server_Manager::get_request(String uri, String *response, bool force_request)
{
    // Open the connection
    if (DEBUG_FLOKER_LIB)
        Serial.println("Open get request:\nuri: " + uri);

    this->http_client.begin(this->wifi_client, uri);

    bool success = false;
    while (!success)
    {
        // Send the request
        int http_code = this->http_client.GET();

        // Get the request response
        *response = this->http_client.getString();

        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("Response code: " + String(http_code));
            if (http_code < 0)
                Serial.println("The request can't be sent: " + String(this->http_client.errorToString(http_code)));
            else if (http_code != 200)
                Serial.println("The request was a failure !\nThe error response is :\n" + *response);
            else
                Serial.println("The request was a success, the data is: \n" + *response);
        }

        success = (http_code == 200);

        // Quit the loop is the force request option is not asked
        if (!force_request)
            break;
    }

    // Close the connection
    if (DEBUG_FLOKER_LIB)
        Serial.println("End of the get request close the connection.");

    this->http_client.end();

    return success;
}

bool Server_Manager::post_request(String uri, String request, String *response, bool force_request)
{
    // Open the connection
    if (DEBUG_FLOKER_LIB)
        Serial.println("Open post request:\nuri: " + uri);

    this->http_client.begin(this->wifi_client, uri);
    this->http_client.addHeader("Content-Type", "application/json");

    bool success = false;
    while (!success)
    {
        // TODO: il faut récupérer les status 1 à 1 des sous requees pour voir si elles sont marchées
        // Send the request
        int http_code = this->http_client.POST(request);

        // Get the request response
        *response = this->http_client.getString();

        if (DEBUG_FLOKER_LIB)
        {
            Serial.println("Response code: " + String(http_code));
            if (http_code < 0)
                Serial.println("The request can't be sent: " + String(this->http_client.errorToString(http_code)));
            else if (http_code != 200)
                Serial.println("The request was a failure !\nThe error response is :\n" + *response);
            else
                Serial.println("The request was a success, the data is: \n" + *response);
        }

        success = (http_code == 200);

        // Quit the loop is the force request option is not asked
        if (!force_request)
            break;
    }

    // Close the connection
    if (DEBUG_FLOKER_LIB)
        Serial.println("End of the post request close the connection.");

    this->http_client.end();

    return success;
}

// Public method(s)
void Server_Manager::begin()
{
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

bool Server_Manager::read(String topic_path, String *get_data, bool force)
{
    String uri = this->make_uri(topic_path);
    return get_request(uri, get_data, force);
}

bool Server_Manager::write(String topic_path, String data_to_write, bool force)
{
    String uri = this->make_uri(topic_path, data_to_write);
    String response;
    return get_request(uri, &response, force);
}

bool Server_Manager::multi_tasks(String request, String *response, bool force)
{
    String uri = this->make_uri();
    return post_request(uri, request, response, force);
}

#pragma endregion

// Software_polling
#pragma region Software_polling
// Constructor
Software_polling::Software_polling(
    String connection_state_topic_path,
    String connection_interval_topic_path,
    String connection_type_topic_path,
    String connection_version_topic_path,
    String connection_ip_topic_path)
{
    this->connection_state_topic_path = connection_state_topic_path;
    this->connection_interval_topic_path = connection_interval_topic_path;
    this->connection_type_topic_path = connection_type_topic_path;
    this->connection_version_topic_path = connection_version_topic_path;
    this->connection_ip_topic_path = connection_ip_topic_path;
}
// Public: Begin and Handle functions
void Software_polling::handle(Server_Manager *server_ptr)
{
    // Execute all request in force mode
    if (millis() - this->last_connection_update > global_connection_update_interval || !this->static_information_pushed)
    {
        this->last_connection_update = millis();
        server_ptr->write(this->connection_state_topic_path, "connected", true);

        if (!this->static_information_pushed)
        {
            // Update the refresh interval for the polling update
            String interval;
            server_ptr->read(this->connection_interval_topic_path, &interval, true);
            global_connection_update_interval = interval.toInt();
            // Send static device informations
            server_ptr->write(this->connection_type_topic_path, server_ptr->device_type, true);
            server_ptr->write(this->connection_version_topic_path, FLOLIB_FLOKER_VERSION, true);
            server_ptr->write(this->connection_ip_topic_path, server_ptr->ip, true);
            this->static_information_pushed = true;
        }
    }
}

Channel Software_polling::create_interval_channel()
{
    return Channel(
        this->connection_interval_topic_path,
        this->update_polling_interval,
        String(global_connection_update_interval));
}
#pragma endregion

#pragma region Floker
// Constructor
Floker::Floker(
    const char *ssid,
    const char *password,
    bool secure_connection,
    String server,
    String root_path,
    String token,
    String device_path)
{
    this->server_ptr = new Server_Manager(
        ssid, password,
        secure_connection ? String(HTTPS_REQUEST) : String(HTTP_REQUEST),
        server,
        secure_connection ? HTTPS_PORT : HTTP_PORT,
        root_path,
        token,
        device_path);
}

// Private method(s)
String Floker::get_path(String path, bool autocomplete)
{
    // Patern device path is set
    if (autocomplete && this->server_ptr->device_path != String(""))
        path = DEFAULT_START_IOT_PATH + this->server_ptr->device_path + path;
    return path;
}

void Floker::classic_subscribed_channels_handle()
{
    for (unsigned short k = 0; k < this->nb_channels; k++)
    {
        if (DEBUG_FLOKER_LIB)
        {
            Serial.println();
            Serial.print("Topic path: ");
            Serial.println(this->channels_ptr[k].topic_path);
        }

        String response;

        if (this->read(this->channels_ptr[k].topic_path, &response, false))
        {
            if (DEBUG_FLOKER_LIB)
            {
                Serial.print("State ------> ");
                Serial.println(response);
                Serial.print("Old state --> ");
                Serial.println(this->channels_ptr[k].state);
            }

            // Check if the state have changed
            if (String(this->channels_ptr[k].state) != response)
            {
                if (DEBUG_FLOKER_LIB)
                {
                    Serial.println("The state have changed, let's execute the callback function !");
                }
                this->channels_ptr[k].function(response);
                this->channels_ptr[k].state = response;
            }
            else if (DEBUG_FLOKER_LIB)
            {
                Serial.println("The state have not changed.");
            }
        }
    }
}

void Floker::multi_subscribed_channels_handle()
{
    // Create Json request
    DynamicJsonDocument json_request(this->nb_channels * DEFAULT_UNDER_REQUEST_SIZE);
    JsonArray json_under_request_array = json_request.to<JsonArray>();

    // All request here are "read" request
    for (unsigned short k = 0; k < this->nb_channels; k++)
        json_under_request_array.add(Json_tools::make_read_json(this->channels_ptr[k].topic_path));

    // Send the Json request and get the Json response
    DynamicJsonDocument json_response(this->nb_channels * DEFAULT_UNDER_RESPONSE_SIZE);

    if (this->multi_tasks(json_request, &json_response))
    {

        // Execute all callback function if it is necessary
        JsonArray json_response_array = json_response.as<JsonArray>();
        for (unsigned short k = 0; k < this->nb_channels; k++)
        {
            if (DEBUG_FLOKER_LIB)
                Serial.println("\nTopic path: " + String(this->channels_ptr[k].topic_path));

            JsonObject under_request_response = json_response_array[k];
            String state = under_request_response["data"].as<String>();
            if (DEBUG_FLOKER_LIB)
                Serial.println("State ------> " + String(state) + "\nOld state --> " + String(this->channels_ptr[k].state));

            // Check if the state have changed
            if (this->channels_ptr[k].state != state)
            {
                if (DEBUG_FLOKER_LIB)
                    Serial.println("The state have changed, let's execute the callback function !");

                this->channels_ptr[k].function(state);
                this->channels_ptr[k].state = state;
            }
            else if (DEBUG_FLOKER_LIB)
                Serial.println("The state have not changed.");
        }
    }

    Serial.println(" json_under_request: ");
    serializeJson(json_request, Serial);
    Serial.println("\n json_response: ");
    serializeJson(json_response, Serial);

    Serial.println(" ");
}

void Floker::subscribed_channels_handle()
{
    if (this->enable_multi_handle)
        this->multi_subscribed_channels_handle();
    else
        this->classic_subscribed_channels_handle();
}

// Public method(s)
void Floker::set_port(unsigned short port)
{
    this->server_ptr->port = port;
}

void Floker::set_multi_handle(bool enable_multi_handle)
{
    this->enable_multi_handle = enable_multi_handle;
}

void Floker::set_connection_polling(
    String no_default_device_path,
    String device_type,
    String start_connection_path,
    String state_connection_path,
    String state_interval_path,
    String state_type_path,
    String state_version_path,
    String state_ip_path)
{
    this->enable_software_polling = true;

    // Create base path
    String base_path = start_connection_path;
    if (no_default_device_path != String(""))
        base_path += no_default_device_path;
    else if (this->server_ptr->device_path != String(""))
        base_path += this->server_ptr->device_path;

    // State topic
    String state_topic_path = base_path + state_connection_path;

    // Interval topic
    String interval_topic_path = base_path + state_interval_path;

    // Device type topic
    String type_topic_path = base_path + state_type_path;
    if (device_type != String(""))
        this->server_ptr->device_type = device_type;

    // Version topic
    String version_topic_path = base_path + state_version_path;

    // IP
    String ip_topic_path = base_path + state_ip_path;

    this->software_polling_ptr = new Software_polling(
        state_topic_path,
        interval_topic_path,
        type_topic_path,
        version_topic_path,
        ip_topic_path);
}

void Floker::begin()
{
    // Init Serial
    if (DEBUG_FLOKER_LIB && !Serial)
        Serial.begin(DEFAULT_SERIAL_BAUDRATE);

    // Init connection polling channel
    if (this->enable_software_polling)
    {
        this->nb_channels++;
        this->channels_ptr = Channel::push_channel_to_array(
            this->channels_ptr,
            this->software_polling_ptr->create_interval_channel(),
            this->nb_channels);
    }

    // Init WiFi connection
    this->server_ptr->begin();
}

void Floker::handle()
{
    if (this->enable_software_polling)
        this->software_polling_ptr->handle(this->server_ptr);

    this->subscribed_channels_handle();
}

void Floker::subscribe(String topic_path, void (*function)(String data), bool autocomplete_topic)
{
    this->nb_channels++;
    this->channels_ptr = Channel::push_channel_to_array(
        this->channels_ptr,
        Channel(this->get_path(topic_path, autocomplete_topic), (*function)),
        this->nb_channels);
}

bool Floker::read(String topic_path, String *get_data, bool autocomplete_topic, bool force_request)
{
    topic_path = this->get_path(topic_path, autocomplete_topic);
    return this->server_ptr->read(topic_path, get_data, force_request);
}

bool Floker::write(String topic_path, String data_to_write, bool autocomplete_topic, bool force_request)
{
    toploker::multi_tasks(DynamicJsonDocument request, DynamicJsonDocument * response, bool force_request)
    {
        String str_response;
        String str_request;
        serializeJson(request, str_request);
        bool success = this->server_ptr->multi_tasks(str_request, &str_response, force_request);

        if (success)
        {
            // Get the deserialize request's response
            Deserialiic_path = this->get_path(topic_path, autocomplete_topic);
            return this->server_ptr->write(topic_path, data_to_write, force_request);
        }

        bool FzationError parse_error = deserializeJson(*response, str_response);

        if (DEBUG_FLOKER_LIB && parse_error)
            Serial.println("Parse response failed ! Error code: " + String(parse_error.c_str()));
    }

    return success;
}
#pragma endregion