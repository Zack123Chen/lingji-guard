#include "Air780EManager.h"

#include <math.h>

#include "Pins.h"

namespace Air780EManager {
namespace {
constexpr uint32_t kInitTimeoutMs = 1200;
constexpr uint32_t kPollTimeoutMs = 300;
constexpr uint32_t kNetworkTimeoutMs = 1200;
constexpr uint32_t kMqttStartTimeoutMs = 8000;
constexpr uint32_t kMqttConnectTimeoutMs = 5000;
constexpr uint32_t kMqttPublishTimeoutMs = 5000;
constexpr uint32_t kMqttCloseTimeoutMs = 2500;
constexpr uint32_t kNoResponseBackoffMs = 30000;
constexpr uint16_t kResponseQuietMs = 80;
constexpr const char* kMqttClientId = "CareGuardESP32S3";
constexpr const char* kMqttHost = "broker-cn.emqx.io";
constexpr const char* kMqttPort = "1883";
constexpr const char* kMqttTopic = "HIT/PetData";
constexpr const char* kMqttControlTopic = "HIT/PetControl";

ModuleState g_air_state;
GnssData g_gnss;
bool g_mqtt_connected = false;
bool g_control_subscribed = false;
bool g_mqtt_cleanup_required = true;
uint32_t g_publish_count = 0;
uint32_t g_publish_fail_count = 0;
uint32_t g_no_response_until_ms = 0;
String g_async_buffer;
RemoteCommandEvent g_pending_command;
bool g_has_pending_command = false;

bool hasTerminalResponse(const String& response) {
  return response.indexOf("\r\nOK") >= 0 || response.indexOf("\nOK") >= 0 ||
         response.indexOf("ERROR") >= 0 || response.indexOf("CONNACK OK") >= 0 ||
         response.indexOf("CONNECT OK") >= 0 || response.indexOf("ALREADY CONNECT") >= 0;
}

RemoteCommand commandFromCode(String code) {
  code.trim();
  code.toUpperCase();
  code.replace("-", "_");
  if (code == "BEEP") {
    return RemoteCommand::Beep;
  }
  if (code == "LIGHT_ON" || code == "LIGHT") {
    return RemoteCommand::LightOn;
  }
  if (code == "ECO_MODE" || code == "ECO") {
    return RemoteCommand::EcoMode;
  }
  if (code == "NORMAL_MODE" || code == "NORMAL") {
    return RemoteCommand::NormalMode;
  }
  if (code == "CAPTURE" || code == "SNAPSHOT" || code == "SNAP") {
    return RemoteCommand::Capture;
  }
  if (code == "STATUS" || code == "PUB") {
    return RemoteCommand::Status;
  }
  return RemoteCommand::None;
}

String extractJsonString(const String& payload, const char* key) {
  String quoted_key = "\"";
  quoted_key += key;
  quoted_key += "\"";
  int key_index = payload.indexOf(quoted_key);
  if (key_index < 0) {
    return "";
  }
  int colon = payload.indexOf(':', key_index + quoted_key.length());
  if (colon < 0) {
    return "";
  }
  int first_quote = payload.indexOf('"', colon + 1);
  if (first_quote < 0) {
    return "";
  }
  String value;
  for (int i = first_quote + 1; i < static_cast<int>(payload.length()); ++i) {
    const char ch = payload[i];
    if (ch == '\\' && i + 1 < static_cast<int>(payload.length())) {
      ++i;
      value += payload[i];
      continue;
    }
    if (ch == '"') {
      return value;
    }
    value += ch;
  }
  return "";
}

String extractCommandCode(const String& payload) {
  String code = extractJsonString(payload, "command");
  if (code.length() == 0) {
    code = extractJsonString(payload, "cmd");
  }
  if (code.length() == 0) {
    code = extractJsonString(payload, "code");
  }
  if (code.length() == 0) {
    code = payload;
  }
  code.trim();
  if (code.startsWith("{") || code.indexOf(':') >= 0) {
    return "";
  }
  code.replace("\"", "");
  return code;
}

void enqueueCommand(const String& payload) {
  const String code = extractCommandCode(payload);
  const RemoteCommand command = commandFromCode(code);
  if (command == RemoteCommand::None) {
    Serial.printf("[Air780E] ignored unknown control payload: %s\n", payload.c_str());
    return;
  }

  g_pending_command.command = command;
  g_pending_command.code = code;
  g_pending_command.raw = payload;
  g_pending_command.received_at_ms = millis();
  g_has_pending_command = true;
  Serial.printf("[Air780E] queued control command: %s\n", code.c_str());
}

void parseMsubLine(String line) {
  line.trim();
  if (line.indexOf("+MSUB:") < 0 || line.indexOf(kMqttControlTopic) < 0) {
    return;
  }

  int payload_index = line.indexOf("byte,");
  if (payload_index >= 0) {
    payload_index += 5;
  } else {
    int comma = line.indexOf(',');
    comma = comma >= 0 ? line.indexOf(',', comma + 1) : -1;
    payload_index = comma >= 0 ? comma + 1 : -1;
  }
  if (payload_index < 0 || payload_index >= static_cast<int>(line.length())) {
    return;
  }

  String payload = line.substring(payload_index);
  payload.trim();
  enqueueCommand(payload);
}

void parseDownlinkFromText(const String& text) {
  int index = text.indexOf("+MSUB:");
  while (index >= 0) {
    int end = text.indexOf('\n', index);
    if (end < 0) {
      end = text.length();
    }
    parseMsubLine(text.substring(index, end));
    index = text.indexOf("+MSUB:", end);
  }
}

void pumpAsyncSerial() {
  while (Serial1.available() > 0) {
    const char ch = static_cast<char>(Serial1.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      parseMsubLine(g_async_buffer);
      g_async_buffer = "";
      continue;
    }
    if (g_async_buffer.length() < 512) {
      g_async_buffer += ch;
    } else {
      g_async_buffer = "";
    }
  }
}

String readResponse(uint32_t timeout_ms) {
  String response;
  const uint32_t start = millis();
  uint32_t last_rx_ms = start;
  while (millis() - start < timeout_ms) {
    while (Serial1.available() > 0) {
      response += static_cast<char>(Serial1.read());
      last_rx_ms = millis();
    }
    if (response.length() > 0 && hasTerminalResponse(response) &&
        millis() - last_rx_ms >= kResponseQuietMs) {
      break;
    }
    delay(5);
  }
  response.trim();
  parseDownlinkFromText(response);
  return response;
}

String fieldAt(const String& csv, uint8_t target_index) {
  int start = 0;
  for (uint8_t i = 0; i < target_index; ++i) {
    start = csv.indexOf(',', start);
    if (start < 0) {
      return "";
    }
    ++start;
  }

  int end = csv.indexOf(',', start);
  if (end < 0) {
    end = csv.length();
  }
  String field = csv.substring(start, end);
  field.trim();
  return field;
}

void parseGnss(const String& cgnsinf) {
  g_gnss.has_fix = false;
  g_gnss.lat = NAN;
  g_gnss.lng = NAN;

  const int gnss_index = cgnsinf.indexOf("+CGNSINF:");
  if (gnss_index < 0) {
    return;
  }

  const int colon_index = cgnsinf.indexOf(':', gnss_index);
  if (colon_index < 0) {
    return;
  }

  String payload = cgnsinf.substring(colon_index + 1);
  payload.trim();
  const String fix_status = fieldAt(payload, 1);
  const String lat = fieldAt(payload, 3);
  const String lng = fieldAt(payload, 4);

  if (fix_status == "1" && lat.length() > 0 && lng.length() > 0) {
    g_gnss.lat = lat.toDouble();
    g_gnss.lng = lng.toDouble();
    g_gnss.has_fix = !(isnan(g_gnss.lat) || isnan(g_gnss.lng));
  }
}

String escapeAtQuotedPayload(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 16);
  for (size_t i = 0; i < value.length(); ++i) {
    const char ch = value[i];
    if (ch == '"') {
      escaped += "\\22";
    } else if (ch == '\\') {
      escaped += "\\5C";
    } else if (ch == '\r') {
      escaped += "\\0D";
    } else if (ch == '\n') {
      escaped += "\\0A";
    } else {
      escaped += ch;
    }
  }
  return escaped;
}

void classifyStatus(const String& at, const String& cpin, const String& cgnsinf) {
  if (at.indexOf("OK") < 0) {
    g_air_state.ready = false;
    g_air_state.status = "No Response";
    g_mqtt_connected = false;
    g_no_response_until_ms = millis() + kNoResponseBackoffMs;
    return;
  }

  g_no_response_until_ms = 0;
  g_air_state.ready = true;
  if (cpin.indexOf("READY") < 0) {
    g_air_state.status = "SIM Error";
    g_mqtt_connected = false;
    return;
  }

  parseGnss(cgnsinf);
  if (g_mqtt_connected) {
    g_air_state.status = "MQTT OK";
  } else {
    g_air_state.status = "Net Ready";
  }
}

bool checkNetworkReady() {
  const String at = sendCommand("AT", kNetworkTimeoutMs);
  const String cpin = sendCommand("AT+CPIN?", kNetworkTimeoutMs);
  const String cgatt = sendCommand("AT+CGATT?", kNetworkTimeoutMs);
  const String csq = sendCommand("AT+CSQ", kNetworkTimeoutMs);
  const String cgnsinf = sendCommand("AT+CGNSINF", 1600);
  (void)csq;

  classifyStatus(at, cpin, cgnsinf);
  if (g_air_state.status == "SIM Error" || g_air_state.status == "No Response") {
    return false;
  }
  if (cgatt.indexOf("+CGATT: 1") < 0 && cgatt.indexOf("+CGATT:1") < 0) {
    g_air_state.status = "MQTT Offline";
    g_mqtt_connected = false;
    return false;
  }
  g_air_state.status = g_mqtt_connected ? "MQTT OK" : "Net Ready";
  return true;
}

bool responseOk(const String& response) {
  return response.indexOf("OK") >= 0 || response.indexOf("CONNACK OK") >= 0 ||
         response.indexOf("CONNECT OK") >= 0 || response.indexOf("ALREADY CONNECT") >= 0;
}

void markMqttDisconnected(const char* status) {
  g_air_state.status = status;
  g_mqtt_connected = false;
  g_control_subscribed = false;
  g_mqtt_cleanup_required = true;
}

void appendAsyncResponse(String& response, const char* label, uint32_t timeout_ms) {
  const String extra = readResponse(timeout_ms);
  if (extra.length() == 0) {
    return;
  }

  Serial.printf("[Air780E] << %s\n", label);
  Serial.println(extra);
  response += "\n";
  response += extra;
}

void resetMqttSession() {
  Serial.println("[Air780E] resetting stale MQTT/TCP session...");
  sendCommand("AT+MDISCONNECT", kMqttCloseTimeoutMs);
  sendCommand("AT+MIPCLOSE", kMqttCloseTimeoutMs);
  g_mqtt_connected = false;
  g_control_subscribed = false;
  g_mqtt_cleanup_required = false;
}

bool tcpOpenOk(String& response) {
  if (response.indexOf("CONNECT OK") >= 0 || response.indexOf("ALREADY CONNECT") >= 0) {
    return true;
  }

  if (response.indexOf("OK") >= 0 && response.indexOf("ERROR") < 0) {
    Serial.println("[Air780E] waiting for TCP CONNECT OK after MIPSTART...");
    appendAsyncResponse(response, "MIPSTART async", kMqttStartTimeoutMs);
  }

  return response.indexOf("CONNECT OK") >= 0 || response.indexOf("ALREADY CONNECT") >= 0;
}

bool subscribeControlTopic() {
  if (g_control_subscribed) {
    return true;
  }

  String command = "AT+MSUB=\"";
  command += kMqttControlTopic;
  command += "\",0";
  String response = sendCommand(command.c_str(), kMqttConnectTimeoutMs);
  if (response.indexOf("SUBACK") < 0 && response.indexOf("OK") >= 0 &&
      response.indexOf("ERROR") < 0) {
    appendAsyncResponse(response, "MSUB async", kMqttConnectTimeoutMs);
  }
  if (response.indexOf("SUBACK") < 0) {
    g_control_subscribed = false;
    Serial.println("[Air780E] control topic subscribe failed.");
    return false;
  }

  g_control_subscribed = true;
  Serial.printf("[Air780E] subscribed control topic: %s\n", kMqttControlTopic);
  return true;
}

bool mqttConnectOk(String& response) {
  if (response.indexOf("CONNACK OK") >= 0) {
    return true;
  }

  if (response.indexOf("+CME ERROR: 767") >= 0) {
    Serial.println("[Air780E] MCONNECT is busy, waiting for async CONNECT OK...");
    appendAsyncResponse(response, "MCONNECT async", kMqttConnectTimeoutMs + 2000);
  } else if (response.indexOf("OK") >= 0 && response.indexOf("ERROR") < 0) {
    Serial.println("[Air780E] waiting for MQTT CONNACK OK after MCONNECT...");
    appendAsyncResponse(response, "MCONNECT async", kMqttConnectTimeoutMs + 2000);
  }

  return response.indexOf("CONNACK OK") >= 0;
}
}  // namespace

String sendCommand(const char* command, uint32_t timeout_ms) {
  pumpAsyncSerial();

  Serial.printf("[Air780E] >> %s\n", command);
  Serial1.print(command);
  Serial1.print("\r\n");

  const String response = readResponse(timeout_ms);
  if (response.length() == 0) {
    Serial.println("[Air780E] << <no response>");
  } else {
    Serial.println("[Air780E] <<");
    Serial.println(response);
  }
  return response;
}

void init() {
  pinMode(Pins::Air780E::EN, OUTPUT);
  pinMode(Pins::Air780E::PWRON, OUTPUT);
  digitalWrite(Pins::Air780E::EN, HIGH);
  digitalWrite(Pins::Air780E::PWRON, HIGH);
  delay(300);
  digitalWrite(Pins::Air780E::PWRON, LOW);
  delay(1200);
  digitalWrite(Pins::Air780E::PWRON, HIGH);
  delay(3000);

  Serial1.begin(Pins::kBaudRate, SERIAL_8N1, Pins::Air780E::RX, Pins::Air780E::TX);
  delay(200);

  const String at = sendCommand("AT", kInitTimeoutMs);
  const String ati = sendCommand("ATI", kInitTimeoutMs);
  const String cpin = sendCommand("AT+CPIN?", kInitTimeoutMs);
  const String csq = sendCommand("AT+CSQ", kInitTimeoutMs);
  const String cgnspwr = sendCommand("AT+CGNSPWR=1", kInitTimeoutMs);
  const String cgnsinf = sendCommand("AT+CGNSINF", 1800);
  (void)ati;
  (void)csq;
  (void)cgnspwr;

  classifyStatus(at, cpin, cgnsinf);
  printStatus();
}

void queryStatus() {
  const String at = sendCommand("AT", kPollTimeoutMs);
  const String cpin = sendCommand("AT+CPIN?", kPollTimeoutMs);
  const String csq = sendCommand("AT+CSQ", kPollTimeoutMs);
  const String cgnsinf = sendCommand("AT+CGNSINF", 700);
  (void)csq;
  classifyStatus(at, cpin, cgnsinf);
}

bool ensureMqttConnected() {
  if (g_mqtt_connected) {
    return true;
  }

  if (!checkNetworkReady()) {
    Serial.println("[Air780E] MQTT network is not ready.");
    return false;
  }

  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    if (g_mqtt_cleanup_required || attempt > 0) {
      resetMqttSession();
    }

    String command = "AT+MCONFIG=\"";
    command += kMqttClientId;
    command += "\",\"\",\"\"";
    String response = sendCommand(command.c_str(), kMqttConnectTimeoutMs);
    if (!responseOk(response)) {
      markMqttDisconnected("MQTT Offline");
      continue;
    }

    command = "AT+MIPSTART=\"";
    command += kMqttHost;
    command += "\",\"";
    command += kMqttPort;
    command += "\"";
    response = sendCommand(command.c_str(), kMqttStartTimeoutMs);
    if (!tcpOpenOk(response)) {
      markMqttDisconnected("MQTT Offline");
      continue;
    }

    response = sendCommand("AT+MCONNECT=1,60", kMqttConnectTimeoutMs);
    if (!mqttConnectOk(response)) {
      markMqttDisconnected("MQTT Offline");
      continue;
    }

    g_air_state.ready = true;
    g_air_state.status = "MQTT OK";
    g_mqtt_connected = true;
    g_mqtt_cleanup_required = false;
    subscribeControlTopic();
    return true;
  }

  markMqttDisconnected("MQTT Offline");
  return false;
}

bool canAttemptAutoPublish() {
  return g_no_response_until_ms == 0 || millis() >= g_no_response_until_ms;
}

bool publishTelemetry(const String& json) {
  pumpAsyncSerial();
  Serial.print("Telemetry JSON: ");
  Serial.println(json);
  Serial.printf("[Air780E] MQTT topic=%s publish_count=%lu fail_count=%lu\n",
                kMqttTopic,
                static_cast<unsigned long>(g_publish_count),
                static_cast<unsigned long>(g_publish_fail_count));

  if (!ensureMqttConnected()) {
    ++g_publish_fail_count;
    g_air_state.status = "Pub Fail";
    return false;
  }

  String command = "AT+MPUB=\"";
  command += kMqttTopic;
  command += "\",0,0,\"";
  command += escapeAtQuotedPayload(json);
  command += "\"";

  const String response = sendCommand(command.c_str(), kMqttPublishTimeoutMs);
  if (response.indexOf("ERROR") >= 0 || !responseOk(response)) {
    ++g_publish_fail_count;
    markMqttDisconnected("Pub Fail");
    return false;
  }

  ++g_publish_count;
  g_mqtt_connected = true;
  g_air_state.status = "Pub OK";
  subscribeControlTopic();
  pumpAsyncSerial();
  return true;
}

bool pollDownlink(RemoteCommandEvent& event) {
  pumpAsyncSerial();
  if (!g_has_pending_command) {
    return false;
  }
  event = g_pending_command;
  g_pending_command = {};
  g_has_pending_command = false;
  return true;
}

void printStatus() {
  Serial.printf("Air780E status: %s | GNSS: %s", g_air_state.status.c_str(), g_gnss.has_fix ? "fixed" : "searching");
  if (g_gnss.has_fix) {
    Serial.printf(" lat=%.6f lng=%.6f", g_gnss.lat, g_gnss.lng);
  }
  Serial.println();
}

const ModuleState& state() {
  return g_air_state;
}

const GnssData& gnss() {
  return g_gnss;
}
}  // namespace Air780EManager
