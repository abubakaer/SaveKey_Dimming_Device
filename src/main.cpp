#include <Arduino.h>

// Define RS-485 communication pins for ESP32
#define RX_PIN 16
#define TX_PIN 17
#define RS485_Enable_Pin 4

// Function Prototypes
bool sendHexData(String hexString);
bool isHexadecimal(String str);
void executeCommand(String command);
void checkDeviceStatus();
void clearSerialBuffer();
void processResponse(uint8_t *responseArray, int length);
float mapHexToPercentage(uint8_t hexValue);
float calculateVoltage(float percentage);

// Hexadecimal commands for specific actions
const String TURN_ON_LIGHT = "0";
const String TURN_OFF_LIGHT = "1";
const String TURN_ON_FAN = "2";
const String TURN_OFF_FAN = "3";
const String CHECK_STATUS = "4";

unsigned long lastStatusCheckTime = 0;  // Variable to track time for periodic status check
const unsigned long statusCheckInterval = 15000;  // Interval for checking status (15 seconds)

void setup() {
    Serial.begin(115200);                // USB Serial for debugging and user input
    Serial2.begin(4800, SERIAL_8N1, RX_PIN, TX_PIN); // Initialize Serial2 with RS-485 pins
    pinMode(RS485_Enable_Pin, OUTPUT);
    Serial.println("RS-485 HEX Communication Initialized.");
    Serial.println("Enter a command: ");
    Serial.println("0- turn on light");
    Serial.println("1- turn off light");
    Serial.println("2- turn on fan");
    Serial.println("3- turn off fan");
    Serial.println("4- check status");

    // Initial status check when the program starts
    checkDeviceStatus();
}

void loop() {
    static String userInput = ""; // Store input as it is received

    // Check if data is available from Serial Monitor
    while (Serial.available()) {
        char receivedChar = Serial.read(); // Read one character
        Serial.print(receivedChar);       // Echo back for debugging

        if (receivedChar == '\n') { // End of input detected
            userInput.trim(); // Remove any whitespace
            executeCommand(userInput); // Execute the user's command
            userInput = ""; // Reset for the next input
        } else {
            userInput += receivedChar; // Append character to input
        }
    }

    // Periodically check the device status every 15 seconds
    unsigned long currentMillis = millis();
    if (currentMillis - lastStatusCheckTime >= statusCheckInterval) {
        checkDeviceStatus();  // Call the function to check device status
        lastStatusCheckTime = currentMillis;  // Update the time of last status check
    }
}

// Function to execute commands
void executeCommand(String command) {
    if (command == TURN_ON_LIGHT) {
        sendHexData("12 82 01 22 B7");  // Turn On Light
    } else if (command == TURN_OFF_LIGHT) {
        sendHexData("12 81 01 22 B6");  // Turn Off Light
    } else if (command == TURN_ON_FAN) {
        sendHexData("12 82 01 25 BA");  // Turn On Fan
    } else if (command == TURN_OFF_FAN) {
        sendHexData("12 81 01 25 B9");  // Turn Off Fan
    } else if (command == CHECK_STATUS) {
        checkDeviceStatus();  // Check status when requested
    } else {
        Serial.println("Invalid command. Please try again.");
    }
}

// Function to send hex data over RS-485
bool sendHexData(String hexString) {
    clearSerialBuffer(); // Clear buffer before sending data

    // Switch RS-485 to transmit mode
    Serial.println("Switching RS-485 to TRANSMIT mode...");
    digitalWrite(RS485_Enable_Pin, HIGH);
    delay(10); // Stabilize the transceiver in transmit mode

    // Process the hex string
    Serial.print("Processing hex string: ");
    Serial.println(hexString);
    hexString.replace(" ", "");
    int length = hexString.length();
    if (length % 2 != 0 || !isHexadecimal(hexString)) {
        Serial.println("Invalid hex data.");
        return false;
    }

    // Convert hex string to byte buffer
    uint8_t buffer[length / 2];
    for (int i = 0; i < length; i += 2) {
        String byteString = hexString.substring(i, i + 2);
        buffer[i / 2] = (uint8_t)strtol(byteString.c_str(), NULL, 16);
    }

    // Send data over RS-485
    Serial2.write(buffer, length / 2);

    // Debug: Print sent data
    Serial.print("Sent: ");
    for (int i = 0; i < length / 2; i++) {
        Serial.printf("0x%02X ", buffer[i]);
    }
    Serial.println();

    // Switch RS-485 to receive mode
    delay(10); // Allow the device to process the command
    Serial.println("Switching RS-485 to RECEIVE mode...");
    digitalWrite(RS485_Enable_Pin, LOW);

    // Wait for and process response
    Serial.println("Waiting for response...");
    unsigned long startTime = millis();
    uint8_t responseArray[10]; // Fixed array to store the response
    int index = 0;

    while (millis() - startTime < 1000) { // Wait up to 1 second
        if (Serial2.available()) {
            while (Serial2.available() && index < 10) {
                responseArray[index++] = Serial2.read(); // Store response in the array
            }
        }
    }

    if (index > 0) {
        Serial.println("Response detected!");
        processResponse(responseArray, index); // Process the response
        return true;
    }

    Serial.println("No response received.");
    return true;
}

// Function to process the response
void processResponse(uint8_t *responseArray, int length) {
    // Ensure the response matches the expected header (device code and function code)
    if (length >= 3 && responseArray[0] == 0x12 && responseArray[1] == 0xC3 && responseArray[2] == 0x01) {
        Serial.print("Response Array: [ ");
        for (int i = 0; i < length; i++) {
            Serial.printf("%02X ", responseArray[i]);
        }
        Serial.println("]");

        // Check the state of Channel 1 and Channel 2
        uint8_t channel1State = responseArray[3];
        uint8_t channel2State = responseArray[4];

        float channel1Percentage = mapHexToPercentage(channel1State);
        float channel2Percentage = mapHexToPercentage(channel2State);

        Serial.printf("Channel 1: %.0f%% dimming, Voltage: %.1fV\n", channel1Percentage, calculateVoltage(channel1Percentage));
        Serial.printf("Channel 2: %.0f%% dimming, Voltage: %.1fV\n", channel2Percentage, calculateVoltage(channel2Percentage));
    } else {
        Serial.println("Response does not match the expected status response. Ignored.");
    }
}

// Function to map hex values to dimming percentage
float mapHexToPercentage(uint8_t hexValue) {
    if (hexValue == 0x00) {
        return 0.0; // OFF
    }
    return (hexValue / 100.0) * 100.0; // Map to percentage (e.g., 0x32 → 50%)
}

// Function to calculate voltage based on dimming percentage
float calculateVoltage(float percentage) {
    return 70 + (percentage * 140 / 100); // Map percentage to voltage
}

// Function to check if a string is valid hexadecimal
bool isHexadecimal(String str) {
    for (char c : str) {
        if (!isxdigit(c)) return false;
    }
    return true;
}

// Function to check device status
void checkDeviceStatus() {
    sendHexData("12 43 01 56 AC"); // Send the status check command
}

// Function to clear the Serial2 buffer
void clearSerialBuffer() {
    while (Serial2.available()) {
        Serial2.read(); // Discard all bytes in the buffer
    }
}
