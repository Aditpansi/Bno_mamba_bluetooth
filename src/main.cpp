#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <EEPROM.h>
#include <SPI.h>
#include <BluetoothSerial.h>

float declinationAngle = -1.20; // Magnetic declination angle

#define BNO055_SAMPLERATE_DELAY_MS (100)

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28); // Initialize the sensor with ID 55 and I2C address 0x28

BluetoothSerial SerialBT;
bool sendData = false;

/**************************/
/*
    Displays some basic information on this sensor from the unified
    sensor API sensor_t type (see Adafruit_Sensor for more information)
    */
/**************************/
void displaySensorDetails(void)
{
    sensor_t sensor;
    bno.getSensor(&sensor);
    Serial.println("------------------------------------");
    Serial.print("Sensor:       "); Serial.println(sensor.name);
    Serial.print("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" xxx");
    Serial.print("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" xxx");
    Serial.print("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" xxx");
    Serial.println("------------------------------------");
    Serial.println("");
    delay(500);
}

/**************************/
/*
    Display some basic info about the sensor status
    */
/**************************/
void displaySensorStatus(void)
{
    /* Get the system status values (mostly for debugging purposes) */
    uint8_t system_status, self_test_results, system_error;
    system_status = self_test_results = system_error = 0;
    bno.getSystemStatus(&system_status, &self_test_results, &system_error);

    /* Display the results in the Serial Monitor */
    Serial.println("");
    Serial.print("System Status: 0x");
    Serial.println(system_status, HEX);
    Serial.print("Self Test:     0x");
    Serial.println(self_test_results, HEX);
    Serial.print("System Error:  0x");
    Serial.println(system_error, HEX);
    Serial.println("");
    delay(500);
}

/**************************/
/*
    Display sensor calibration status
    */
/**************************/
void displayCalStatus(void)
{
    /* Get the four calibration values (0..3) */
    /* Any sensor data reporting 0 should be ignored, */
    /* 3 means 'fully calibrated" */
    uint8_t system, gyro, accel, mag;
    system = gyro = accel = mag = 0;
    bno.getCalibration(&system, &gyro, &accel, &mag);

    /* The data should be ignored until the system calibration is > 0 */
    Serial.print("\t");
    SerialBT.print("\t");
    if (!system)
    {
        Serial.print("! ");
        SerialBT.print("! ");
    }

    /* Display the individual values */
    Serial.print("Sys:");
    Serial.print(system, DEC);
    Serial.print(" G:");
    Serial.print(gyro, DEC);
    Serial.print(" A:");
    Serial.print(accel, DEC);
    Serial.print(" M:");
    Serial.print(mag, DEC);

    SerialBT.print("Sys:");
    SerialBT.print(system, DEC);
    SerialBT.print(" G:");
    SerialBT.print(gyro, DEC);
    SerialBT.print(" A:");
    SerialBT.print(accel, DEC);
    SerialBT.print(" M:");
    SerialBT.print(mag, DEC);

    Serial.print(">Sys:");
    Serial.println(system, DEC);

    Serial.print("> G:");
    Serial.println(gyro, DEC);

    Serial.print(">A:");
    Serial.println(accel, DEC);

    Serial.print(">M:");
    Serial.println(mag, DEC);

    SerialBT.print(">Sys:");
    SerialBT.println(system, DEC);

    SerialBT.print("> G:");
    SerialBT.println(gyro, DEC);

    SerialBT.print(">A:");
    SerialBT.println(accel, DEC);

    SerialBT.print(">M:");
    SerialBT.println(mag, DEC);
}

/**************************/
/*
    Display the raw calibration offset and radius data
    */
/**************************/
void displaySensorOffsets(const adafruit_bno055_offsets_t &calibData)
{
    Serial.print("Accelerometer: ");
    Serial.print(calibData.accel_offset_x); Serial.print(" ");
    Serial.print(calibData.accel_offset_y); Serial.print(" ");
    Serial.print(calibData.accel_offset_z); Serial.print(" ");

    Serial.print("\nGyro: ");
    Serial.print(calibData.gyro_offset_x); Serial.print(" ");
    Serial.print(calibData.gyro_offset_y); Serial.print(" ");
    Serial.print(calibData.gyro_offset_z); Serial.print(" ");

    Serial.print("\nMag: ");
    Serial.print(calibData.mag_offset_x); Serial.print(" ");
    Serial.print(calibData.mag_offset_y); Serial.print(" ");
    Serial.print(calibData.mag_offset_z); Serial.print(" ");

    Serial.print("\nAccel Radius: ");
    Serial.print(calibData.accel_radius);

    Serial.print("\nMag Radius: ");
    Serial.print(calibData.mag_radius);
}

/**************************/
/*
    Arduino setup function (automatically called at startup)
    */
/**************************/

#define I2C_SDA 21 // Use the appropriate SDA pin for your ESP32 board
#define I2C_SCL 22 
void setup(void)
{
    Serial.begin(115200);
    SerialBT.begin("BNO055_Sensor"); // Bluetooth device name
    delay(1000);
    Serial.println("Orientation Sensor Test"); Serial.println("");

    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000); // Set I2C clock speed to 100kHz

    delay(1000); // Wait for BNO055 to power up

    // Initialise the sensor
    if (!bno.begin())
    {
        /* There was a problem detecting the BNO055 ... check your connections */
        Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
        while (1);
    }

    EEPROM.begin(512); // Initialize EEPROM with 512 bytes of space

    int eeAddress = 0;
    long bnoID;
    bool foundCalib = false;

    EEPROM.get(eeAddress, bnoID);

    adafruit_bno055_offsets_t calibrationData;
    sensor_t sensor;

    /* Look for the sensor's unique ID at the beginning of EEPROM. */
    bno.getSensor(&sensor);
    if (bnoID != sensor.sensor_id)
    {
        Serial.println("\nNo Calibration Data for this sensor exists in EEPROM");
        delay(500);
    }
    else
    {
        Serial.println("\nFound Calibration for this sensor in EEPROM.");
        eeAddress += sizeof(long);
        EEPROM.get(eeAddress, calibrationData);

        displaySensorOffsets(calibrationData);

        Serial.println("\n\nRestoring Calibration data to the BNO055...");
        bno.setSensorOffsets(calibrationData);

        Serial.println("\n\nCalibration data loaded into BNO055");
        foundCalib = true;
    }

    delay(1000);

    /* Display some basic information on this sensor */
    displaySensorDetails();

    /* Optional: Display current status */
    displaySensorStatus();

    /* Crystal must be configured AFTER loading calibration data into BNO055. */
    bno.setExtCrystalUse(true);

    /* Set to NDOF mode to enable absolute orientation output */
    bno.setMode(static_cast<adafruit_bno055_opmode_t>(0x0C)); // Assuming 0x0C is the correct mode

    sensors_event_t event;
    bno.getEvent(&event);
    /* always recal the mag as It goes out of calibration very often */
    if (foundCalib){
        Serial.println("Move sensor slightly to calibrate magnetometers");
        while (!bno.isFullyCalibrated())
        {
            bno.getEvent(&event);
            delay(BNO055_SAMPLERATE_DELAY_MS);
        }
    }
    else
    {
        Serial.println("Please Calibrate Sensor: ");
        while (!bno.isFullyCalibrated())
        {
            bno.getEvent(&event);

            Serial.print("X: ");
            Serial.print(event.orientation.x, 4);
            Serial.print("\tY: ");
            Serial.print(event.orientation.y, 4);
            Serial.print("\tZ: ");
            Serial.print(event.orientation.z, 4);

            /* Optional: Display calibration status */
            displayCalStatus();

            /* New line for the next sample */
            Serial.println("");

            /* Wait the specified delay before requesting new data */
            delay(BNO055_SAMPLERATE_DELAY_MS);
        }
    }

    Serial.println("\nFully calibrated!");
    Serial.println("--------------------------------");
    Serial.println("Calibration Results: ");
    adafruit_bno055_offsets_t newCalib;
    bno.getSensorOffsets(newCalib);
    displaySensorOffsets(newCalib);

    Serial.println("\n\nStoring calibration data to EEPROM...");

    eeAddress = 0;
    bno.getSensor(&sensor);
    bnoID = sensor.sensor_id;

    EEPROM.put(eeAddress, bnoID);

    eeAddress += sizeof(long);
    EEPROM.put(eeAddress, newCalib);
    EEPROM.commit(); // Save changes to EEPROM
    Serial.println("Data stored to EEPROM.");

    Serial.println("\n--------------------------------\n");
    delay(500);
}

void loop() {
    if (SerialBT.available()) {
        char input = SerialBT.read();
        if (input == '1') {
            sendData = true;
            Serial.println("Starting to send data");
        } else if (input == '0') {
            sendData = false;
            Serial.println("Stopping data transmission");
        }
    }

    if (sendData) {
        /* Get a new sensor event */
        sensors_event_t event;
        bno.getEvent(&event);

        /* Correct the yaw value (heading) for declination */
        float yaw = (event.orientation.x + declinationAngle + 90) * -1;

        /* Ensure yaw stays within 0-360 degrees */
        if (yaw < 0) yaw += 360;
        if (yaw > 360) yaw -= 360;

        /* Display the floating point data with declination adjustment */
        SerialBT.print(">roll:");
        SerialBT.println(event.orientation.z * -1, 4);
        SerialBT.print(">pitch:");
        SerialBT.println(event.orientation.y, 4);
        SerialBT.print(">yaw with declination:");
        SerialBT.println(yaw, 4);

        /* Optional: Display calibration status */
        displayCalStatus();

        /* New line for the next sample */
        SerialBT.println("");

        /* Wait the specified delay before requesting new data */
        delay(BNO055_SAMPLERATE_DELAY_MS);
    }
}