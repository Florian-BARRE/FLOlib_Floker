#define ESP8266_ENABLED true // Choose your device type
#define ESP32_ENABLED false

#include "FLOlib_floker.h" 

Floker broker(
  /*WiFi ssid*/, 
  /*WiFi password*/,
  false, /*Enable secure connection (http or https */
  /*your server ip or domain name*/, 
  /*your server's port*/, 
  /*your root api path*/, 
  /*your token server access*/,
  /*number of channel you will subscribe*/,
  /*Optionnal: your device path*/
);

// If you want to set a specific port for the connection to the server
//broker.set_port(5000);
        
void update_test(){
  //bool state = (FLOKER_DATA == String("ON") || FLOKER_DATA == String("1"));
  Serial.println("update_test");
  Serial.println(FLOKER_DATA);
}

void CallBack_Function(){
  // this function will be call when "iot$maison$gate" channel state change
  // You can get this value, there is in HTTP_DATA variable
  // Exemple:
  Serial.println(HTTP_DATA);

  // You can write on a channel:
  // 2 cases
  // 1) If you have set a default device path in the constructor you can just set short path
  broker.write("/state", String(FLOKER_DATA)); // The real topic will be 'iot/...device_path.../state'
  // 2) Else you can give the full path 
  broker.write("iot/...device_path.../state", String(FLOKER_DATA));
}

void setup() {
  // If you enable DEBUG_FLOKER_LIB, the Serial will be automatically started on 9600 baudrate
  broker.begin();

  // You can enable connection polling 
  broker.set_connection_polling(
    /*Refresh state interval in ms*/,
    /*Device name (ESP8266 for exemple)*/,
    /*Optional: device path if you don't want to use the one given in the constructor*/
    /*Optional: the start path (default is 'iot/')*/
 );

  // Subscribe to a channel
  // 2 cases
  // 1) If you have set a default device path in the constructor you can just set short path
  broker.subscribe("/listen_topic", CallBack_Function); // The real topic will be 'iot/...device_path.../state'
  // 2) Else you can give the full path 
  broker.subscribe("iot/...device_path.../listen_topic", CallBack_Function);
}

void loop() {
  broker.handle();
}
