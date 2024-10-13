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
  {0x88, 0x13, 0xBF, 0x03, 0x2C, 0xB0}, // Purple - Sophia (id 2)
  {0x88, 0x13, 0xBF, 0x03, 0x5B, 0xD8}  // Yellow - Leon (id 3)
};

const uint8_t MY_ID = 1; 

typedef struct struct_message {
  uint8_t id;
  bool interactionComplete;
  int currentList;
  int sentenceIndex;
} struct_message;

struct_message myData;
struct_message incomingData[4];

// Sentence lists for Lia
const char* sentences[][2] = {
  // List 1
  {"I am a", "treasure hunter"},
  {"I can decode", "ancient symbols"},
  {"Who am I?", ""},
  {"I am Lia", ""},
  {"Click again", "then click right"},
  // List 2
  {"You can decode", "all the runes"},
  {"in the ruins", ""},
  {"Ivan has the", "lock code"},
  {"You'll work", "with him"},
  {"Click again", "then click right"},
  // List 3
  {"Tell Ivan the", "truth"},
  {"I: Ivan, I've", "decoded all"},
  {"the runes and", "know the"},
  {"passage location", "Will you help"},
  {"with your", "lock code?"},
  // List 4
  {"Scene", "Two"},
  {"Wait for", "others"},
  {"You are silenced", "this round"},
  {"You can't", "interact"},
  {"Waiting for", "silence to end"},
  {"Click again", "then click right"},
  // List 5
  {"Still silenced", ""},
  {"You can't speak", "or act now"},
  {"Observe the", "others"},
  {"Waiting for", "silence to end"},
  // List 6
  {"Scene", "Three"},
  {"Wait for", "others"},
  {"Your voice", "returns"},
  {"You can speak", "again"},
  {"The artifact", "is near"},
  {"Click again", "then click right"},
  // List 7
  {"Tell Leon the", "truth"},
  {"You: Don't", "worry Leon"},
  {"I've fully", "decoded the"},
  {"runes. We can", "activate the"},
  {"artifact", "safely"},
  {"Put your", "crystal balls"},
  {"together", ""},
  // List 8
  {"Last", "Scene"},
  {"Wait for", "others"},
  {"Sophia destroys", "the artifact"},
  {"The ruins", "collapse"},
  {"You are trapped", "in the ruins"},
  {"Story", "End"}
};

const int totalLists = 8;
int listStartIndices[totalLists] = {0, 5, 10, 15, 21, 25, 31, 38};
int listEndIndices[totalLists] = {4, 9, 14, 20, 24, 30, 37, 43};

void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingDataRaw, int len) {
  struct_message *incomingDataPtr = (struct_message *)incomingDataRaw;
  
  if (incomingDataPtr->id < 4) {
    memcpy(&incomingData[incomingDataPtr->id], incomingDataPtr, sizeof(struct_message));
  }

  int rssi = esp_now_info->rx_ctrl->rssi;
  
  if (rssi > RSSI_THRESHOLD) {
    if ((currentList == 3 && incomingDataPtr->id == 0) || // Interaction with Ivan
        (currentList == 7 && incomingDataPtr->id == 3)) { // Interaction with Leon
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

  for (int i = 0; i < 4; i++) {
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
  
  if ((currentList == 3 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1]) ||
      (currentList == 7 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1])) {
    ledState = HIGH;
    digitalWrite(ledPin, HIGH);
  } else {
    ledState = LOW;
    digitalWrite(ledPin, LOW);
  }
  
  delay(100); 
}

void displaySentence() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(sentences[listStartIndices[currentList-1] + sentenceIndex][0]);
  lcd.setCursor(0, 1);
  lcd.print(sentences[listStartIndices[currentList-1] + sentenceIndex][1]);

  if ((currentList == 3 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1]) ||
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
  
  if (currentList == 3) {
    currentList = 4;  // Move to Scene Two after interaction with Ivan
  } else if (currentList == 7) {
    currentList = 8;  // Move to final scene after interaction with Leon
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
      if ((currentList == 3 && sentenceIndex == listEndIndices[currentList-1] - listStartIndices[currentList-1]) ||
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