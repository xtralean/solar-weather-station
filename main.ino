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

extern char APIKEY[];
// uncomment and put your thingspeak api here
//char APIKEY[] = "????????????????"; //thingspeak API key
char status[256] = "";
char IPADDRESS[] = "api.thingspeak.com";
char URL[] = "/update";

SYSTEM_MODE(MANUAL);

void setup()
{
  // turn off main led to save power
  RGB.control(true);
  RGB.color(0,0,0);

  // get solar and battery voltage
  solarVoltage = analogRead(A1);
  batteryVoltage = analogRead(A0);

  // early exit - battery voltage is too LOW
  if (batteryVoltage < 2180)
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

  // battery voltage is good, boot system
  pinMode(D7, OUTPUT);
  digitalWrite(D7, HIGH);

  delay(500);
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

int dataReady = 0;
int connectionStarted = 0;
int transmitData = 0;

void loop()
{
  // read data from sensors
  if (dataReady == 0)
  {
    temperature = tempSensor.readTemperature();
    humidity = tempSensor.readHumidity();
    uv = uvIndex.readUV();
    ir = uvIndex.readIR();
    visible = uvIndex.readVisible();
    dataReady = 1;
  }

  // start up Internet connection
  if (connectionStarted == 0)
  {
    RGB.control(false);
    digitalWrite(D7, LOW);
    Spark.connect();
    connectionStarted = 1;
  }

  if (Spark.connected())
  {
    // process any background tasks for wifi
    Spark.process();

    if (transmitData == 0)
    {
      transmitData = 1;
      request.hostname = IPADDRESS;
      request.port = 80;
      request.path = URL;

      request.body =  "field1=" + String((int)temperature, DEC)
                       + "&" + "field2=" + String((int)humidity, DEC) // Do not omit
                       + "&" + "field3=" + String(uv, DEC) // the &
                       + "&" + "field4=" + String(ir, DEC)
                       + "&" + "field5=" + String(visible, DEC)
                       + "&" + "field6=" + String(solarVoltage, DEC)
                       + "&" + "field7=" + String(batteryVoltage, DEC);
      http.post(request, response, headers);
    }
  }

  // stay on for only 30 seconds maximum
  if (millis() > 30000)
  {
    // did not connect to the Internet
    if (Spark.connected() == false)
    {
      if (batteryVoltage > 2250 && solarVoltage > 2400)
      {
        // strong battery and solar voltage - power back on in 60 seconds
        System.sleep(SLEEP_MODE_DEEP, 60);
        goto exit;
      }
      // else fall thru and test for day/night
    }

    // connection successul - come back in 5 minutes during daytime, 10 minutes at night
    if (solarVoltage < 1000)
    {
      System.sleep(SLEEP_MODE_DEEP, 10*60);
    }
    else
    {
      System.sleep(SLEEP_MODE_DEEP, 5*60);
    }
  }
  exit:;
}
