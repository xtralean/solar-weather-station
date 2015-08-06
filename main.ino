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

void setup()
{
  pinMode(D7, OUTPUT);

  Spark.publish("Weather Station", "boot");

  delay(2000);
  if (uvIndex.begin())
  {
    Spark.publish("Sensor", "SI1145");
  }
  if (tempSensor.begin())
  {
    Spark.publish("Sensor", "HDC1000");
  }
}

int lastSecond = -1;
int lastMinute = -1;

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
  // items that get processed one a second
  if (lastSecond != Time.second())
  {
    lastSecond = Time.second();
    if (lastSecond & 1)
    {
      digitalWrite(D7, LOW);
    }
    else
    {
      digitalWrite(D7, HIGH);
    }
  }
  // items that get processed once a minute
  if (lastMinute != Time.minute())
  {
    if (Spark.connected() == false)
      System.reset();
      
    lastMinute = Time.minute();
    temperature = tempSensor.readTemperature();
    humidity = tempSensor.readHumidity();
    uv = uvIndex.readUV();
    ir = uvIndex.readIR();
    visible = uvIndex.readVisible();
    int solar = analogRead(A1);
    int battery = analogRead(A0);

    sprintf(message, "%dC %d%% uv:%d ir:%d v:%d v1:%d v2:%d", (int)temperature, (int)humidity, uv, ir, visible, solar, battery);
    Spark.publish("data", message);

    request.hostname = IPADDRESS;
    request.port = 80;
    request.path = URL;

    request.body =  "field1=" + String((int)temperature, DEC)
                     + "&" + "field2=" + String((int)humidity, DEC) // Do not omit
                     + "&" + "field3=" + String(uv, DEC) // the &
                     + "&" + "field4=" + String(ir, DEC)
                     + "&" + "field5=" + String(visible, DEC)
                     + "&" + "field6=" + String(solar, DEC)
                     + "&" + "field7=" + String(battery, DEC);
    http.post(request, response, headers);
  }
}
