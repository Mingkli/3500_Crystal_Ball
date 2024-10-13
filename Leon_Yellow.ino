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

// Unified MAC addresses
uint8_t peerAddresses[][6] = {
  {0x88, 0x13, 0xBF, 0x03, 0x3C, 0x58}, // Blue - Ivan (id 0)
  {0x88, 0x13, 0xBF, 0x03, 0x82, 0xEC}, // Pink - Lia (id 1)
  {0x88, 0x13, 0xBF, 0x03, 0x2C, 0xB0}, // Purple - Sophia (id 2)
  {0x88, 0x13, 0xBF, 0x03, 0x5B, 0xD8}  // Yellow - Leon (id 3)
};

const int NUM_PEERS = 4;
const uint8_t MY_ID = 3; // Leon's ID (0-based index)

typedef struct struct_message {
  uint8_t id;
  bool interactionComplete;
  int currentList;
  int sentenceIndex;
} struct_message;

struct_message myData;
struct_message incomingData[NUM_PEERS];


// Leon's Sentence lists
const char* sentences[][2] = {
  // List 1
  {"I have the", "map"},
  {"The map shows", "the passage"},
  {"Who am I?", ""},
  {"I am Leon", ""},
  {"Click again", "then click right"},
  // List 2
  {"You found the", "passage"},
  {"But you need", "Lia's help"},
  {"to decode", "the runes"},
  // List 3
  {"Tell Lia about", "the map"}, 
  {"You: Lia, I have", "the passage"},
  {"location, but", "I need you"},
  {"to decode the", "runes"},
  // List 4
  {"Scene", "Two"},
  {"Wait for", "others"},
  {"You don't want", "to interact"},
  {"You observe", "the others"},
  {"Click again", "then click right"},
  // List 5
  {"Scene", "Three"},
  {"Wait for", "others"},
  {"Click again", "then click right"},
  // List 6
  {"Last", "Scene"},
  {"Wait for", "others"},
  {"Sophia destroys", "the artifact"},
  {"The ruins", "collapse"},
  {"You escape", "with Sophia"},
  {"Story End", ""}
};

const int totalLists = 6;
int listStartIndices[totalLists] = {0, 5, 8, 12, 17, 20};
int listEndIndices[totalLists] = {4, 7, 11, 16, 19, 25};

bool interactionRequired = false;

void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingDataRaw, int len) {
  struct_message *incomingDataPtr = (struct_message *)incomingDataRaw;
  
  if (incomingDataPtr->id < NUM_PEERS) {
    memcpy(&incomingData[incomingDataPtr->id], incomingDataPtr, sizeof(struct_message));
  }

  int rssi = esp_now_info->rx_ctrl->rssi;
  
  if (rssi > RSSI_THRESHOLD && interactionRequired) {
    if ((currentList == 3 && incomingDataPtr->id == 1) || // Interaction with Lia
        (currentList == 5 && incomingDataPtr->id == 2)) { // Interaction with Sophia in Scene Three
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
    if (i != MY_ID) {
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, peerAddresses[i], 6);
      peerInfo.channel = 0;
      peerInfo.encrypt = false;
      if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
      }
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
  
  if ((currentList == 3 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1])) {
    interactionRequired = true;
    ledState = HIGH;
    digitalWrite(ledPin, HIGH);
  } else {
    interactionRequired = false;
    ledState = LOW;
    digitalWrite(ledPin, LOW);
  }
  
  delay(100); // Adjust this delay to control how often data is sent
}

void displaySentence() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(sentences[listStartIndices[currentList-1] + sentenceIndex][0]);
  lcd.setCursor(0, 1);
  lcd.print(sentences[listStartIndices[currentList-1] + sentenceIndex][1]);

  if ((currentList == 3 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1])) {
    interactionRequired = true;
    ledState = HIGH;
    digitalWrite(ledPin, HIGH);
  } else {
    interactionRequired = false;
    ledState = LOW;
    digitalWrite(ledPin, LOW);
  }
}

void handleSuccessfulInteraction() {
  interactionRequired = false;
  ledState = LOW;
  digitalWrite(ledPin, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Interaction");
  lcd.setCursor(0, 1);
  lcd.print("Success!");
  delay(2000);
  
  if (currentList == 3) {
    currentList = 4;  // Move to Scene Two after interaction with Lia
  } else if (currentList == 5) {
    currentList = 6;  // Move to final scene after interaction with Sophia
  }
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
      if (interactionRequired) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Waiting for");
        lcd.setCursor(0, 1);
        lcd.print("interaction");
        delay(2000);
        displaySentence();
      } else {
        moveToNextList();
      }

      while (digitalRead(ledButtonPin) == LOW);
    }
  }
}

void moveToNextList() {
  currentList++;
  if (currentList > totalLists) {
    currentList = 1;
  }
  sentenceIndex = 0;
  listFinished = false;
  rssiThresholdReached = false;
  displaySentence();
}