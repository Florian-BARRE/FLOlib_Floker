#include "FLOlib_floker.h" 

Floker broker(
  /*WiFi ssid*/, 
  /*WiFi password*/,
  String(/*your server ip or domain name*/), 
  /*your server's port*/, 
  String(/*your root api path*/), 
  String(/*your token server access*/)
);

void CallBack_Function(){
  // this function will be call when "iot$maison$gate" channel state change
  // You can get this value, there is in HTTP_DATA variable
  // Exemple:
  Serial.println(HTTP_DATA);

  // You can write on a channel:
  broker.Write("iot$notification$alert", String(HTTP_DATA));
}
void setup() {
  Serial.println(9600);
  broker.Begin();
  // Add a channel to read
  broker.AddChannel("iot$maison$gate", CallBack_Function);
}

void loop() {
  broker.Loop();
}
