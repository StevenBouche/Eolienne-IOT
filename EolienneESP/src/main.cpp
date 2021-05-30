//************************************************************
// this is a MqttBroker example that uses the painlessMesh library
//
// connect to a another network and relay messages from a MQTT broker to the nodes of the mesh network.
//
// - To send a message to a mesh node, you can publish it to "painlessMesh/to/NNNN" where NNNN equals the nodeId.
// - To broadcast a message to all nodes in the mesh you can publish it to "painlessMesh/to/broadcast".
// - When you publish "getNodes" to "painlessMesh/to/gateway" you receive the mesh topology as JSON
//
// - Every message from the mesh which is sent to the gateway node will be published to "painlessMesh/from/NNNN" where NNNN
//   is the nodeId from which the packet was sent.
//
// - The web server has only 3 pages:
//     ip_address_of_the_bridge      to broadcast messages to all nodes
//     ip_address_of_the_bridge/map  to show the topology of the network
//     ip_address_of_the_bridge/scan to get the topology of the network ( json format )
//
//************************************************************
//#include <Arduino.h>
#include "painlessMesh.h"
#include "PubSubClient.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

//MESH
#define HOSTNAME "MQTT_Bridge"
#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555
painlessMesh mesh;

//Wifi
#define STATION_SSID "Bbox-4DD70ADE"
#define STATION_PASSWORD "551F54E2D72A27CA1EA44567F149E1"
WiFiClient wifiClient;

//MQTT
#define PUBPLISHSUFFIX "eolienne/painlessMesh/from/"
#define SUBSCRIBESUFFIX "eolienne/painlessMesh/to/"
#define PUBPLISHFROMGATEWAYSUFFIX PUBPLISHSUFFIX "gateway"
#define CHECKCONNDELTA 60 // secondes
#define MQTTPORT 1883
char mqttBroker[] = "broker.hivemq.com";
PubSubClient mqttClient;

//Task
Scheduler userScheduler; 

// Prototypes
void receivedCallback(const uint32_t &from, const String &msg);
void mqttCallback(char *topic, byte *payload, unsigned int length);

IPAddress getlocalIP();
IPAddress myIP(0, 0, 0, 0);
IPAddress myAPIP(0, 0, 0, 0);

bool calc_delay = false;
SimpleList<uint32_t> nodes;
uint32_t nsent = 0;
char buff[512];
uint32_t nexttime = 0;
uint8_t initialized = 0;

// messages received from the mqtt broker
void messageReceivedGateway(String msg){

  if (msg == "getNodes")
    {
      auto nodes = mesh.getNodeList(true);
      String str;
      for (auto &&id : nodes)
        str += String(id) + String(" ");
      mqttClient.publish(PUBPLISHFROMGATEWAYSUFFIX, str.c_str());
    }

    if (msg == "getrt")
      mqttClient.publish(PUBPLISHFROMGATEWAYSUFFIX, mesh.subConnectionJson(false).c_str());
    
    if (msg == "asnodetree")
    {
      //mqttClient.publish( PUBPLISHFROMGATEWAYSUFFIX, mesh.asNodeTree().c_str() );
    }

}

void mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{

  //extract message MQTT and topic
  char *cleanPayload = (char *)malloc(length + 1);
  payload[length] = '\0';
  memcpy(cleanPayload, payload, length + 1);
  String msg = String(cleanPayload);
  free(cleanPayload);

  //affiche
  Serial.printf("mc t:%s  p:%s\n", topic, payload);

  //en fonction de l'esp
  String targetStr = String(topic).substring(strlen(SUBSCRIBESUFFIX));

  if (targetStr == "gateway")
    messageReceivedGateway(msg); // envoyer a moi
  else if (targetStr == "broadcast")
    mesh.sendBroadcast(msg); // envoyer a tous les esps
  else
  { 
    // envoyer a un esp en particulier
    uint32_t target = strtoul(targetStr.c_str(), NULL, 10);
    if (mesh.isConnected(target))
      mesh.sendSingle(target, msg);
  }
}
// Needed for painless library

// message encoyer d'un des esp du réseau
void receivedCallback(const uint32_t &from, const String &msg)
{
  Serial.printf("bridge: Received from %u msg=%s\n", from, msg.c_str());
  String topic = PUBPLISHSUFFIX + String(from);
  mqttClient.publish(topic.c_str(), msg.c_str());
}

void newConnectionCallback(uint32_t nodeId)
{
  Serial.printf("--> Start: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> Start: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
}

//quand une des connections esp du reseau change
void changedConnectionCallback()
{
  Serial.printf("Changed connections\n");

  nodes = mesh.getNodeList();

  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");


  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end())
  {
    Serial.printf(" %u", *node);
    node++;
  }

  Serial.println();
  calc_delay = true;

  sprintf(buff, "Nodes:%d", nodes.size());
}

void nodeTimeAdjustedCallback(int32_t offset)
{
  Serial.printf("Adjusted time %u Offset = %d\n", mesh.getNodeTime(), offset);
}

void onNodeDelayReceived(uint32_t nodeId, int32_t delay)
{
  Serial.printf("Delay from node:%u delay = %d\n", nodeId, delay);
}

void reconnectMQTT()
{
  int i;

  // generate unique addresss.
  char MAC[9];
  sprintf(MAC, "%08X", (uint32_t)ESP.getEfuseMac());

  while (!mqttClient.connected())
  {
    Serial.println("Attempting MQTT connection...");

    if(mqttClient.connect(/*MQTT_CLIENT_NAME*/ MAC))
    {
      Serial.println("Connected");
      mqttClient.publish(PUBPLISHFROMGATEWAYSUFFIX, "Ready!");
      mqttClient.subscribe(SUBSCRIBESUFFIX"#");
    }
    else
    {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
      //regarde si changement sur mesh ou mqtt
      mesh.update();
      mqttClient.loop();
    }
  }
}

IPAddress getlocalIP()
{
  return IPAddress(mesh.getStationIP());
}

String scanprocessor(const String &var)
{
  if (var == "SCAN")
    return mesh.subConnectionJson(false);
  return String();
}

void setup()
{

  Serial.begin(115200);

  // set before init() so that you can see startup messages
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | MSG_TYPES | REMOTE ); // all types on except GENERAL
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION); 

  // Channel set to 1. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);

  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);

  //mesh.subConnectionJson(false)

  //mesh.initOTAReceive("bridge");

  sprintf(buff, "Id:%d", mesh.getNodeId());

  mqttClient.setServer(mqttBroker, MQTTPORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setClient(wifiClient);
}

void loop()
{
  // it will run the user scheduler as well
  mesh.update();
  mqttClient.loop();

  if (myIP != getlocalIP())
  {
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
    initialized = 1;
  }
  if ((millis() >= nexttime) && (initialized))
  {
    nexttime = millis() + CHECKCONNDELTA * 1000;
    if (!mqttClient.connected())
    {
      reconnectMQTT();
    }
  }
}