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

#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
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

void setup()
{

  Serial.begin(115200);

  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | MSG_TYPES | REMOTE ); // all types on except GENERAL
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  //mesh.initOTAReceive(ROLE);

  //userScheduler.addTask( taskSendMessage );
  //taskSendMessage.enable();

  sprintf(buff, "Id:%d", mesh.getNodeId());

}

void loop()
{
  mesh.update();
}