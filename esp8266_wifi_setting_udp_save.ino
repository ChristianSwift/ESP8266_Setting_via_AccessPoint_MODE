#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <FS.h>  
#include <WiFiUdp.h>

//定义使用的引脚
#define RSTSIG 2
#define SIGNAL 4
#define RESETP 5

//重置计数器
int count=0;

//创建DNS
DNSServer dnsServer;
IPAddress GetwayIP(192, 168, 4, 1);

//提供设置连接用的服务器
ESP8266WebServer httpServer;

//AP接入点设置
const char * AP_SSID = "Lock_Setting";

//初始化WiFi信息变量
String ssid = "";
String code = "";

//HTML页面设置
String html;

//UDP Setting
WiFiUDP Udp;
unsigned int localUdpPort = 2048;
char incomingPacket[537];

/** 
 * 根据文件后缀获取html协议的返回内容类型 
 */  
String getContentType(String filename){  
  if(httpServer.hasArg("download")) return "application/octet-stream";  
  else if(filename.endsWith(".htm")) return "text/html";  
  else if(filename.endsWith(".html")) return "text/html";  
  else if(filename.endsWith(".css")) return "text/css";  
  else if(filename.endsWith(".js")) return "application/javascript";  
  else if(filename.endsWith(".png")) return "image/png";  
  else if(filename.endsWith(".gif")) return "image/gif";  
  else if(filename.endsWith(".jpg")) return "image/jpeg";  
  else if(filename.endsWith(".ico")) return "image/x-icon";  
  else if(filename.endsWith(".xml")) return "text/xml";  
  else if(filename.endsWith(".pdf")) return "application/x-pdf";  
  else if(filename.endsWith(".zip")) return "application/x-zip";  
  else if(filename.endsWith(".gz")) return "application/x-gzip";  
  return "text/plain";  
}  
/* NotFound处理 
 * 用于处理没有注册的请求地址 
 * 一般是处理一些页面请求 
 */  
void handleNotFound() {  
  String path = httpServer.uri();  
  Serial.print("load url:");  
  Serial.println(path);  
  String contentType = getContentType(path);  
  String pathWithGz = path + ".gz";  
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  
    if(SPIFFS.exists(pathWithGz))  
      path += ".gz";  
    File file = SPIFFS.open(path, "r");  
    size_t sent = httpServer.streamFile(file, contentType);  
    file.close();  
    return;  
  }  
  String message = "File Not Found\n\n";  
  message += "URI: ";  
  message += httpServer.uri();  
  message += "\nMethod: ";  
  message += ( httpServer.method() == HTTP_GET ) ? "GET" : "POST";  
  message += "\nArguments: ";  
  message += httpServer.args();  
  message += "\n";  
  for ( uint8_t i = 0; i < httpServer.args(); i++ ) {  
    message += " " + httpServer.argName ( i ) + ": " + httpServer.arg ( i ) + "\n";  
  }  
  httpServer.send ( 404, "text/plain", message );  
}

void setup() {
  pinMode(RSTSIG, OUTPUT);
  pinMode(SIGNAL, OUTPUT);
  pinMode(RESETP, INPUT_PULLUP);
  digitalWrite(RSTSIG, LOW);
  Serial.begin(115200);
  SPIFFS.begin();  
  EEPROM.begin(512);
  delay(10);
  
  Serial.println("\n\n\n\n");
  
  //从EEPROM中读取WiFi名称信息
  for(int i=0;i<32;i++){
    ssid += char(EEPROM.read(i));
  }
  Serial.print("Stored SSID Name:");
  Serial.println(ssid);
  //从EEPROM中读取WiFi密码信息
  for(int i=32;i<96; i++){
    code += char(EEPROM.read(i));
  }
  Serial.print("Stored Password:");
  Serial.println(code);
  
  //连接网络
  connectWiFi();

  //显示连接成功的信息
  digitalWrite(RSTSIG, HIGH);
  Serial.println("Connected!");
  Serial.print("IP: ");
  Serial.print(WiFi.localIP());
  //完成设置后断开连接
  WiFi.softAPdisconnect(true);
  //以下开启UDP监听并打印输出信息
  Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
}

//连接Wi-Fi的方法
void connectWiFi(){
  Serial.print("\nVerifing WiFi Connection:");
    //用EEPROM中的Wi-Fi名称和密码来连接
    WiFi.begin(ssid.c_str(),code.c_str());
    //定义100ms为连接超时
    int c = 0;
    while(c < 100){
      if(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(100);
        c ++;
      }
      else{
        return;
      }
     }
    Serial.println();
    Serial.print("WiFi Connect Timeout!");
    //重新设定WiFi环境
    resetWifi();

}

//配置Wi-Fi信息的方法
void resetWifi(){
  Serial.print("\nInitial Setting!\n");
  //创建DNS服务器
  Serial.print("\nInitial DNS Server!");
  dnsServer.setTTL(300);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, "set.wifi", GetwayIP);
  //设置Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  //搜索网络
  int networks = WiFi.scanNetworks();
  Serial.print("All networks: ");
  Serial.println(networks);
  //创建Option标签供用户选择
  String htmlList = "<select id='ssid'><option value='' disabled selected>ホットスポットの名</option>";
  for(int i = 0;i<networks;i++){
    String scanSSID = WiFi.SSID(i);
    int scanRSSI = WiFi.RSSI(i);
    Serial.print("SSID Names:");
    Serial.println(scanSSID);
    Serial.print("SSID Signals:");
    Serial.println(scanRSSI);
    htmlList +="<option value='"+scanSSID+"'>"+scanSSID + " : "+scanRSSI+"</option>";
  }
  htmlList+="</select>";
  Serial.println("Searching finished!");
  delay(100);
  //创建AP供设置用户连接
  WiFi.softAP(AP_SSID);
  Serial.print("AP Activited! IP:");
  Serial.println(WiFi.softAPIP());
  bool setup = true;
  //以HTML方式显示WiFi和设置信息
  html+= "<html><head><title>WiFi コンフィギュレーション</title><meta http-equiv='Content-Type' content='text/html;charset=utf-8'><link type='text/css' rel='stylesheet' href='materialize.min.css' media='screen,projection'/><link type='text/css' rel='stylesheet' href='customize.css' media='screen,projection'/></head><body><form method='get' action='setup' class='z-depth-3 grey lighten-5 card' style='width:500px;height:300px;position: absolute;left:50%;margin-left:-250px;top:50%;margin-top:-150px;'><div class='card-content'><div class='container'><div class='row'><center><span class='card-title activator grey-text text-darken-4'>Wi-Fi初期化設定</span></center><div class='input-field col s12'>";
  html+= htmlList;
  html+= "</div><div class='input-field col s12'><label>ホットスポットのパスワード</label><input id='code' type='password' length=64 type='text' /></div><center><a class='waves-effect waves-green btn-flat submit' onclick='updateWiFi()' style='width:94%'>今すぐ接続する！</a></center></div></div></div><div class='card-reveal'><div class='container'><div class='row'><center><span class='card-title grey-text text-darken-4'>通知</span></center></div><div class='row'><p>セットアップが完了しました,青色の点滅後にリセットボタンを押す!</p></div></div></div></form><script type='text/javascript' src='jquery.min.js'></script><script type='text/javascript' src='materialize.min.js'></script><script type='text/javascript'>$(document).ready(function(){$('select').material_select();});function updateWiFi(){$.get('setup', { ssid: $('#ssid').val(), code: $('#code').val()} );$('.activator').click()};</script></body></html>";
  //激活HTTP服务器
  httpServer.on("/",[](){
    Serial.println("Configuration Page");
    httpServer.send(200,"text/html",html);
  });
  //提示可以进行设置了
  digitalWrite(RSTSIG, HIGH);
  delay(200);
  digitalWrite(RSTSIG, LOW);
  delay(200);
  digitalWrite(RSTSIG, HIGH);
  //接受设置数据
  httpServer.on("/setup",[](){
    String setupSSID = httpServer.arg("ssid");
    String setupcode = httpServer.arg("code");
    httpServer.send(200,"text/html","{'status':'OK'}");
    Serial.print("\nWrite SSID&Password\nSSID: ");
    Serial.println(setupSSID);
    Serial.print("CODE: \n");
    Serial.println(setupcode);
    //重置EEPROM内存数据
    for(int i=0;i<96;i++){
      EEPROM.write(i, 0);
    }
    
    //写入SSID名称数据
    for(int i =0;i< setupSSID.length();i++){
      EEPROM.write(i,setupSSID[i]);
    }
    //写入WiFi密码数据
    for(int i=0;i<setupcode.length();i++){
      EEPROM.write(i+32,setupcode[i]);
    }
    //将修改内容保存进EEPROM
    EEPROM.commit();
    Serial.println("Data saved!");
    //重置开发板提示
    while(1){
      digitalWrite(RSTSIG, LOW);
      delay(200);
      digitalWrite(RSTSIG, HIGH);
      delay(200);
    }
    //ESP.reset();
  });
  //寻找资源或返回404
  httpServer.onNotFound(handleNotFound);
  //启动HTTP服务器
  httpServer.begin();
  //进入循环
  while(setup){
    dnsServer.processNextRequest();
    httpServer.handleClient();
  }
}

void loop() {
  int packetSize = Udp.parsePacket(); //获取当前队首数据包长度
  if (packetSize)                     // 有数据可用
  {
    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    int len = Udp.read(incomingPacket, 536); // 读取数据到incomingPacket
    if (len > 0)                             // 如果正确读取
    {
      incomingPacket[len] = 0; //末尾补0结束字符串
      Serial.printf("UDP packet contents: %s\n", incomingPacket);

      if (strcmp(incomingPacket, "Unlock") == 0) // 如果收到Turn off
      {
        digitalWrite(SIGNAL, HIGH);
        delay(100);
        digitalWrite(SIGNAL, LOW);
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write("Door has opened!"); // 回复LED has been turn off
        Udp.endPacket();
      }
      else // 如果非指定消息
      {
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write("Data Error!"); // 回复Data Error!
        Udp.endPacket();
      }
    }
  }
  else if (digitalRead(RESETP)==0)
  {
    delay(50);
    if (count==0)
    {
      Serial.printf("Reset Wi-Fi Event Detected!");
      count++;
      resetWifi();
    }
  }
}
