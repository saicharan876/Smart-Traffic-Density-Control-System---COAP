#include <WiFi.h>
#include <coap-simple.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// lanes / sensors / LEDs
#define LANES 4

int trigPins[LANES] = {5, 17, 16, 4};
int echoPins[LANES] = {18, 19, 21, 22};

int rgbPins[LANES][3] = {
  {25, 26, 27}, 
  {14, 12, 13}, 
  {32, 33, 23}, 
  {2, 15, 0}
};

int threshold = 10;

// CoAP client
WiFiUDP udp;
Coap coap(udp);

// CoAP server
IPAddress coapServer;
const char* coapHost = "coap.me";
const int coapPort = 5683;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000;

// --- Ultrasonic ---
long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

// --- RGB ---
void setRGB(int lane, bool R, bool G, bool B) {
  digitalWrite(rgbPins[lane][0], R ? HIGH : LOW);
  digitalWrite(rgbPins[lane][1], G ? HIGH : LOW);
  digitalWrite(rgbPins[lane][2], B ? HIGH : LOW);
}

// CoAP response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';

  Serial.println("\n=== CoAP Response Received ===");
  Serial.print("From: ");
  Serial.print(ip);
  Serial.print(":");
  Serial.println(port);
  Serial.print("Payload: ");
  Serial.println(p);
  Serial.println("============================\n");
}

// Send only lane data (no timestamp)
void sendLaneData(long distances[]) {
  char payload[200];
  snprintf(payload, sizeof(payload),
           "{\"lane1\":%ld,\"lane2\":%ld,\"lane3\":%ld,\"lane4\":%ld}",
           distances[0], distances[1], distances[2], distances[3]);

  Serial.println("\n--- Sending to coap.me ---");
  Serial.print("Payload: ");
  Serial.println(payload);

  coap.put(coapServer, coapPort, "sink", payload);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Setup pins
  for (int i = 0; i < LANES; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
    for (int j = 0; j < 3; j++) pinMode(rgbPins[i][j], OUTPUT);
  }

  // LEDs blue initially
  for (int i = 0; i < LANES; i++) setRGB(i, false, false, true);

  // WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Resolve coap.me
  Serial.print("Resolving coap.me... ");
  if (WiFi.hostByName(coapHost, coapServer)) {
    Serial.print("Resolved to: ");
    Serial.println(coapServer);
  } else {
    Serial.println("Failed to resolve! Using fallback.");
    coapServer.fromString("134.102.218.18");
  }

  // CoAP client
  coap.response(callback_response);
  coap.start();

  Serial.println("\nCoAP Client Started!");
  Serial.println("Sending lane data every 1 second...");

  lastSendTime = millis();
}

void loop() {
  coap.loop();

  unsigned long currentTime = millis();
  static long distance[LANES];

  // Measure all lanes
  for (int i = 0; i < LANES; i++) {
    distance[i] = getDistance(trigPins[i], echoPins[i]);
    delay(50);
  }

  // Print + LED control
  for (int i = 0; i < LANES; i++) {
    Serial.print("Lane ");
    Serial.print(i + 1);
    Serial.print(": ");

    if (distance[i] < 0) {
      Serial.print("No echo");
      setRGB(i, true, false, true);
    } else {
      Serial.print(distance[i]);
      Serial.print(" cm");
      if (distance[i] < threshold) {
        Serial.print(" | FULL");
        setRGB(i, true, false, false);
      } else {
        Serial.print(" | FREE");
        setRGB(i, false, true, false);
      }
    }
    Serial.println();
  }

  // Send every second
  if (currentTime - lastSendTime >= sendInterval) {
    lastSendTime = currentTime;
    sendLaneData(distance);
  }

  delay(10);
}
