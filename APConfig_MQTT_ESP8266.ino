#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#ifdef ESP32
#include <SPIFFS.h>
#endif
//needed for library
#include <DNSServer.h>
//#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);
#define TOKEN "demoDimmer"  //deviceName-->demoSwitch-->demo@hotmail.com
const char* TELEMETRY = "v1/devices/me/telemetry";
static const char *fingerprint PROGMEM = "69 E5 FE 17 2A 13 9C 7C 98 94 CA E0 B0 A6 CB 68 66 6C CB 77";
const long interval = 20000;  //millisecond
unsigned long previousMillis = 0;

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "mqtt.thingcontrol.io";
char mqtt_port[6]  = "8883";
char api_token[32] = "demoDimmer";

//default custom static IP
char static_ip[16] = "10.0.1.56";
char static_gw[16] = "10.0.1.1";
char static_sn[16] = "255.255.255.0";

//flag for saving data
bool shouldSaveConfig = true;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
  WiFiManager wm;
void setupSpiffs() {
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(api_token, json["api_token"]);

          // if(json["ip"]) {
          //   Serial.println("setting custom ip from config");
          //   strcpy(static_ip, json["ip"]);
          //   strcpy(static_gw, json["gateway"]);
          //   strcpy(static_sn, json["subnet"]);
          //   Serial.println(static_ip);
          // } else {
          //   Serial.println("no custom ip in config");
          // }

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}
// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("  Topic: ");
  Serial.println(topic);
  Serial.print("  Message: ");
  Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  //  if (methodName.equals("getValue")) {
  //    // Reply with GPIO status
  //    String responseTopic = String(topic);
  //    responseTopic.replace("request", "response");
  //      Serial.print("\tPublish to ");
  //      Serial.println(responseTopic.c_str());
  //
  //      Serial.println(get_gpio_status().c_str());
  //    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
  //  } else if (methodName.equals("setValue")) {
  //    // Update GPIO status and reply
  //    set_gpio_status(data["params"]["pin"], data["params"]["enabled"]);
  //    String responseTopic = String(topic);
  //    responseTopic.replace("request", "response");
  //    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
  //      Serial.println("  Publish to v1/devices/me/attributes");
  //      Serial.println(get_gpio_status().c_str());
  //    client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
  //  }
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  setupSpiffs();

  setWiFi();

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
}

void setWiFi() {
  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around


  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  // setup custom parameters
  //
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_api_token("api", "api token", api_token, 32);

  //add all your parameters here
  //  wm.addParameter(&custom_mqtt_server);
  //  wm.addParameter(&custom_mqtt_port);
  //  wm.addParameter(&custom_api_token);

  // set static ip
  // IPAddress _ip,_gw,_sn;
  // _ip.fromString(static_ip);
  // _gw.fromString(static_gw);
  // _sn.fromString(static_sn);
  // wm.setSTAStaticIPConfig(_ip, _gw, _sn);

  //reset settings - wipe credentials for testing
  //  wm.resetSettings();

  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name
  //here  "AutoConnectAP" if empty will auto generate basedcon chipid, if password is blank it will be anonymous
  //and goes into a blocking loop awaiting configuration
  if (!wm.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // if we still have not connected restart and try all over again
    ESP.restart();
    delay(5000);
  }

  // always start configportal for a little while
  wm.setConfigPortalTimeout(60);
  wm.startConfigPortal();

  //if you get here you have connected to the WiFi
  wifiClient.setFingerprint(fingerprint);

  Serial.println("connected...yeey :)");
  client.setServer("mqtt.thingcontrol.io", 8883 );
  client.setCallback(on_message);
  if ( client.connect("WROOM02", TOKEN, NULL) ) {
    Serial.println( "[DONE]" );
    client.subscribe("v1/devices/me/rpc/request/+");

  } else {
    Serial.print( "[FAILED] [ rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying in 5 seconds]" );
    // Wait 5 seconds before retrying
    delay( 5000 );
  }
  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(api_token, custom_api_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"]   = mqtt_port;
    json["api_token"]   = api_token;

    json["ip"]          = WiFi.localIP().toString();
    json["gateway"]     = WiFi.gatewayIP().toString();
    json["subnet"]      = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if ( !client.connected() ) {
    if ( client.connect("WROOM02_", TOKEN, NULL) ) {
      Serial.println( "[DONE]" );

    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }

  client.loop();
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
      Serial.println(client.connected());
    //  Serial.print("Temperature: ");

    // Convert raw temperature in F to Celsius degrees
    double temp =  1.8;
    String json = "";
    json = "{\"temp\":";
    json.concat(String(temp));
    json.concat("}");
    Serial.print(temp);
    Serial.println(" C");
    Serial.println(json.c_str());
    int responseCode = client.publish(TELEMETRY, json.c_str());
    Serial.print("responseCode:");
    Serial.println(responseCode);
    //  delay(5000);
    previousMillis = currentMillis;

  }



}
