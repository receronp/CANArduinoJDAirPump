#include <ArduinoWebsockets.h>
#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <mcp_can.h>
#include <SPI.h>

/* Websocket definitions */
const char *ssid = "NETWORK_SSID";     // Enter SSID
const char *password = "NETWORK_PASS"; // Enter Password
const char *serverAddress = "wss://vast-reef-61525.herokuapp.com/:80";

using namespace websockets;
WebsocketsClient client;

/* MCP2515 definitions */
#define CAN_ID_EXTENDED 1
#define WS_BUFFER_SIZE 13
// CAN TX Variables
unsigned long txId = 0;
unsigned char txLen = 0;
unsigned char txBuf[8];
// CAN RX Variables
unsigned long rxId;
unsigned char len;
unsigned char rxBuf[8];
unsigned char rxMsgBuffer[WS_BUFFER_SIZE];
// CAN0 INT and CS
#ifdef ARDUINO_ARCH_ESP32
#define CAN0_INT 21 // Set INT to pin 2 on ESP32
MCP_CAN CAN0(5);    // Set CS to pin 5 on ESP32
#else
#define CAN0_INT 4 // Set INT to pin 2 on ESP8266
MCP_CAN CAN0(15); // Set CS to pin 15 on ESP8266
#endif

void setup()
{
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("*");
  }

  Serial.println("");
  Serial.println("WiFi connection Successful");
  Serial.print("The IP Address of Module is: ");
  Serial.println(WiFi.localIP()); // Print the IP address

  delay(2000);

  // Connect to the websocket server
  if (client.connect(serverAddress))
  {
    Serial.println("Connected");
  }
  else
  {
    Serial.println("Connection failed.");
    while (1)
    {
      // Hang on failure
    }
  }

  // run callback when messages are received
  client.onMessage([&](WebsocketsMessage message)
                   {
      // Parse ws received data
      const char *wsDataStr = message.c_str();
      // Split data to diff variables
      for (int dataIndex = 3; dataIndex >= 0; dataIndex--)
      {
        txId = (txId << 8) + wsDataStr[dataIndex];
      }
      txLen = wsDataStr[4];
      memcpy(&txBuf, &wsDataStr[5], sizeof(txBuf));
      // Send messate to CAN module
      byte sndStat = CAN0.sendMsgBuf(txId,CAN_ID_EXTENDED, txLen, txBuf);
    
      if(sndStat == CAN_OK)
        Serial.println("CAN Message Sent Successfully!");
      else
        Serial.println("Error Sending CAN Message..."); });

  /* Initialize MCP CAN */
  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");

  // Set NORMAL mode
  CAN0.setMode(MCP_NORMAL);

  pinMode(CAN0_INT, INPUT); // Configuring pin for /INT input
}

void loop()
{
  if (!digitalRead(CAN0_INT)) // If CAN0_INT pin is low, read receive buffer
  {
    CAN0.readMsgBuf(&rxId, &len, rxBuf); // Read data: len = data length, buf = data byte(s)

    // Send received message to websocket server
    if (client.available())
    {
      // Copy data to buffer
      rxId = rxId & 0x1FFFFFFF;
      memcpy(rxMsgBuffer, (unsigned char *)&(rxId), sizeof(unsigned long));
      rxMsgBuffer[4] = len;
      memcpy(&rxMsgBuffer[5], &rxBuf, sizeof(rxBuf));
      // Send buffer - fixed for CAN extended message type
      client.sendBinary((const char *)rxMsgBuffer, 13);
      // Serial.println("WS Message Sent Successfully!");
      // Serial.println(rxMsgBuffer[0]);
      // for(int i = 0; i < sizeof(rxBuf); i++){
      // Serial.print(rxBuf[i]);
      //}
      // Serial.println();
    }
  }

  // let the websockets client check for incoming messages
  if (client.available())
  {
    client.poll();
  }
}