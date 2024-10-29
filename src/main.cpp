#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define FB_NO_FILE
#include <FastBot2.h>
FastBot2 bot;

#include <Preferences.h>
Preferences preferences;

const uint8_t PIN_BTN = 4;
const uint8_t PIN_CONTROL = 5;

const uint8_t NUM_LEDS = 1;
const uint8_t LED_PIN = 48;
CRGB leds[NUM_LEDS];
bool needLedToShutoff = false;
uint32_t blinkTimer = 0;

AsyncWebServer server(80);
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
bool isBotMode = false;
uint32_t botActivateTimer;
bool isBotActive = false;

void logToChat(String text, String username) 
{
  fb::Message msg;
  msg.text = username;
  msg.text += ": ";
  msg.text += text;
  msg.chatID = preferences.getString("logChatId");
  bot.sendMessage(msg);
}

void blink(CRGB color) 
{
  leds[0] = color;
  FastLED.show();
  blinkTimer = millis() + 200;
  needLedToShutoff = true;
}

void botSendMessage(String message, su::Value chatId) 
{
  fb::Message msg;
  msg.text = message;
  msg.chatID = chatId;
  bot.sendMessage(msg);
}

void doOpenOrClose(String initiator)
{
  blink(CRGB::Green);
  digitalWrite(PIN_CONTROL, 1);
  delay(100);
  digitalWrite(PIN_CONTROL, 0);
  logToChat("Открыто/Закрыто", initiator);
}

void doHelp(su::Value chatId) 
{
  String str = "";
  str += "Добавить пользователя (макс 30): \n/useradd nickname\n";
  str += "Удалить пользователя: \n/userdelete nickname\n";
  str += "Добавить администратора (макс 5): \n/adminadd nickname\n";
  str += "Удалить администратора: \n/admindelete nickname\n\n";
  str += "Чтобы узнать id чата для лога, сначала надо добавить бота в этот чат, после чего ввести в нём \n/getid\n\n";
  str += "Чтобы перенастроить подключение к сети - требуется перезагрузить устройство с зажатой кнопкой, подключиться к Wifi-сети ShlagbaumBot 12345678, зайти на адрес 192.168.1.1\n\n";
  fb::Message msg(str, chatId);
  bot.sendMessage(msg);
}

void doGetAdmins(su::Value chatId) 
{
  String str = "Список администраторов:\n";
  str += "@alextrof94\n";
  for (int i = 0; i < 5; i++) 
  {
    String adminKey = "admin" + String(i);
    if (preferences.isKey(adminKey.c_str())) 
    {
      String admin = preferences.getString(adminKey.c_str(), "");
      if (admin == "") { continue; }
      str += "@" + admin + "\n";
    }
  }
  botSendMessage(str, chatId);
}

void doGetUsers(su::Value chatId) 
{
  String str = "Список пользователей:\n";
  for (int i = 0; i < 30; i++) 
  {
    String userKey = "user" + String(i);
    if (preferences.isKey(userKey.c_str())) 
    {
      String user = preferences.getString(userKey.c_str(), "");
      if (user == "") { continue; }
      str += "@" + user + "\n";
    }
  }
  if (str == "Список пользователей:\n") 
  {
    str += "Пользователей нет";
  }
  botSendMessage(str, chatId);
}

bool getIsAdmin(String username) 
{
  if (username == "alextrof94") { return true; }
  for (int i = 0; i < 5; i++) 
  {
    String adminKey = "admin" + String(i);
    if (preferences.isKey(adminKey.c_str())) 
    {
      String admin = preferences.getString(adminKey.c_str(), "");
      if (admin == "") { continue; }
      if (admin == username) 
      {
        return true;
      }
    }
  }
  return false;
}

bool getIsUser(String username) 
{
  for (int i = 0; i < 30; i++) 
  {
    String userKey = "user" + String(i);
    if (preferences.isKey(userKey.c_str())) 
    {
      String user = preferences.getString(userKey.c_str(), "");
      if (user == "") { continue; }
      if (user == username) 
      {
        return true;
      }
    }
  }
  return false;
}

void deleteUserOrAdmin(String username, su::Value chatId, bool isNeedToDeleteAdmin, String initiator) 
{
  if (username == "") { return; }
  for (int i = 0; i < ((isNeedToDeleteAdmin) ? 5 : 30); i++) 
  {
    String userKey = ((isNeedToDeleteAdmin) ? "admin" : "user") + String(i);
    if (preferences.isKey(userKey.c_str()))
    {
      String user = preferences.getString(userKey.c_str(), "");
      if (user == username)
      {
        preferences.putString(userKey.c_str(), "");
        String msg = ((isNeedToDeleteAdmin) ? "Админ " : "Пользователь ");
        msg += username;
        msg += " успешно удалён";
        botSendMessage(msg, chatId);
        logToChat(msg, initiator);
        return;
      }
    }
  }
  String msg = ((isNeedToDeleteAdmin) ? "Админ " : "Пользователь ");
  msg += username;
  msg += " не найден";
  botSendMessage(msg, chatId);
}

void addUserOrAdmin(String username, su::Value chatId, bool isNewUserAdmin, String initiator) 
{
  if (username == "") { return; }

  for (int i = 0; i < ((isNewUserAdmin) ? 5 : 30); i++) 
  {
    String userKey = ((isNewUserAdmin) ? "admin" : "user") + String(i);
    if (preferences.isKey(userKey.c_str())) 
    {
      String user = preferences.getString(userKey.c_str(), "");
      if (user == username)
      {
        String msg = ((isNewUserAdmin) ? "Админ " : "Пользователь ");
        msg += username;
        msg += " уже был добавлен";
        botSendMessage(msg, chatId);
        return;
      }
    }
  }

  int freeIndex = -1;
  for (int i = 0; i < ((isNewUserAdmin) ? 5 : 30); i++) 
  {
    String userKey = ((isNewUserAdmin) ? "admin" : "user") + String(i);
    if (preferences.isKey(userKey.c_str())) 
    {
      String user = preferences.getString(userKey.c_str(), "");
      if (user != "") { continue; }
      freeIndex = i;
      break;
    }
    else 
    {
      freeIndex = i;
      break;
    }
  }
  if (freeIndex > -1) 
  {
    String userKey = ((isNewUserAdmin) ? "admin" : "user") + String(freeIndex);
    preferences.putString(userKey.c_str(), username);
    String msg = ((isNewUserAdmin) ? "Админ " : "Пользователь ");
    msg += username;
    msg += " успешно добавлен";
    botSendMessage(msg, chatId);
    logToChat(msg, initiator);
    return;
  }
  else 
  {
    String msg = "Достигнуто максимальное количество";
    msg += ((isNewUserAdmin) ? "админов" : "пользователей");
    botSendMessage(msg, chatId);
  }
}

void openMenu(su::Value chatId, bool isAdmin, String text = "") 
{
  String outText = "";
  if (text != "") {
    outText += text;
    outText += "\n\n";
  }
  outText += "Управление шлагбаумом";

  fb::Message msg(outText, chatId);

  String menuStr = "Открыть/Закрыть";
  String menuCmds = "openorclose";

  if (isAdmin) {
    menuStr += "\nПомощь по администрированию\nСписок администраторов\nСписок пользователей";
    menuCmds += ";help;getAdmins;getUsers";
  }

  fb::InlineMenu menu(menuStr, menuCmds);
  msg.setInlineMenu(menu);
  bot.sendMessage(msg);
}

void botUpdate(fb::Update& u) 
{
  if (!isBotActive) { return; }
  blink(CRGB::Blue);
  su::Value personalChatId = u.message().from().id();
  String thisChatId = u.message().chat().id().toString();
  String initiator = u.message().from().username();
  bool isAdmin = getIsAdmin(u.message().from().username());
  bool isUser = false;

  if (!isAdmin) 
  {
    isUser = getIsUser(u.message().from().username());
  }

  if (isAdmin || isUser) 
  {
    if (u.isMessage()) { // введено сообщение
      uint32_t userMessageId = u.message().id();
      if (u.message().text().startsWith("/useradd ") && isAdmin) {
        String newUser = u.message().text().substring(strlen("/useradd "));
        addUserOrAdmin(newUser, personalChatId, false, initiator);
      }
      else if (u.message().text().startsWith("/userdelete ") && isAdmin) 
      {
        String username = u.message().text().substring(strlen("/userdelete "));
        deleteUserOrAdmin(username, personalChatId, false, initiator);
      }
      else if (u.message().text().startsWith("/adminadd ") && isAdmin) 
      {
        String newUser = u.message().text().substring(strlen("/adminadd "));
        addUserOrAdmin(newUser, personalChatId, true, initiator);
      }
      else if (u.message().text().startsWith("/admindelete ") && isAdmin) 
      {
        String username = u.message().text().substring(strlen("/admindelete "));
        deleteUserOrAdmin(username, personalChatId, true, initiator);
      }
      else if (u.message().text().startsWith("/users") && isAdmin) 
      {
        doGetUsers(personalChatId);
      }
      else if (u.message().text().startsWith("/admins") && isAdmin) 
      {
        doGetAdmins(personalChatId);
      }
      else if (u.message().text().startsWith("/getid") && isAdmin) 
      {
        botSendMessage(thisChatId, personalChatId);
      }
      else 
      {
        bot.deleteMessage(personalChatId, userMessageId); // если белиберда - удалить сообщение пользователя
        openMenu(personalChatId, isAdmin);
      }
    }

    if (u.isQuery()) { // нажата кнопка в меню
      uint32_t queryMessageId = u.query().message().id();
      bot.deleteMessage(personalChatId, queryMessageId);
      switch (u.query().data().hash()) 
      {
        case su::SH("openorclose"):
          doOpenOrClose(initiator);
          bot.deleteMessage(personalChatId, queryMessageId);
          openMenu(personalChatId, isAdmin, "Открыто/Закрыто");
          break;
        case su::SH("help"):
          doHelp(personalChatId);
          openMenu(personalChatId, isAdmin);
          break;
        case su::SH("getAdmins"):
          doGetAdmins(personalChatId);
          openMenu(personalChatId, isAdmin);
          break;
        case su::SH("getUsers"):
          doGetUsers(personalChatId);
          openMenu(personalChatId, isAdmin);
          break;
      }
      bot.answerCallbackQuery(u.query().id()); // пометить прочитанным
    }
  }
  else 
  {
    botSendMessage("Вы не являетесь пользователем шлагбаум бота, попросите администратора вас добавить. Узнать админов можно в казанском чате.", personalChatId);
  }
}

void handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", R"rawliteral(
    <!DOCTYPE HTML><html>
    <head><title>ShalbaumBot Settings</title></head>
    <body>
        <h1>ShalbaumBot Settings</h1>
        <form action="/save">
            <label for="ssid">WIFI-SSID (name):</label><br>
            <input type="text" id="ssid" name="ssid"><br>
            <label for="pass">WIFI-Password:</label><br>
            <input type="password" id="pass" name="pass"><br><br>
            <label for="token">Bot token:</label><br>
            <input type="password" id="token" name="token"><br><br>
            <label for="log">Log Chat Id:</label><br>
            <input type="password" id="log" name="log"><br><br>
            <input type="submit" value="Save">
        </form>
    </body>
    </html>)rawliteral");
}

void handleSave(AsyncWebServerRequest *request) 
{
  if (request->hasParam("ssid") && request->hasParam("pass") && request->hasParam("token") && request->hasParam("log")) {
    String inputSSID = request->getParam("ssid")->value();
    String inputPassword = request->getParam("pass")->value();
    String inputToken = request->getParam("token")->value();
    String inputChatLogId = request->getParam("log")->value();

    if (inputSSID != "")
    {
      preferences.putString("ssid", inputSSID);
    }
    if (inputSSID != "")
    {
      preferences.putString("pass", inputPassword);
    }
    if (inputSSID != "")
    {
      preferences.putString("token", inputToken);
    }
    if (inputSSID != "")
    {
      preferences.putString("logChatId", inputChatLogId);
    }
    preferences.end();

    request->send(200, "text/html", "Saved. Reboot...");
    delay(2000);
    ESP.restart();
  } else {
    request->send(200, "text/html", "Отсутствуют параметры");
  }
}

void setup() {
  pinMode(PIN_BTN, INPUT_PULLDOWN);
  pinMode(PIN_CONTROL, OUTPUT);
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  preferences.begin("ShlagbaumBot", false);
  //preferences.putString("admin0", "nickname"); // добавление постоянных админов, 0 - индекс, максимум 5 (т.е. admin4)

  if (digitalRead(PIN_BTN) || !preferences.isKey("ssid") || !preferences.isKey("pass") || !preferences.isKey("token") || !preferences.isKey("logChatId"))
  {
    isBotMode = false;
    blink(CRGB::White);
    WiFi.softAP("ShlagbaumBot", "12345678");
    WiFi.softAPConfig(local_ip, gateway, subnet);
    IPAddress IP = WiFi.softAPIP();
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_GET, handleSave);
    server.begin();
  }
  else 
  {
    blink(CRGB::Red);
    WiFi.begin(preferences.getString("ssid"), preferences.getString("pass"));
    while (WiFi.status() != WL_CONNECTED) 
    {
      delay(200);
    }

    bot.setToken(preferences.getString("token"));
    bot.setPollMode(fb::Poll::Long, 30000);
    bot.attachUpdate(botUpdate);
    isBotMode = true;
    botActivateTimer = millis() + 5000;
    logToChat("Устройство перезагружено", "Бот");
  }  
}

void loop() {
  if (needLedToShutoff && millis() > blinkTimer) 
  {
    leds[0] = CRGB::Black;
    FastLED.show();
    needLedToShutoff = false;
  }
  if (!isBotActive && millis() > botActivateTimer) 
  {
    isBotActive = true;
  }
  if (isBotMode) 
  {
    if (digitalRead(PIN_BTN)) {
      doOpenOrClose("Кнопка");
      delay(1000);
    }
    bot.tick();
  }
}