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
    Debug::channel_deep_copy(&channel_to_copy);
    return Channel(channel_to_copy.topic_path, channel_to_copy.function, channel_to_copy.state);
}

Channel *Channel::push_channel_to_array(Channel *old_ptr, Channel channel_to_push, unsigned short new_size)
{
    Channel *new_ptr;
    Debug::channel_push_channel_to_array_alloc_memory(&channel_to_push, new_size);

    // If no element in the array
    if (new_size - 1 == 0)
        new_ptr = (Channel *)calloc(new_size, sizeof(Channel));
    else
    {
        new_ptr = (Channel *)calloc(new_size, sizeof(Channel));

        Debug::channel_push_channel_to_array_start_deep_copy();
        for (unsigned short k = 0; k < new_size - 1; k++)
            new_ptr[k] = Channel::deep_copy(old_ptr[k]);

        free(old_ptr);
        Debug::channel_push_channel_to_array_end_deep_copy();
    }
    Debug::channel_push_channel_to_array_show_adress(old_ptr, new_ptr);

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
    Debug::server_make_uri(&topic, &data_to_write, &uri);
    return uri;
}

bool Server_Manager::get_request(String uri, String *response, bool force_request)
{
    // Open the connection
    Debug::server_request_open_connection("GET", &uri);
    this->http_client.begin(this->wifi_client, uri);

    bool success = false;
    while (!success)
    {
        // Send the request
        int http_code = this->http_client.GET();

        // Get the request response
        *response = this->http_client.getString();
        Debug::server_request_analyse_http_code(&this->http_client, &http_code, response);
        success = (http_code == 200);

        // Quit the loop is the force request option is not asked
        if (!force_request)
            break;
    }

    // Close the connection
    Debug::server_request_close_connection("GET");
    this->http_client.end();

    return success;
}

bool Server_Manager::post_request(String uri, String request, String *response, bool force_request)
{
    // Open the connection
    Debug::server_request_open_connection("POST", &uri);
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

        Debug::server_request_analyse_http_code(&this->http_client, &http_code, response);

        success = (http_code == 200);

        // Quit the loop is the force request option is not asked
        if (!force_request)
            break;
    }

    // Close the connection
    Debug::server_request_close_connection("POST");
    this->http_client.end();
    return success;
}

// Public method(s)
void Server_Manager::begin()
{
    // Init WiFi connection
    WiFi.begin(this->ssid, this->password);
    String str_ssid = String(this->ssid);
    Debug::server_begin_wifi_connection(&str_ssid);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Debug::server_begin_wifi_connection_waiting();
    }
    this->ip = WiFi.localIP().toString();
    Debug::server_begin_wifi_connection_done(&this->ip);
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
        // if (FLOKER_DEBUG_MODE)
        //{
        //     Serial.println();
        //     Serial.print("Topic path: ");
        //     Serial.println(this->channels_ptr[k].topic_path);
        // }

        String response;

        if (this->read(this->channels_ptr[k].topic_path, &response, false))
        {
            // if (FLOKER_DEBUG_MODE)
            //{
            //     Serial.print("State ------> ");
            //    Serial.println(response);
            //    Serial.print("Old state --> ");
            //    Serial.println(this->channels_ptr[k].state);
            //}

            // Check if the state have changed
            if (String(this->channels_ptr[k].state) != response)
            {
                // if (FLOKER_DEBUG_MODE)
                // Serial.println("The state have changed, let's execute the callback function !");

                this->channels_ptr[k].function(response);
                this->channels_ptr[k].state = response;
            }
            // else if (FLOKER_DEBUG_MODE)

            // Serial.println("The state have not changed.");
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
            // if (FLOKER_DEBUG_MODE)
            //    Serial.println("\nTopic path: " + String(this->channels_ptr[k].topic_path));

            JsonObject under_request_response = json_response_array[k];
            String state = under_request_response["data"].as<String>();
            // if (FLOKER_DEBUG_MODE)
            //    Serial.println("State ------> " + String(state) + "\nOld state --> " + String(this->channels_ptr[k].state));

            // Check if the state have changed
            if (this->channels_ptr[k].state != state)
            {
                // if (FLOKER_DEBUG_MODE)
                //     Serial.println("The state have changed, let's execute the callback function !");

                this->channels_ptr[k].function(state);
                this->channels_ptr[k].state = state;
            }
            // else if (FLOKER_DEBUG_MODE)
            //    Serial.println("The state have not changed.");
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
    // if (DEBUG_FLOKER_LIB && !Serial)
    //    Serial.begin(DEFAULT_SERIAL_BAUDRATE);

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
    topic_path = this->get_path(topic_path, autocomplete_topic);
    return this->server_ptr->write(topic_path, data_to_write, force_request);
}

bool Floker::multi_tasks(DynamicJsonDocument request, DynamicJsonDocument *response, bool force_request)
{
    String str_response;
    String str_request;
    serializeJson(request, str_request);
    bool success = this->server_ptr->multi_tasks(str_request, &str_response, force_request);

    if (success)
    {
        // Get the deserialize request's response
        DeserializationError parse_error = deserializeJson(*response, str_response);

        // if (DEBUG_FLOKER_LIB && parse_error)
        //    Serial.println("Parse response failed ! Error code: " + String(parse_error.c_str()));
    }

    return success;
}
#pragma endregion

#pragma region Debug
bool Debug::check_auth_to_print(word level)
{
    bool auth = false;
#ifdef FLOKER_DEBUG_MODE
    if (FLOKER_DEBUG_LEVEL >= level)
        auth = true;
#endif
    return auth;
}

String Debug::end_printed_function()
{
    return String("#- End of function !");
}

// Channel
void Debug::channel_deep_copy(Channel *channel, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- Channel deep copy called !\n";
    msg += "#-- Topic path: " + channel->topic_path + "\n";
    msg += "#-- State: " + channel->state;
    msg += Debug::end_printed_function();

    Serial.println(msg);
}

void Debug::channel_push_channel_to_array_alloc_memory(Channel *channel, unsigned short new_size, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- Push channel to array was called !\n";

    msg += "#-- The channel topic is : " + channel->topic_path + "\n";

    if (new_size - 1 == 0)
        msg += "#-- This is the first channel of the array.\n";
    else
        msg += "#-- This is " + String(new_size) + " channel of the array.\n";

    msg += "#--- Let's alloc the memory.";
    Serial.println(msg);
}

void Debug::channel_push_channel_to_array_start_deep_copy(word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    Serial.println("#--- Start deep copy...");
}

void Debug::channel_push_channel_to_array_end_deep_copy(word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    Serial.println("#--- The deep copy is ended.");
}

void Debug::channel_push_channel_to_array_show_adress(Channel *old_ptr, Channel *new_ptr, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "#--- The old channels array adress is : " + String((unsigned long)old_ptr) + "\n";
    msg += "#--- The new channels array adress is : " + String((unsigned long)new_ptr) + "\n";
    msg += Debug::end_printed_function();
    Serial.println(msg);
}

// Server
void Debug::server_make_uri(String *topic, String *data_to_write, String *uri, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- Make URI called !\n";

    if (*topic != String("") && *data_to_write != String(""))
        msg += "#-- Create URI for 'write' request !\n";
    else if (*topic != String(""))
        msg += "#-- Create URI for 'read' request !\n";
    else
        msg += "#-- Create URI for 'multi' request !\n";

    msg += "#--- URI result: " + *uri + "\n";
    msg += Debug::end_printed_function();
    Serial.println(msg);
}

void Debug::server_request_open_connection(String type, String *uri, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- Open a " + type + " connection...\n";
    msg += "#-- URI: " + *uri + "\n";
    Serial.println(msg);
}

void Debug::server_request_analyse_http_code(HTTPClient *client, int *code, String *reponse, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- Analyse HTTP code.\n";
    msg += "#-- HTTP code: " + String(*code) + "\n";

    if (*code == 200)
        msg += "#-- The request was a success !\n";
    else
    {
        msg += "#-- The request was a failure !\n";
        msg += "#--- The error is : " + client->errorToString(*code) + "\n";
    }
    msg += "#-- The reponse is : " + *reponse + "\n";
    Serial.println(msg);
}

void Debug::server_request_close_connection(String type, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- Close a " + type + " connection.\n";
    msg += Debug::end_printed_function();
    Serial.println(msg);
}

void Debug::server_begin_wifi_connection(String *ssid, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- Try to connect to" + *ssid + "\n";
    Serial.println(msg);
}

void Debug::server_begin_wifi_connection_waiting(word level)
{
    Serial.print(".");
}

void Debug::server_begin_wifi_connection_done(String *ip, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- The WiFi connction is done !\n";
    msg += "#-- The device ip is " + *ip + "\n";
    msg += Debug::end_printed_function();
    Serial.println(msg);
}

void Debug::server_request(String type, String *topic_path_or_request, String *data, bool *force, word level)
{
    if (!Debug::check_auth_to_print(level))
        return;

    String msg = "\n#- Open a " + type + " request.\n";

    if (*force)
        msg += "#-- The request is in force mode !\n";

    msg += "#-- The concerned topic is " + *topic_path_or_request + "\n";
    if (type == "write")
        msg += "#-- The data to write is : " + *data + "\n";
    else if (type == "multi")
        msg += "#-- The request content is : " + *data + "\n";

    msg += Debug::end_printed_function();
    Serial.println(msg);
}
#pragma endregion