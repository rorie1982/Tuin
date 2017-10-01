#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <RTClib.h>
#include <Wire.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Adafruit_AM2315.h>

#define Vh400Pin A0
#define Inlite D6

struct VH400 {
  double analogValue;
  double analogValue_sd;
  double voltage;
  double voltage_sd;
  double VWC;
  double VWC_sd;
  DateTime timeStamp;
};

struct chartData {
  String dateTimeStamp;
  String vwcMeanValue;
};

char daysOfTheWeek[7][12] = {"Zondag", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag"};

IPAddress ip(192, 168, 1, 105);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
DateTime now;

String currentHostName = "ESP-Tuin-01";
String currentVersion = "1.2.2";
String inliteGarrdenLights = "Uitgeschakelt";
long vh400LastInterval  = 0;
long am2315LastInterval = 0;
struct VH400 VH400CurrentValue;
int counter = 0;
bool dst = true;
float currentTemperatureSensorValue = 0;
float currentHumiditySensorValue = 0;

chartData chartDataTable[24];
RTC_DS3231 rtc;
Adafruit_AM2315 am2315;

void setup() {
  const char* username = "username";
  const char* password = "password";
  
  //Serial.begin(9600);
  pinMode(Vh400Pin, OUTPUT);
  pinMode(Inlite, OUTPUT);

  digitalWrite(Inlite, HIGH);
  
  ConnectToWifi();

  InitializeRtcDS3231();
  InitializeAM2315();

  httpServer.on("/", handlePage);
  httpServer.on("/json",handleJson);
  httpServer.on("/action",handleAction);

  ReadAndStoreVH400Values();  
  ReadHumidityAndTemperature();

  httpUpdater.setup(&httpServer, username, password);
  httpServer.begin();
  
}

void loop() {
  
  //elk uur
  if (millis() - vh400LastInterval > 3600000)
  { 
    counter++;
    CheckForDSTAdjustment();
    ReadAndStoreVH400Values();
    vh400LastInterval = millis();
  }

   //elke minuut
  if (millis() - am2315LastInterval > 60000)
  { 
    ReadHumidityAndTemperature();
    am2315LastInterval =  millis();
  }

  httpServer.handleClient();

}

void ReadHumidityAndTemperature()
{ 
  currentHumiditySensorValue = am2315.readHumidity();
  delay(2000);
  currentTemperatureSensorValue = am2315.readTemperature();
}

void ReadAndStoreVH400Values()
{
  VH400CurrentValue = readVH400_wStats(100,50);

  if (now.hour() == 0)
  {
     counter = 0;
  }
  
  chartDataTable[counter].vwcMeanValue = VH400CurrentValue.VWC;
  chartDataTable[counter].dateTimeStamp = String(now.year()) + "," + String(now.month()-1) + "," + String(now.day()) + "," + String(now.hour()) + "," + String(now.minute()) + "," + String(now.second()); 
  
}

void CheckForDSTAdjustment()
{
  if (now.month() == 10 && now.day() >= 25 && String(daysOfTheWeek[now.dayOfTheWeek()]) == "Zondag" && now.hour() >= 2 && dst)
  {
     now = rtc.now();  
     rtc.adjust(DateTime(now.year(), now.month(), now.day(), (now.hour()-1), now.minute(), now.second()));
     counter--;
     dst = false;
  } 
  
  if (now.month() == 3 && now.day() >= 25 && String(daysOfTheWeek[now.dayOfTheWeek()]) == "Zondag" && now.hour() >= 2 && !dst) 
  {
     now = rtc.now();  
     rtc.adjust(DateTime(now.year(), now.month(), now.day(), (now.hour()+1), now.minute(), now.second()));
     counter++;
     dst = true;
  } 
}

void InitializeAM2315()
{
  am2315.begin();  
}


void InitializeRtcDS3231()
{
  rtc.begin();
  // if (rtc.lostPower()) {
  //  Serial.println("RTC lost power, lets set the time!");
  //  // following line sets the RTC to the date & time this sketch was compiled
  //  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //  // This line sets the RTC with an explicit date & time, for example to set
  //  // January 21, 2014 at 3am you would call:
  //  // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  //}
  //now = rtc.now();
  //Serial.println();
  //Serial.print(now.year(), DEC);
  //Serial.print('/');
  //Serial.print(now.month(), DEC);
  //Serial.print('/');
  //Serial.print(now.day(), DEC);
  //Serial.print(" (");
  //Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  //Serial.print(") ");
  //Serial.print(now.hour(), DEC);
  //Serial.print(':');
  //Serial.print(now.minute(), DEC);
  //Serial.print(':');
  //Serial.print(now.second(), DEC);
  //Serial.println(); 
}

void handlePage()
{
  String webPage;
  String currentIpAdres = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  
  webPage += "<!DOCTYPE html>";
  webPage += "<html lang=\"en\">";
  webPage += "<head>";
  webPage += "<title>Tuin</title>";
  webPage += "<meta charset=\"utf-8\">";
  webPage += "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">";
  webPage += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  webPage += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  webPage += GenerateFavIcon();
  webPage += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.10.2/jquery.min.js\"></script>";
  webPage += "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\"></script>";
  webPage += "<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>";
  webPage += GenerateJavaScript();
  webPage += "</head>";
  webPage += "<body>"; 
 
  webPage += "<div class=\"container\">";
  webPage += "<h2>Tuin</h2>";
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Bodemvochtigheid</div>";
  webPage += "<div class=\"panel-body\" id=\"curve_chart\"></div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-tint'></span>&nbsp;";
  webPage += "Laatst gemeten VWC: <span class='pull-right' id=\"TVH40001\"></span>";
  webPage += "</div>";
  webPage += "</div>";

  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Klimaat</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-fire'></span>&nbsp;";
  webPage += "Temperatuur: <span class='pull-right' id='TTemp01'></span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-tint'></span>&nbsp;";
  webPage += "Luchtvogtigheid: <span class='pull-right' id='THum01'></span>";
  webPage += "</div>";
  webPage += "</div>";
  
  webPage += "<div id=\"GPanelLights\" class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Verlichting</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-menu-hamburger'></span>&nbsp;";
  webPage += "Inlite bediening:<span class='pull-right'>";
  webPage += "<button type='button' class='btn btn-default' id='Inlite' onclick='ButtonPressed(this.id)'><span id=\"Tbuton01\">Aan</span></button>&nbsp;";
  webPage += "</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-lamp'></span>&nbsp;";  
  webPage += "Tuinverlichting: <span class='pull-right' id=\"TInlite01\"></span>";
  webPage += "</div>";
  webPage += "</div>";
  
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Algemeen</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;";
  webPage += "Firmware: <span class='pull-right'>"+ currentVersion +"</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;";
  webPage += "IP-adres: <span class='pull-right'>" + currentIpAdres + "</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;";
  webPage += "Host naam: <span class='pull-right'>" + currentHostName + "</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;";
  webPage += "System date time: <span class='pull-right' id=\"TSystemTime01\"></span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "</body>";
  webPage += "</html>";

  httpServer.send ( 200, "text/html", webPage);
  
}

String GenerateJavaScript()
{
  String javaScript;
  javaScript += "<script type=\"text/javascript\">";
  
  // Load the Visualization API and the piechart package.
  javaScript += "google.charts.load('current', {packages: ['corechart', 'line']});";
      
  // Set a callback to run when the Google Visualization API is loaded.
  javaScript += "google.charts.setOnLoadCallback(GetJson);";
  javaScript += "var chart;";
  javaScript += "function GetJson(){";
  javaScript += "var jsonData = $.ajax({";
  javaScript += "url: \"http://192.168.1.105/json\",";
  javaScript += "dataType: \"json\",";
  javaScript += "async: false,";
  javaScript += "success: updatePage";
  javaScript += "});";
  javaScript += "};";
  javaScript += "function updatePage(json){";
  javaScript += "DrawChart(json.devices.VH400History);";
  javaScript += "UpdateLightsPanel(json);";
  javaScript += "};";
  javaScript += "function UpdateLightsPanel(json){";
  javaScript += "$('#TInlite01').text(json.devices.Inlite.state);";
                
  javaScript += "$('#TVH40001').text(json.devices.VH400.value);";
  javaScript += "$('#TTemp01').text(json.devices.Temperature.value);";
  javaScript += "$('#THum01').text(json.devices.Humidity.value);";
  javaScript += "$('#TSystemTime01').text(json.devices.SystemTime.value);";

  javaScript += "var statusLights = $('#TInlite01').html();";
  javaScript += "if (statusLights == \"Ingeschakelt\")";
  javaScript += "{";
  javaScript += "$('#GPanelLights').removeClass('panel panel-default').addClass('panel panel-success');";
  javaScript += "$('#Tbuton01').text(\" Uit\");";                 
  javaScript += "}";
  javaScript += "else";
  javaScript += "{";
  javaScript += "$('#GPanelLights').removeClass('panel panel-success').addClass('panel panel-default');";
  javaScript += "$('#Tbuton01').text(\" Aan\");";
  javaScript += "}";
  javaScript += "}";

  javaScript += "function ButtonPressed(id){";
  javaScript += "$.post('http://192.168.1.105/action?name=' + id + '&value=empty');";
  javaScript += "}";
  
  javaScript += "function DrawChart(data){"; 
  javaScript += "var chartdata = new google.visualization.DataTable(data);";
  javaScript += "var options = {";
  javaScript += "legend: { position: 'bottom' },";
  javaScript += "hAxis: {";
  javaScript += "title: 'Time',format: 'HH:mm'";
  javaScript += "},";
  javaScript += "vAxis: {viewWindowMode:'explicit',viewWindow: {max:50,min:0},";
  javaScript += "title: 'VWC %'";
  javaScript += "},";
  javaScript += "title: 'Volumetric water content',";
  javaScript += "pointSize: 5";
  javaScript += "};";
  
  javaScript += "chart = new google.visualization.AreaChart(document.getElementById('curve_chart'));";
  
  javaScript += "chart.draw(chartdata, options);";
  
  javaScript += "}";
  
  javaScript += "$(document).ready(function(){";
  javaScript += "setInterval(GetJson,5000);";
  javaScript += "});";
  javaScript += "</script>";

  return javaScript;
}

void handleJson()
{
  String JSON;
  DateTime systemTime;
  
  JSON += "{";
  JSON += "\"devices\": {";
  JSON += "\"VH400History\": {";
  JSON += "\"cols\": [{";
  JSON += "\"label\":\"X \",\"type\": \"datetime\"";
  JSON += "}, {";
  JSON += "\"label\":\"" + String(daysOfTheWeek[now.dayOfTheWeek()]) + "\",\"type\": \"number\"";
  JSON += "}],";
  JSON += "\"rows\": [";

  for (int i = 0; i <=counter; i++)
  {
     JSON += "{ \"c\": [{\"v\": \"Date(" + chartDataTable[i].dateTimeStamp + ")\"}, {\"v\": " + chartDataTable[i].vwcMeanValue + "}]},";
  }
     
  JSON.remove(JSON.length()-1);
  
  JSON += "]";
  JSON += "},";
  JSON += "\"Inlite\": {";
  JSON += "\"state\": \"" + inliteGarrdenLights + "\"";
  JSON += "},";

  systemTime = rtc.now();
  JSON += "\"SystemTime\": {";
  JSON += "\"value\": \"" + String(systemTime.year()) + "-" + String(systemTime.month()-1) + "-" + String(systemTime.day()) + ":" + String(systemTime.hour()) + ":" + String(systemTime.minute()) + ":" + String(systemTime.second()) + "\"";
  JSON += "},";

  JSON += "\"Humidity\": {";
  JSON += "\"value\": \"" + String(currentHumiditySensorValue) + String(" %") + "\"";
  JSON += "},";

  JSON += "\"Temperature\": {";
  JSON += "\"value\": \"" + String(currentTemperatureSensorValue) + String(" °C") + "\"";
  JSON += "},";
  
  JSON += "\"VH400\": {";
  JSON += "\"value\": \"" + String(VH400CurrentValue.VWC) + " % (" + String(VH400CurrentValue.timeStamp.hour()) + ":" + String(VH400CurrentValue.timeStamp.minute()) + ")\"";
  JSON += "}";
  JSON += "}";
  JSON += "}";

  httpServer.send ( 200, "text/html", JSON);
}

String GenerateFavIcon()
{
  String returnValue;
  returnValue += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"57x57\" href=\"http://192.168.1.108/apple-icon-57x57.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"60x60\" href=\"http://192.168.1.108/apple-icon-60x60.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"72x72\" href=\"http://192.168.1.108/apple-icon-72x72.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"76x76\" href=\"http://192.168.1.108/apple-icon-76x76.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"114x114\" href=\"http://192.168.1.108/apple-icon-114x114.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"120x120\" href=\"http://192.168.1.108/apple-icon-120x120.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"144x144\" href=\"http://192.168.1.108/apple-icon-144x144.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"152x152\" href=\"http://192.168.1.108/apple-icon-152x152.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"180x180\" href=\"http://192.168.1.108/apple-icon-180x180.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"192x192\"  href=\"http://192.168.1.108/android-icon-192x192.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"32x32\" href=\"http://192.168.1.108/favicon-32x32.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"96x96\" href=\"http://192.168.1.108/favicon-96x96.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"http://192.168.1.108/favicon-16x16.png\">";
  returnValue += "<link rel=\"manifest\" href=\"http://192.168.1.108/manifest.json\">";
  returnValue += "<meta name=\"msapplication-TileColor\" content=\"#ffffff\">";
  returnValue += "<meta name=\"msapplication-TileImage\" content=\"http://192.168.1.108/ms-icon-144x144.png\">";
  returnValue += "<meta name=\"theme-color\" content=\"#ffffff\">";
  return returnValue;
}

void handleAction()
{  
  String argumentName = "";
  String argumentValue = "";
  if (httpServer.args() > 0)
  {
    //server.arg(0) = name
    //server.arg(1) = value
    if (httpServer.arg(0).length() > 0 && httpServer.arg(1).length() > 0)
    {
      argumentName = httpServer.arg(0);
      argumentValue = httpServer.arg(1);
      if ( argumentName == "Inlite")
      {
        if (argumentValue == "empty")
        {
          if(inliteGarrdenLights == "Ingeschakelt")
          {
            argumentValue = "Uitgeschakelt";
          }
          else
          {
            argumentValue = "Ingeschakelt";
          }
        }

      inliteGarrdenLights = argumentValue;
      ControlLights(argumentValue);  
      
      }
    }
  }
}

void ControlLights(String value)
{
  if (value == "Ingeschakelt" )
  {
    digitalWrite(Inlite, LOW);
  }
  else
  {
    digitalWrite(Inlite, HIGH);
  }
}

void ConnectToWifi()
{
  char ssid[] = "ssid";
  char pass[] = "password";
  int i = 0;

  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);

  WiFi.hostname(currentHostName);
  WiFi.begin(ssid, pass);
  WiFi.mode(WIFI_STA);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

}

float readVH400() {
  // This function returns Volumetric Water Content by converting the analogPin value to voltage
  // and then converting voltage to VWC using the piecewise regressions provided by the manufacturer
  // at http://www.vegetronix.com/Products/VH400/VH400-Piecewise-Curve.phtml
  
  // NOTE: You need to set analogPin to input in your setup block
  //   ex. pinMode(<analogPin>, INPUT);
  //   replace <analogPin> with the number of the pin you're going to read from.
  
  // Read value and convert to voltage  
  int sensor1DN = analogRead(Vh400Pin);
  float sensorVoltage = sensor1DN*(3.0 / 1023.0);
  float VWC;
  
  // Calculate VWC
  if(sensorVoltage <= 1.1) {
    VWC = 10*sensorVoltage-1;
  } else if(sensorVoltage > 1.1 && sensorVoltage <= 1.3) {
    VWC = 25*sensorVoltage-17.5;
  } else if(sensorVoltage > 1.3 && sensorVoltage <= 1.82) {
    VWC = 48.08*sensorVoltage-47.5;
  } else if(sensorVoltage > 1.82) {
    VWC = 26.32*sensorVoltage-7.89;
  }
  return(VWC);
}

struct VH400 readVH400_wStats(int nMeasurements, int delayBetweenMeasurements) {
  // This variant calculates the mean and standard deviation of 100 measurements over 5 seconds.
  // It reports mean and standard deviation for the analog value, voltage, and WVC.
  
  // This function returns Volumetric Water Content by converting the analogPin value to voltage
  // and then converting voltage to VWC using the piecewise regressions provided by the manufacturer
  // at http://www.vegetronix.com/Products/VH400/VH400-Piecewise-Curve.phtml
  
  // NOTE: You need to set analogPin to input in your setup block
  //   ex. pinMode(<analogPin>, INPUT);
  //   replace <analogPin> with the number of the pin you're going to read from.

  struct VH400 result;
  
  // Sums for calculating statistics
  int sensorDNsum = 0;
  double sensorVoltageSum = 0.0;
  double sensorVWCSum = 0.0;
  double sqDevSum_DN = 0.0;
  double sqDevSum_volts = 0.0;
  double sqDevSum_VWC = 0.0;

  // Arrays to hold multiple measurements
  int sensorDNs[nMeasurements];
  double sensorVoltages[nMeasurements];
  double sensorVWCs[nMeasurements];

  // Make measurements and add to arrays
  for (int i = 0; i < nMeasurements; i++) { 
    // Read value and convert to voltage 
    int sensorDN = analogRead(Vh400Pin);
    double sensorVoltage = sensorDN*(3.0 / 1023.0);
        
    // Calculate VWC
    float VWC;
    if(sensorVoltage <= 1.1) {
      VWC = 10*sensorVoltage-1;
    } else if(sensorVoltage > 1.1 && sensorVoltage <= 1.3) {
      VWC = 25*sensorVoltage-17.5;
    } else if(sensorVoltage > 1.3 && sensorVoltage <= 1.82) {
      VWC = 48.08*sensorVoltage-47.5;
    } else if(sensorVoltage > 1.82) {
      VWC = 26.32*sensorVoltage-7.89;
    }

    // Add to statistics sums
    sensorDNsum += sensorDN;
    sensorVoltageSum += sensorVoltage;
    sensorVWCSum += VWC;

    // Add to arrays
    sensorDNs[i] = sensorDN;
    sensorVoltages[i] = sensorVoltage;
    sensorVWCs[i] = VWC;

    // Wait for next measurement
    delay(delayBetweenMeasurements);
  }

  // Calculate means
  double DN_mean = double(sensorDNsum)/double(nMeasurements);
  double volts_mean = sensorVoltageSum/double(nMeasurements);
  double VWC_mean = sensorVWCSum/double(nMeasurements);

  // Loop back through to calculate SD
  for (int i = 0; i < nMeasurements; i++) { 
    sqDevSum_DN += pow((DN_mean - double(sensorDNs[i])), 2);
    sqDevSum_volts += pow((volts_mean - double(sensorVoltages[i])), 2);
    sqDevSum_VWC += pow((VWC_mean - double(sensorVWCs[i])), 2);
  }
  double DN_stDev = sqrt(sqDevSum_DN/double(nMeasurements));
  double volts_stDev = sqrt(sqDevSum_volts/double(nMeasurements));
  double VWC_stDev = sqrt(sqDevSum_VWC/double(nMeasurements));

  // Setup the output struct
  result.analogValue = DN_mean;
  result.analogValue_sd = DN_stDev;
  result.voltage = volts_mean;
  result.voltage_sd = volts_stDev;
  result.VWC = VWC_mean;
  result.VWC_sd = VWC_stDev;

  now = rtc.now();
  result.timeStamp = now;
  
  // Return the result
  return(result);
}
