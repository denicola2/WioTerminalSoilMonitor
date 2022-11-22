#include <Arduino.h>
#include "disk91_LoRaE5.h"
#include "Free_Fonts.h"
#include "seeed_line_chart.h" //include the library

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Sprite

#define MAX_SIZE 30 // maximum size of data
doubles data;       // Initilising a doubles type to store data

int sensorPin = A0;
int sensorValue = 0;
int sensorValuePercent = 0;

Disk91_LoRaE5 lorae5(&Serial); // Where the AT command and debut traces are printed

#define UplinkPeriodMs 150000

// Calibrated by submerging sensor
#define UpperSensorReading 550 // Fully submerged

#define Frequency DSKLORAE5_ZONE_US915
/*
Select your frequency band here.
DSKLORAE5_ZONE_EU868
DSKLORAE5_ZONE_US915
DSKLORAE5_ZONE_AS923_1
DSKLORAE5_ZONE_AS923_2
DSKLORAE5_ZONE_AS923_3
DSKLORAE5_ZONE_AS923_4
DSKLORAE5_ZONE_KR920
DSKLORAE5_ZONE_IN865
DSKLORAE5_ZONE_AU915
 */
uint8_t deveui[] = {0x9C, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xC5};
uint8_t appeui[] = {0x71, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0x5E};
uint8_t appkey[] = {0x78, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0x14};

int value_to_percent(int val)
{
  int pct = 0;
  if(val >= UpperSensorReading)
  {
    pct = 100;
  }
  else
  {
    pct = ((float)val / (float)UpperSensorReading) * 100;
  }
  return pct;
}

void data_decord(int val, uint8_t data[2])
{
  data[0] = val >> 8 & 0xFF;
  data[1] = val & 0xFF;
}

void setup(void)
{
  Serial.begin(9600);
  uint32_t start = millis();
  while (!Serial && (millis() - start) < 1500)
    ; // Open the Serial Monitor to get started or wait for 1.5"
  // init the library, search the LORAE5 over the different WIO port available
  if (!lorae5.begin(DSKLORAE5_SEARCH_WIO))
  {
    Serial.println("LoRa E5 Init Failed");
    while (1)
      ;
  }

  // Setup the LoRaWan Credentials
  if (!lorae5.setup(Frequency, deveui, appeui, appkey))
  {
    Serial.println("LoRa E5 Setup Failed");
    while (1)
      ;
  }

  // Screen setup
  pinMode(sensorPin, INPUT);
  tft.begin();
  tft.setRotation(3);
  spr.createSprite(TFT_HEIGHT, TFT_WIDTH);
  spr.setRotation(3);
}

void update_graph(int value)
{
  spr.fillSprite(TFT_WHITE);
  if (data.size() > MAX_SIZE)
  {
    data.pop(); // this is used to remove the first read variable
  }
  data.push(value); // read variables and store in data

  // Settings for the line graph title
  auto header = text(0, 0)
                    .value("Soil Moisture Readings")
                    .align(center)
                    .valign(vcenter)
                    .width(spr.width())
                    .thickness(2);

  header.height(header.font_height(&spr) * 2);
  header.draw(&spr); // Header height is the twice the height of the font

  // Settings for the line graph
  auto content = line_chart(20, header.height()); //(x,y) where the line graph begins
  content
      .height(spr.height() - header.height() * 1.5) // actual height of the line chart
      .width(spr.width() - content.x() * 2)         // actual width of the line chart
      .based_on(0.0)                                // Starting point of y-axis, must be a float
      .show_circle(true)                           // drawing a cirle at each point, default is on.
      .value(data)                                  // passing through the data to line graph
      .max_size(MAX_SIZE)
      .color(TFT_DARKGREEN) // Setting the color for the line
      .backgroud(TFT_WHITE)
      .draw(&spr);

  // Screen: 320x240
  spr.setFreeFont(FF5);
  if(lorae5.isJoined()) 
  {
    spr.drawString("Joined", 10, 220);
  }
  else
  {
    spr.drawString("Not Joined", 10, 220);
  }

  char pct[5] = {0};
  sprintf(pct, "Moisture: %d%%", sensorValuePercent);
  spr.drawString(pct, 175, 220);

  spr.pushSprite(0, 0);
  delay(5000);
}

void loop(void)
{
  unsigned long curTick = millis();
  static unsigned long lastTick = 0;

  sensorValue = analogRead(sensorPin);
  sensorValuePercent = value_to_percent(sensorValue);
  Serial.println("Soil Moisture: ");
  Serial.println(sensorValue);
  Serial.println(sensorValuePercent);

  static uint8_t data[2] = {0, 0}; // Use the data[] to store the values of the sensors

  if ((curTick - lastTick) >= UplinkPeriodMs)
  {
    data_decord(sensorValuePercent, data);

    if (lorae5.send_sync( // Sending the sensor values out
            1,            // LoRaWan Port
            data,         // data array
            sizeof(data), // size of the data
            false,        // we are not expecting a ack
            7,            // Spread Factor
            14            // Tx Power in dBm
            ))
    {
      Serial.println("Uplink done");
      if (lorae5.isDownlinkReceived())
      {
        Serial.println("A downlink has been received");
        if (lorae5.isDownlinkPending())
        {
          Serial.println("More downlink are pending");
        }
      }
    }

    lastTick = curTick;
  }
  
  // Update graph in percentages
  update_graph(sensorValuePercent);
}