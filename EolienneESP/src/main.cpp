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
#define MESH_PREFIX "EolienneMeshNetwork"
#define MESH_PASSWORD "FRhgekQV3zu=y?Rs"
#define MESH_PORT 5555
painlessMesh mesh;

//Wifi
#define STATION_SSID "TOTA"
#define STATION_PASSWORD "TATA"
WiFiClient wifiClient;

//MQTT
#define GATEWAY_SUFFIX "Gateway"
#define BROADCAST_SUFFIX "Broadcast"
#define SUBSCRIBESUFFIX "Eolienne/Mesh/To/"
#define PUBPLISHFROMGATEWAYSUFFIX PUBPLISHSUFFIX GATEWAY_SUFFIX
#define PUB_DATA_NODE "Eolienne/DataNode"
#define PUB_NETWORK "Eolienne/Network"
#define GET_NODES_GATEWAY "GetNodes"
#define GET_NETWORK "GetNetwork"
String PUBLISH_GATEWAY = "Eolienne/Mesh/From/" + String(GATEWAY_SUFFIX);

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

// Message recu du broker mqtt pour la gateway du reseau mesh
void messageReceivedGateway(String msg){

  //Demande d'envoi des noeuds de la gateway actuel
  if (msg == GET_NODES_GATEWAY)
  {
    auto nodes = mesh.getNodeList(true);
    String str;
    for (auto &&id : nodes)
      str += String(id) + String(" ");
    mqttClient.publish(PUBLISH_GATEWAY.c_str(), str.c_str());
  }

  //Demande d'envoi du reseau mesh actuel
  if (msg == GET_NETWORK)
    mqttClient.publish(PUB_NETWORK, mesh.subConnectionJson(false).c_str());
    
}

void mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  // Extraction du topic et du message venant du broker mqtt
  char *cleanPayload = (char *)malloc(length + 1);
  payload[length] = '\0';
  memcpy(cleanPayload, payload, length + 1);
  String msg = String(cleanPayload);
  free(cleanPayload);
  Serial.printf("mc t:%s  p:%s\n", topic, payload);

  // Recupere l'action, cad message pour la gateway, 
  // ou a broadcast au reseau, 
  // ou pour un noeud en particulier
  String targetStr = String(topic).substring(strlen(SUBSCRIBESUFFIX));

  // Si c'est pour la gateway
  if (targetStr == GATEWAY_SUFFIX) messageReceivedGateway(msg); 
  // Si le message est un broadcast
  else if (targetStr == BROADCAST_SUFFIX) mesh.sendBroadcast(msg);
  // Sinon essaye d'extraire l'id du noeud a envoyer
  else
  { 
    // Extrait l'id du noeud 
    uint32_t target = strtoul(targetStr.c_str(), NULL, 10);
    // Si il est connecter, envoi le message au noeud
    if (mesh.isConnected(target))
      mesh.sendSingle(target, msg);
  }
}

// Message recu d'un des noeuds du reseau
void receivedCallback(const uint32_t &from, const String &msg)
{
  Serial.printf("bridge: Received from %u msg=%s\n", from, msg.c_str());
  // Envoi des données du noeud vers le broker mqtt
  String topic = PUB_DATA_NODE;
  mqttClient.publish(topic.c_str(), msg.c_str());
}

// Quand une notification lié aux connections mesh
void onConnectionCallback(){
  mqttClient.publish(PUB_NETWORK, mesh.subConnectionJson(false).c_str());
}

// Notification quand il y a une connection au bridge
void newConnectionCallback(uint32_t nodeId)
{
  Serial.printf("--> Start: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> Start: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
  onConnectionCallback();
}

// Notification quand il y a un changement au niveau des connections
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

  onConnectionCallback();
}

void nodeTimeAdjustedCallback(int32_t offset)
{
  Serial.printf("Adjusted time %u Offset = %d\n", mesh.getNodeTime(), offset);
}

void onNodeDelayReceived(uint32_t nodeId, int32_t delay)
{
  Serial.printf("Delay from node:%u delay = %d\n", nodeId, delay);
}

// Tentative de connexion au broker MQTT
void reconnectMQTT()
{
  // Generation d'une ID unique comme ID mqtt
  char MAC[9];
  sprintf(MAC, "%08X", (uint32_t)ESP.getEfuseMac());

  // Tant que le bridge n'est pas connecter au broker mqtt
  while (!mqttClient.connected())
  {

    Serial.println("Attempting MQTT connection...");

    //Si le bridge est connecter 
    if(mqttClient.connect(MAC))
    {
      Serial.println("Connected");
      //Subscribe au topic peut importe la terminaison
      mqttClient.subscribe(SUBSCRIBESUFFIX"#");
    }
    else
    {
      Serial.print("Failed, rc=" + String(mqttClient.state()));
      Serial.println(" try again in 2 seconds");
      delay(2000);
      // Regarde si il y a un changement pour mesh ou mqtt
      mesh.update();
      mqttClient.loop();
    }
  }
}

IPAddress getlocalIP()
{
  return IPAddress(mesh.getStationIP());
}

void taskSendNetwork(void * args){
  while(true){
    mqttClient.publish(PUB_NETWORK, mesh.subConnectionJson(false).c_str());
    vTaskDelay(30000);
  }
  vTaskDelete(NULL);
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

  // Definie la porte de sortie du bridge
  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);

  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);

  sprintf(buff, "Id:%d", mesh.getNodeId());

  // Initialisation des informations concernant le broker mqtt
  mqttClient.setServer(mqttBroker, MQTTPORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setClient(wifiClient);

  xTaskCreate(taskSendNetwork, "SendingData", 2048, NULL, 1, NULL);

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