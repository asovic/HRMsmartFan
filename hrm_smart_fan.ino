/**
   HRM smart fan
   Controlling fan speed based on heart rate
*/

#include "BLEDevice.h"
#include <RBDdimmer.h>

const String sketchName = "HRM Smart Fan";
unsigned char PWM = 23;
unsigned char ZEROCROSS = 4;
unsigned char power = 0;    // Power level (0-100)
unsigned char old_power = 0;    // Power level (0-100)
boolean enable = false;       // Dimming not enabled by default. Will need to reach min HR
dimmerLamp dimmer(PWM, ZEROCROSS);

// The remote service we wish to connect to.
static BLEUUID serviceUUID(BLEUUID((uint16_t)0x180D));
// The characteristic of the remote service we are interested in.
// this is the standard ID of the HEART RATE characteristic
static BLEUUID charUUID(BLEUUID((uint16_t)0x2A37));

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static boolean notification = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

// TypeDef to receive Heart Rate data
typedef struct {
  char ID[20];
  uint16_t HRM;
} HRM;
HRM hrm;

const int maxHR = 188;

// connect LED to pin 13 (PWM capable). LED will breathe with period of
// 2000ms and a delay of 1000ms after each period.
// flashing pattern will be updated in accordance with Bluetooth connection status
//auto led = JLed(17).Breathe(2000).DelayAfter(1000).Forever();

//--------------------------------------------------------------------------------------------
// Setup the Serial Port and output Sketch name and compile date
//--------------------------------------------------------------------------------------------
void startSerial(uint32_t baud) {

  // Setup Serial Port aut 115200 Baud
  Serial.begin(baud);

  delay(10);

  Serial.println();
  Serial.print(sketchName);
  Serial.print(F(" | Compiled: "));
  Serial.print(__DATE__);
  Serial.print(F("/"));
  Serial.println(__TIME__);
} // End of startSerial



static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.print(length);
  Serial.print(" data: ");
  for (int i = 0; i < length; i++) {
    Serial.print(pData[i]);
  }
  Serial.println();

  hrm.HRM = pData[1];
  Serial.print("Heart Rate ");
  Serial.print(hrm.HRM, DEC);
  Serial.println("bpm");
  adjust_power();
}

//--------------------------------------------------------------------------------------------
//  Connect to BLE HRM
//--------------------------------------------------------------------------------------------
bool connectToServer(BLEAddress pAddress) {

  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }

  Serial.print(F("Forming a connection to "));
  Serial.println(pAddress.toString().c_str());

  BLEClient*  pClient  = BLEDevice::createClient();
  Serial.println(F(" - Created client"));

  // Connect to the HRM BLE Server.
  // Note that a lot of example snippets I found on the web ommitted the second argument (optionnal) in the following line
  // bug the BT connection was very slow and hang up most of the time
  // once I added "BLE_ADDR_TYPE_RANDOM" the connection was very quick and smooth
  pClient->connect(pAddress, BLE_ADDR_TYPE_RANDOM);
  Serial.println(F(" - Connected to server"));

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print(F("Failed to find our service UUID: "));
    Serial.println(serviceUUID.toString().c_str());
    return false;
  }
  Serial.println(F(" - Found our service"));


  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print(F("Failed to find our characteristic UUID: "));
    Serial.println(charUUID.toString().c_str());
    return false;
  }
  Serial.println(F(" - Found our characteristic"));

  // Read the value of the characteristic.
  std::string value = pRemoteCharacteristic->readValue();
  Serial.print("The characteristic value was: ");
  Serial.println(value.c_str());

  // Register for Notify
  pRemoteCharacteristic->registerForNotify(notifyCallback);
  return true;
}

/**
   Scan for BLE servers and find the first one that advertises the service we are looking for.
*/
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());

      // We have found a device, let us now see if it contains the service we are looking for.
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {
        Serial.println("Found our device!");
        advertisedDevice.getScan()->stop();

        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
        doConnect = true;
      }
    }
};

void adjust_power() {  // function to be fired at the zero crossing to dim the light

  enable = true;
  old_power = power;

  //HR zones upper limits
  int zone0 = maxHR * 0.5;
  int zone1 = maxHR * 0.6;
  int zone2 = maxHR * 0.7;
  int zone3 = maxHR * 0.8;
  int zone4 = maxHR * 0.9;

  if (hrm.HRM < zone0) {
    enable = false;
  }
  if (hrm.HRM >= zone0 and hrm.HRM < zone1 ) {
    power = 20;
  }
  if (hrm.HRM >= zone1 and hrm.HRM < zone2 ) {
    power = 40;
  }
  if (hrm.HRM >= zone2 and hrm.HRM < zone3 ) {
    power = 60;
  }
  if (hrm.HRM >= zone3 and hrm.HRM < zone4 ) {
    power = 80;
  }
  if (hrm.HRM >= zone4) {
    power = 100;
  }

  if (old_power > power) {
    Serial.print("LESS power ! decrease to : ");
    Serial.println(power);
  }
  if (old_power < power) {
    Serial.print("MORE power ! increase to : ");
    Serial.println(power);
  }

  if (enable) {
    dimmer.setPower(power);
  }
}

void setup() {
  startSerial(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  dimmer.begin(NORMAL_MODE, ON);
  dimmer.setPower(0);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device. Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect. Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      connected = true;

      // We are connected, turn on green LED
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");

      // change led flashing pattern according to new BT status
      digitalWrite(LED_BUILTIN, LOW);
    }
    doConnect = false;
  }

  // Turn notification on
  // to subscribe to heart rate updates
  if (connected) {
    if (notification == false) {
      Serial.println(F("Turning Notification On"));
      const uint8_t onPacket[] = {0x1, 0x0};
      pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)onPacket, 2, true);
      notification = true;
    }
  }
}
