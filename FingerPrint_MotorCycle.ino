#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

#define LEDpin_RED 6
#define LEDpin_GREEN 5
#define PBpin 4
#define RELAYpin1 A0

// Define the software serial pins
SoftwareSerial mySerial(2, 3);  // RX, TX

Adafruit_Fingerprint finger(&mySerial);

String CurrentState = "IDLE";

bool IgnitionStatus = false;  //true if UNLOCKED, false if LOCKED

void setup() {
  pinMode(LEDpin_RED, OUTPUT);
  pinMode(LEDpin_GREEN, OUTPUT);
  pinMode(RELAYpin1, OUTPUT);
  pinMode(PBpin, INPUT_PULLUP);

  Relay1_Close();

  for (uint8_t i = 0; i < 10; i++) {
    digitalWrite(LEDpin_RED, HIGH);
    digitalWrite(LEDpin_GREEN, LOW);
    delay(100);
    digitalWrite(LEDpin_RED, LOW);
    digitalWrite(LEDpin_GREEN, HIGH);
    delay(100);
  }
  digitalWrite(LEDpin_RED, LOW);
  digitalWrite(LEDpin_GREEN, LOW);

  Serial.begin(9600);     // Debugging via serial monitor
  mySerial.begin(57600);  // Sensor baud rate

  Serial.println("Initializing fingerprint sensor...");
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor initialized successfully!");
  } else {
    Serial.println("Failed to initialize fingerprint sensor. Check connections.");
    while (1)
      ;  // Stop execution if the sensor isn't initialized
  }


}

void loop() {
  uint16_t counter = 0;

  if (status_PushButton()) {
    Serial.println("Push Button Pressed");
    while (status_PushButton()) {
      delay(10);
      counter++;
      if (counter > 300) {
        Serial.println("Finger Enrollment Detected!");

        while (status_PushButton()) {
          digitalWrite(LEDpin_RED, LOW);
          delay(100);
          digitalWrite(LEDpin_RED, HIGH);
          delay(100);
        }
      }
    }

    if (counter < 50) {
      CurrentState = "SCAN_FP";
    } else {
      CurrentState = "ENROLL_FP";
    }
  }

  if (!IgnitionStatus) {
    if (CurrentState == "SCAN_FP") {
      if (scanFingerprint()) {
        digitalWrite(LEDpin_GREEN, HIGH);
        digitalWrite(LEDpin_RED, LOW);
        IgnitionStatus = true;
      } else {
        digitalWrite(LEDpin_RED, HIGH);
        digitalWrite(LEDpin_GREEN, LOW);
        for (uint8_t i = 0; i < 10; i++) {
          digitalWrite(LEDpin_RED, LOW);
          delay(100);
          digitalWrite(LEDpin_RED, HIGH);
          delay(100);
        }
        IgnitionStatus = false;
      }
    } else if (CurrentState == "ENROLL_FP") {
      Serial.println("Enter an ID (1-127) for the new fingerprint:");

      int id = 1;
      if (id > 0 && id <= 127) {
        Serial.print("Enrolling ID #");
        Serial.println(id);
        enrollFingerprint(id);
      } else {
        Serial.println("Invalid ID. Please try again.");
      }
      CurrentState = "IDLE";
    }
  }else{
    Relay1_Open();
  }
}

// Function to scan and identify a fingerprint
bool scanFingerprint() {
  for (uint8_t i = 0; i < 10; i++) {
    digitalWrite(LEDpin_GREEN, HIGH);
    delay(100);
    digitalWrite(LEDpin_GREEN, LOW);
    delay(100);
  }
  Serial.println("Place your finger on the sensor...");
  int result = finger.getImage();

  if (result == FINGERPRINT_OK) {
    Serial.println("Fingerprint detected. Processing...");
    result = finger.image2Tz();
    if (result == FINGERPRINT_OK) {
      result = finger.fingerFastSearch();
      if (result == FINGERPRINT_OK) {
        Serial.print("Fingerprint recognized with ID: ");
        Serial.println(finger.fingerID);
        return true;
      } else {
        Serial.println("Fingerprint not recognized.");
        return false;
      }
    } else {
      Serial.println("Failed to convert image.");
      return false;
    }
  } else if (result == FINGERPRINT_NOFINGER) {
    Serial.println("No finger detected.");
    return false;
  } else {
    Serial.println("Error reading fingerprint.");
    return false;
  }
}

// Function to enroll a fingerprint
void enrollFingerprint(int id) {
  int p;

  Serial.println("Place your finger on the sensor...");
  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOFINGER) {
      Serial.print(".");
    } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
      Serial.println("Error receiving data.");
    } else if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println("Image capture failed.");
    } else {
      Serial.println("Unknown error.");
    }
    delay(100);
  }

  // Convert the image to a template
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to convert image to template.");
    return;
  }
  Serial.println("Image converted.");

  Serial.println("Remove your finger and place it again...");
  for (uint8_t i = 0; i < 20; i++) {
    digitalWrite(LEDpin_RED, LOW);
    delay(100);
    digitalWrite(LEDpin_RED, HIGH);
    delay(100);
  }

  // Wait for the second image
  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOFINGER) {
      Serial.print(".");
    } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
      Serial.println("Error receiving data.");
    } else if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println("Image capture failed.");
    } else {
      Serial.println("Unknown error.");
    }
    delay(100);
  }

  // Convert the second image to a template
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to convert second image to template.");
    return;
  }
  Serial.println("Second image converted.");

  // Create a model from the two templates
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    if (p == FINGERPRINT_PACKETRECIEVEERR) {
      Serial.println("Error receiving data.");
    } else if (p == FINGERPRINT_ENROLLMISMATCH) {
      Serial.println("Fingerprints do not match.");
    } else {
      Serial.println("Unknown error.");
    }
    return;
  }
  Serial.println("Fingerprint matched and model created.");

  // Store the model in the specified ID
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint successfully enrolled!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Error receiving data.");
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints do not match.");
  } else {
    Serial.println("Unknown error.");
  }
}

bool status_PushButton() {
  if (digitalRead(PBpin) == LOW) {
    return true;
  } else {
    return false;
  }
}

void Relay1_Open() {
  digitalWrite(RELAYpin1, LOW);
}

void Relay1_Close() {
  digitalWrite(RELAYpin1, HIGH);
}