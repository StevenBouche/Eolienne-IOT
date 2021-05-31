//************************************************************
// this is a simple example that uses the painlessMesh library
//
// 1. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 2. prints anything it receives to Serial.print
//
//
//************************************************************
//#include <Arduino.h>
#include "painlessMesh.h"
#include "painlessmesh/protocol.hpp"

#define MESH_PREFIX "EolienneMeshNetwork"
#define MESH_PASSWORD "FRhgekQV3zu=y?Rs"
#define MESH_PORT 5555

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;
uint32_t nsent = 0;
char buff[512];

void sendMessage(String msg); // Prototype 

//recois un message d'un autre noeud
void receivedCallback(uint32_t from, String &msg)
{
  //affiche
  Serial.printf("Rx from %u <- %s\n", from, msg.c_str());

  if (strcmp(msg.c_str(), "GETRT") == 0)
    mesh.sendBroadcast(mesh.subConnectionJson(true).c_str());
  else
    sprintf(buff, "Rx:%s", msg.c_str());
}

void newConnectionCallback(uint32_t nodeId)
{
  Serial.printf("--> Start: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> Start: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
}

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

void sendMessage(String msg)
{
  if (strcmp(msg.c_str(), "") == 0)
  {
    nsent++;
    msg = "TOTO";
    //msg += mesh.getNodeId();
    msg += nsent;
  }

  mesh.sendBroadcast(msg);

  Serial.printf("Tx-> %s\n", msg.c_str());

  sprintf(buff, "Tx:%s", msg.c_str());

  if (calc_delay)
  {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end())
    {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }
}

size_t getRootId(painlessmesh::protocol::NodeTree nodeTree) {
  if (nodeTree.root) return nodeTree.nodeId;
  for (auto&& s : nodeTree.subs) {
    auto id = getRootId(s);
    if (id != 0) return id;
  }
  return 0;
}

String getJsonState(){

  String str;
  StaticJsonDocument<256> doc;

  long randBroken = random(11);
  doc["idMesh"] = mesh.getNodeId();
  doc["isBroken"] = randBroken <= 8 ? false : true;
  doc["speedPourcent"] = randBroken <= 8 ? random(101) : 0;
  
  serializeJson(doc, str);

  return str;
}

void taskSendStateEolienne(void * args){
  while(true){
    size_t rootId = getRootId(mesh.asNodeTree());
    if(rootId != 0){
      String str = getJsonState();
      Serial.println(str);
      mesh.sendSingle(rootId,str);
    }
    vTaskDelay(10000);
  }
  vTaskDelete(NULL);
}

void setup()
{
  Serial.begin(115200);

  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | MSG_TYPES | REMOTE ); // all types on except GENERAL
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes(ERROR | STARTUP); 

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.
  sprintf(buff, "Id:%d", mesh.getNodeId());

  xTaskCreate(taskSendStateEolienne, "SendingData", 2048, NULL, 1, NULL);

}

void loop()
{
  mesh.update();
}