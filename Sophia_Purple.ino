#include <esp_now.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int sentenceButtonPin = 14;
const int ledButtonPin = 13;
const int ledPin = 25;

bool ledState = LOW;
int sentenceIndex = 0;
int currentList = 1;
bool listFinished = false;

// RSSI threshold
const int RSSI_THRESHOLD = -40;
bool rssiThresholdReached = false;

// Peer addresses
uint8_t peerAddresses[][6] = {
  {0x88, 0x13, 0xBF, 0x03, 0x3C, 0x58}, // Blue - Ivan (id 0)
  {0x88, 0x13, 0xBF, 0x03, 0x82, 0xEC}, // Pink - Lia (id 1)
  {0x88, 0x13, 0xBF, 0x03, 0x5B, 0xD8}  // Yellow - Leon (id 3)
};

const int NUM_PEERS = 3;
const uint8_t MY_ID = 2; 

typedef struct struct_message {
  uint8_t id;
  bool interactionComplete;
  int currentList;
  int sentenceIndex;
} struct_message;

struct_message myData;
struct_message incomingData[NUM_PEERS + 1];

// Sophia's Sentence lists
const char* sentences[][2] = {
  // List 1
  {"I'm a kind", "person"},
  {"I want to do", "good things"},
  {"Who am I?", ""},
  {"Click again", "then click right"},
  // List 2
  {"You are Sophia", ""},
  {"You know the", "artifact is"},
  {"too dangerous", "and must prevent"},
  {"anyone from", "accessing it"},
  {"You notice Lia's", "interest in"},
  {"the runes", ""},
  {"Click again", "then click right"},
  // List 3
  {"Silence", "Lia"},
  // List 4
  {"Scene", "Two"},
  {"Wait for", "others"},
  {"You see Ivan's", "strong interest"},
  {"in the artifact", "and decide to"},
  {"scare him", ""},
  {"Waiting for", "interaction"},
  {"Click again", "then click right"},
  // List 5
  {"Lie to", "Ivan"}, 
  {"You: If you", "touch the"},
  {"artifact, it", "will curse you"},
  {"with endless", "suffering"},
  // List 6
  {"Scene", "Three"},
  {"Wait for", "others"},
  {"You still try to", "convince Ivan"},
  {"to give up", "the artifact"},
  {"Waiting for", "interaction"},
  {"Click again", "then click right"},
  // List 7
  {"Tell Ivan", "the truth"},
  {"You: Ivan, if", "you keep chasing"},
  {"the artifact,", "we'll all face"},
  {"disaster. We", "must destroy it"},
  // List 8
  {"Last", "Scene"},
  {"Wait for", "others"},
  {"You destroys", "the artifact"},
  {"The ruins", "collapse"},
  {"You escape", "with Leon"},
  {"Story", "End"}
};

const int totalLists = 8;
int listStartIndices[totalLists] = {0, 4, 11, 12, 19, 23, 29, 33};
int listEndIndices[totalLists] = {3, 10, 11, 18, 22, 28, 32, 38};

void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingDataRaw, int len) {
  struct_message *incomingDataPtr = (struct_message *)incomingDataRaw;
  
  if (incomingDataPtr->id < NUM_PEERS + 1) {
    memcpy(&incomingData[incomingDataPtr->id], incomingDataPtr, sizeof(struct_message));
  }

  int rssi = esp_now_info->rx_ctrl->rssi;
  
  if (rssi > RSSI_THRESHOLD && !rssiThresholdReached) {
    if ((currentList == 3 && incomingDataPtr->id == 1) || // Interaction with Lia
        (currentList == 5 && incomingDataPtr->id == 0) || // Interaction with Ivan (Lie to Ivan)
        (currentList == 7 && incomingDataPtr->id == 0)) { // Interaction with Ivan (Tell Ivan the truth)
      rssiThresholdReached = true;
      handleSuccessfulInteraction();
    }
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
  Serial.println(sendStatus == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  pinMode(sentenceButtonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(ledButtonPin, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  for (int i = 0; i < NUM_PEERS; i++) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peerAddresses[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
    }
  }

  // Initialize myData
  myData.id = MY_ID;
  myData.interactionComplete = false;
  myData.currentList = currentList;
  myData.sentenceIndex = sentenceIndex;

  displaySentence();
}

void loop() {
  handleSentenceButton();
  handleLedButton();
  
  // Update myData
  myData.currentList = currentList;
  myData.sentenceIndex = sentenceIndex;
  myData.interactionComplete = rssiThresholdReached;

  // Send data continuously
  if (esp_now_send(NULL, (uint8_t*)&myData, sizeof(myData)) != ESP_OK) {
    Serial.println("Error sending data");
  }
  
  delay(100); // Adjust this delay to control how often data is sent
}

void displaySentence() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(sentences[listStartIndices[currentList-1] + sentenceIndex][0]);
  lcd.setCursor(0, 1);
  lcd.print(sentences[listStartIndices[currentList-1] + sentenceIndex][1]);

  if ((currentList == 3) || 
      (currentList == 5 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1]) ||
      (currentList == 7 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1])) {
    ledState = HIGH;
    digitalWrite(ledPin, HIGH);
  } else {
    ledState = LOW;
    digitalWrite(ledPin, LOW);
  }
}

void handleSuccessfulInteraction() {
  ledState = LOW;
  digitalWrite(ledPin, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Interaction");
  lcd.setCursor(0, 1);
  lcd.print("Success!");
  delay(2000);
  
  currentList++;
  sentenceIndex = 0;
  listFinished = false;
  rssiThresholdReached = false;
  displaySentence();
}

void handleSentenceButton() {
  if (digitalRead(sentenceButtonPin) == LOW) {
    delay(50);
    if (digitalRead(sentenceButtonPin) == LOW) {
      if (sentenceIndex < listEndIndices[currentList-1] - listStartIndices[currentList-1]) {
        sentenceIndex++;
        displaySentence();
      } else {
        listFinished = true;
      }
      
      while (digitalRead(sentenceButtonPin) == LOW);
    }
  }
}

void handleLedButton() {
  if (listFinished && digitalRead(ledButtonPin) == LOW) {
    delay(50);
    if (digitalRead(ledButtonPin) == LOW) {
      if ((currentList == 3) || 
          (currentList == 5 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1]) ||
          (currentList == 7 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1])) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Waiting for");
        lcd.setCursor(0, 1);
        lcd.print("interaction");
        delay(2000);
        displaySentence();
      } else {
        currentList++;
        if (currentList > totalLists) {
          currentList = 1;
        }
        sentenceIndex = 0;
        listFinished = false;
        rssiThresholdReached = false;
        displaySentence();
      }

      while (digitalRead(ledButtonPin) == LOW);
    }
  }
}