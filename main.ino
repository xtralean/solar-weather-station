#include "SI1145.h"
#include "HDC1000.h"
#include "HttpClient.h"

SI1145 uvIndex = SI1145();
HDC1000 tempSensor = HDC1000();

float temperature;
float humidity;
int uv;
int ir;
int visible;
int solarVoltage;
int batteryVoltage;
bool daytime;

typedef enum {
  STATE_PROGRAM = 0,
  STATE_BOOT,
  STATE_WIFI_CONNECTING,
  STATE_WIFI_COMPLETE
} machine_state;

machine_state currentState;

extern char APIKEY[];
// uncomment and put your thingspeak api here
//char APIKEY[] = "????????????????"; //thingspeak API key
char status[256] = "";
char IPADDRESS[] = "api.thingspeak.com";
char URL[] = "/update";

SYSTEM_MODE(MANUAL);

void setup()
{
  pinMode(D4, INPUT_PULLUP);
  currentState = STATE_BOOT;

  // test if input at D4 is LOW, enter program mode if so
  // turn on LED, and start connection process
  if (digitalRead(D4) == LOW)
  {
    currentState = STATE_PROGRAM;
    pinMode(D7, OUTPUT);
    digitalWrite(D7, HIGH);
    Spark.connect();
    return;
  }

  // turn off main led to save power, also use our own LED colors for states
  RGB.control(true);
  RGB.color(0,0,0);

  // get solar and battery voltage
  solarVoltage = analogRead(A1);
  batteryVoltage = analogRead(A0);

  // early exit - battery voltage is too LOW
  if (batteryVoltage < 2200)
  {
    // if at night time wait 20 minutes before trying again
    if (solarVoltage < 2250)
    {
      System.sleep(SLEEP_MODE_DEEP, 60*20);
    }
    else
    {
      // solar power is working, wait 5 minutes, let it charge the battery
      System.sleep(SLEEP_MODE_DEEP, 60*5);
    }
  }

  daytime = true;
  if (solarVoltage < 1000)
    daytime = false;

  delay(100);
  uvIndex.begin();
  tempSensor.begin();
}

char message[80];

HttpClient http;
http_request_t request;
http_response_t response;

http_header_t headers[] = {
    //  { "Content-Type", "application/json" },
    //  { "Accept" , "application/json" },
    { "X-THINGSPEAKAPIKEY" , APIKEY },
    { "Content-Type", "application/x-www-form-urlencoded" },
    { NULL, NULL } // NOTE: Always terminate headers with NULL
};

void loop()
{
  switch (currentState)
  {
    case STATE_PROGRAM:
      if (Spark.connected())
      {
        Spark.process();
      }
      return;
    case STATE_BOOT:
      RGB.color(0,0,255);
      // boot - read data from sensors
      temperature = tempSensor.readTemperature();
      humidity = tempSensor.readHumidity();
      uv = uvIndex.readUV();
      ir = uvIndex.readIR();
      visible = uvIndex.readVisible();
      WiFi.connect();
      currentState = STATE_WIFI_CONNECTING;
      break;
    case STATE_WIFI_CONNECTING:
      Spark.process();
      RGB.color(255,0,0);
      if (WiFi.ready())
      {
        RGB.color(0,255,0);
        request.hostname = IPADDRESS;
        request.port = 80;
        request.path = URL;

        float solar = (float)solarVoltage / 2048.0 * 3.3;
        float battery = (float)batteryVoltage / 2048.0 * 3.3;

        request.body =  "field1=" + String((int)temperature, DEC)
                         + "&" + "field2=" + String((int)humidity, DEC) // Do not omit
                         + "&" + "field3=" + String(uv, DEC) // the &
                         + "&" + "field4=" + String(ir, DEC)
                         + "&" + "field5=" + String(visible, DEC)
                         + "&" + "field6=" + String(solar, 2)
                         + "&" + "field7=" + String(battery, 2);
        http.post(request, response, headers);
        currentState = STATE_WIFI_COMPLETE;
      }
      break;
  }

  // stay on for only 30 seconds maximum or if data has been transmitted
  if (millis() > 30000 || currentState == STATE_WIFI_COMPLETE)
  {
    // default - reconnect in 10 minutes
    int reconnectIn = 10;
    if (daytime == false)
      reconnectIn = 15;

    // did not connect to the Internet
    if (!WiFi.ready())
    {
      if (batteryVoltage > 2250 && solarVoltage > 2400)
      {
        // strong battery and solar voltage - power back on in 3 minutes
        reconnectIn = 3;
      }
      // else fall thru and test for day/night
    }
    System.sleep(SLEEP_MODE_DEEP, reconnectIn*60);
  }
}
