#include <Arduino.h>
#include "disk91_LoRaE5.h"
#include "Free_Fonts.h"
#include "seeed_line_chart.h" //include the library

typedef enum DisplayMode
{
  eDisplayMode_Graph = 0,
  eDisplayMode_Value
  // Additional Modes here
} eDisplayMode;

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Sprite

#define MAX_SIZE 60 // 30 // maximum size of data
doubles data;       // Initilising a doubles type to store data

int sensorPin = A0;
int sensorValue = 0;
int sensorValuePercent = 0;

static bool screenSleeping = false;
static bool manualUplink = false;
static eDisplayMode curDisplayMode = eDisplayMode_Graph;

Disk91_LoRaE5 lorae5(&Serial); // Where the AT command and debut traces are printed

// 15 Minute uplink (modify to adjust uplink)
#define UplinkPeriodMs 900000

// How often to poll the sensor
#define SENSOR_READ_MS 10000

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

/* Protos */
void update_screen(void);

void set_next_display_mode(void)
{
  switch (curDisplayMode)
  {
  case eDisplayMode_Graph:
    curDisplayMode = eDisplayMode_Value;
    break;
  case eDisplayMode_Value:
    curDisplayMode = eDisplayMode_Graph;
    break;
  }
  // Find a way to trigger screen updating without waiting for main loop...
}

void btn1_screen_sleep_callback(void)
{
  Serial.println("SCR Pushed");
  screenSleeping = !screenSleeping;
  if (screenSleeping)
  {
    digitalWrite(LCD_BACKLIGHT, LOW);
  }
  else
  {
    digitalWrite(LCD_BACKLIGHT, HIGH);
  }
  delay(1000);
}
void btn2_uplink_callback(void)
{
  Serial.println("UPL Pushed");
  manualUplink = true;
  delay(1000);
}
void btn3_disp_mode_callback(void)
{
  Serial.println("DSP Pushed");
  set_next_display_mode();
  delay(1000);
}

int value_to_percent(int val)
{
  int pct = 0;
  if (val >= UpperSensorReading)
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

  // Button setup
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  attachInterrupt(WIO_KEY_A, btn1_screen_sleep_callback, FALLING);
  attachInterrupt(WIO_KEY_B, btn2_uplink_callback, FALLING);
  attachInterrupt(WIO_KEY_C, btn3_disp_mode_callback, FALLING);

  // Sensor setup
  pinMode(sensorPin, INPUT);

  // Screen setup
  tft.begin();
  tft.setRotation(1); // 3);
  spr.createSprite(TFT_HEIGHT, TFT_WIDTH);
  spr.setRotation(1); // 3);
}

void draw_footer(void)
{
  // Screen: 320x240
  spr.setFreeFont(FF5);
  if (lorae5.isJoined())
  {
    spr.drawString("Joined", 10, 220);
  }
  else
  {
    spr.drawString("Not Joined", 10, 220);
  }
  spr.drawString("SCR  UPL  DSP", 155, 220);
}

void update_graph(int value)
{
  spr.fillSprite(TFT_WHITE);

  // Settings for the line graph title
  char title[32] = {0};
  sprintf(title, "Soil Moisture (%d%%)", value);
  auto header = text(0, 0)
                    .value(title)
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
      .show_circle(true)                            // drawing a cirle at each point, default is on.
      .value(data)                                  // passing through the data to line graph
      .max_size(MAX_SIZE)
      .color(TFT_DARKGREEN) // Setting the color for the line
      .backgroud(TFT_WHITE)
      .draw(&spr);

  draw_footer();
  spr.pushSprite(0, 0);
}

void update_big_value(int value)
{
  char pct[5] = {0};
  sprintf(pct, "%d%%", value);

  spr.fillSprite(TFT_WHITE);
  spr.setFreeFont(FF8);
  spr.drawString(pct, 140, 100);
  draw_footer();
  spr.pushSprite(0, 0);
}

void handle_data(int value)
{
  if (data.size() > MAX_SIZE)
  {
    data.pop(); // this is used to remove the first read variable
  }
  data.push(value); // read variables and store in data
}

void update_screen(void)
{
  switch (curDisplayMode)
  {
  case eDisplayMode_Graph:
    update_graph(sensorValuePercent);
    break;
  case eDisplayMode_Value:
    update_big_value(sensorValuePercent);
    break;
  default:
    Serial.println("Unknown displaymode set");
    break;
  }
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

  if (((curTick - lastTick) >= UplinkPeriodMs) || manualUplink)
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
    if (manualUplink)
    {
      manualUplink = false;
    }
  }

  // Keep the data collection running
  handle_data(sensorValuePercent);

  // Update graph in percentages
  update_screen();
  delay(SENSOR_READ_MS);
}