/*
 * Copyright (C) 2015 Southern Storm Software, Pty Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <Crypto.h>
#include <AES.h>
#include <StaticSerialCommands.h>
#include <SPI.h>
#include <FastLED.h>
#include "pins_arduino.h"

#define DATARATE 9600
#define DATA_PIN 3
#define COLOR_ORDER GRB
#define LED_TYPE WS2812
#define NUM_LEDS 14

#define BRIGHTNESS 150
#define FRAMES_PER_SECOND 60

CRGB leds[NUM_LEDS];

volatile boolean received;
volatile int recCount = 0;
volatile byte receivedData[16];

void cmd_help(SerialCommands &sender, Args &args);
void cmd_tpm(SerialCommands &sender, Args &args);
void cmd_tpm_set(SerialCommands &sender, Args &args);
void cmd_tpm_status(SerialCommands &sender, Args &args);
void cmd_tpm_check(SerialCommands &sender, Args &args);

/* https://www.arduino.cc/reference/en/libraries/crypto/ */
/* statemachine */
/* aes256 encryption for challenge response */
/* UART interface for setting of key */

// TPM set_key 0xAB,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F
// TPM set_key 0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09
/*                                                        */
/*    ---------------     ---------------                 */
/*    |             |     |             |                 */
/*    |             |-----|             |                 */
/*    |     BBB     | SPI |     TPM     |---------------- */
/*    |   AES Key   |     |   AES Key   |    Serial       */
/*    |    Linux    |     |  Changable  | Debug interface */
/*    |             |     |             |      USB        */
/*    ---------------     ---------------                 */
/*                                                        */
/*
COMMAND macro is used to create Command object.
It takes the following arguments:
    COMMAND(function, command)
    COMMAND(function, command, subcommands)
    COMMAND(function, command, subcommands, description)
    COMMAND(function, command, arguments..., subcommands, description)
*/

Command tpmSubCommands[]{
    COMMAND(cmd_tpm_set, "set_key", ArgType::String, nullptr, "Set new AES Key in the form of 0xXX,0xXX,...,0xXX with 32 Bytes"),
    COMMAND(cmd_tpm_status, "get_status", NULL, "Get Status of TPM module"),
    COMMAND(cmd_tpm_check, "check_key", NULL, "Checks if new key works as expected"),
};

Command commands[]{
    COMMAND(cmd_help, "help", nullptr, "list commands"),
    COMMAND(cmd_tpm, "TPM", tpmSubCommands, "TPM commands"),
};

// SerialCommands serialCommands(Serial, commands, sizeof(commands) / sizeof(Command));

// if default buffer size (64) is too small pass a buffer through constructor
char staticSerialBuffer[256];
SerialCommands serialCommands(Serial, commands, sizeof(commands) / sizeof(Command), staticSerialBuffer, sizeof(staticSerialBuffer));

ISR(SPI_STC_vect) // Inerrrput routine function
{
    if (recCount < 8)
    {
        receivedData[recCount] = SPDR; // Get the received data from SPDR register
        received = true;               // Sets received as True
        recCount++;
    }
}

AES256 aes256;
byte cypherBuffer[16];

struct AES256Vector
{
    const char *name;
    byte key[32];
    byte plaintext[16];
    byte ciphertext[16];
};

static AES256Vector vectorAES256 = {
    .name = "AES-256-ECB",
    .key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F},
    .plaintext = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                  0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    .ciphertext = {0x8E, 0xA2, 0xB7, 0xCA, 0x51, 0x67, 0x45, 0xBF,
                   0xEA, 0xFC, 0x49, 0x90, 0x4B, 0x49, 0x60, 0x89}};

static AES256Vector vectorAES256Crossref = {
    .name = "AES-256-ECB-crossref",
    .key = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x01,
            0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
            0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x01,
            0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09},
    .plaintext = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff},
    .ciphertext = {0xfe, 0x46, 0x47, 0x0f, 0x75, 0x46, 0x21, 0x74,
                   0x86, 0xc3, 0x03, 0xfb, 0x29, 0xc2, 0xd0, 0x8c}};

void testCipher(BlockCipher *cipher, const struct AES256Vector *test)
{
    crypto_feed_watchdog();
    Serial.print(test->name);
    Serial.print(" Encryption ... ");
    cipher->setKey(test->key, cipher->keySize());
    cipher->encryptBlock(cypherBuffer, test->plaintext);
    if (memcmp(cypherBuffer, test->ciphertext, 16) == 0)
        Serial.println("Passed");
    else
        Serial.println("Failed");

    Serial.print(test->name);
    Serial.print(" Decryption ... ");
    cipher->decryptBlock(cypherBuffer, test->ciphertext);
    if (memcmp(cypherBuffer, test->plaintext, 16) == 0)
        Serial.println("Passed");
    else
        Serial.println("Failed");
}

void encryptAES256(BlockCipher *cipher, const struct AES256Vector *vector)
{
    crypto_feed_watchdog();
    Serial.print(vector->name);
    Serial.print(" Encryption ... ");
    cipher->setKey(vector->key, cipher->keySize());
    cipher->encryptBlock(cypherBuffer, vector->plaintext);
    if (memcmp(cypherBuffer, vector->ciphertext, 16) == 0)
        Serial.println("Passed");
    else
        Serial.println("Failed");
}

void decryptAES256(BlockCipher *cipher, const struct AES256Vector *vector)
{
    crypto_feed_watchdog();
    Serial.print(vector->name);
    Serial.print(" Decryption ... ");
    cipher->decryptBlock(cypherBuffer, vector->ciphertext);
    if (memcmp(cypherBuffer, vector->plaintext, 16) == 0)
        Serial.println("Passed");
    else
        Serial.println("Failed");
}

void setup()
{
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip); // initializes LED strip
    FastLED.setBrightness(BRIGHTNESS);                                                               // global brightness
    showProgramGray();

    pinMode(SS, INPUT_PULLUP);
    pinMode(MOSI, INPUT);
    pinMode(SCK, INPUT);
    pinMode(MISO, OUTPUT); // Sets MISO as OUTPUT

    SPCR |= _BV(SPE); // Turn on SPI in Slave Mode
    SPCR |= _BV(SPIE);

    Serial.begin(DATARATE);
    serialCommands.listCommands();

    Serial.println();

    Serial.println("State Sizes:");
    Serial.print("AES256 ... ");
    Serial.println(sizeof(AES256));
    Serial.println();

    Serial.println("Test Vectors:");
    testCipher(&aes256, &vectorAES256);

    Serial.println();
}

void loop()
{
    serialCommands.readSerial();

    if (recCount >= 8)
    {
        SPCR &= ~_BV(SPE); // Turn on SPI in Slave Mode
        recCount = 0;
        // SPDR = receivedData;    // send back the received data, this is not necessary, only for demo purpose
        received = false;
        // Serial.println((char *)receivedData);
        // Serial.write('\n');
        if (strcmp(receivedData, "ALAAAARM") == 0)
        {
            while (true)
            {
                showPolice(250);
            }
        }
        if (strcmp(receivedData, "ALLES_OK") == 0)
        {
            for (int i = 0; i < NUM_LEDS; i++)
            {
                showProgramRandom();
                delay(100);
            }
            showProgramWhite(); // show "random" program
        }
        SPCR |= _BV(SPE); // Turn on SPI in Slave Mode
        recCount = 0;
    }
}

void cmd_help(SerialCommands &sender, Args &args)
{
    sender.listAllCommands();
}

void cmd_tpm(SerialCommands &sender, Args &args)
{
    sender.listAllCommands(tpmSubCommands, sizeof(tpmSubCommands) / sizeof(Command));
}

byte nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 255; // Not a valid hexadecimal character
}

void cmd_tpm_set(SerialCommands &sender, Args &args)
{
    String key;
    String keypart;
    char cKeypart = 0;
    byte aesKey[32] = {0};
    byte bytes[5] = {0};
    int startByte = 0;
    int endByte = 0;
    char temp = 0;

    key = args[0].getString();
    if (key.length() == 159)
    {
        for (int i = 0; i < 32; i++)
        {
            startByte = i * 5 + 2;
            endByte = startByte + 2;
            keypart = key.substring(startByte, endByte - 1);
            cKeypart = keypart[0];

            temp = nibble(cKeypart);
            if (temp == 255)
            {
                Serial.println("No valid Hex format!");
                break;
            }
            aesKey[i] = temp << 4;
            keypart = key.substring(startByte + 1, endByte);
            cKeypart = keypart[0];

            temp = nibble(cKeypart);
            if (temp == 255)
            {
                Serial.println("No valid Hex format!");
                break;
            }
            aesKey[i] = aesKey[i] + temp;

            vectorAES256.key[i] = aesKey[i];
        }
        keypart = key.substring(2, 4);
    }
    else
    {
        Serial.println("Invalid Keylength");
    }
}

void cmd_tpm_status(SerialCommands &sender, Args &args)
{
    Serial.println("TPM Status Okay!");
}

void cmd_tpm_check(SerialCommands &sender, Args &args)
{
    Serial.println("Check against original Key:");
    testCipher(&aes256, &vectorAES256);
    for (int i = 0; i < 32; i++)
    {
        vectorAES256Crossref.key[i] = vectorAES256.key[i];
    }
    Serial.println("Check against testing reference Key:");
    testCipher(&aes256, &vectorAES256Crossref);
}

void showProgramRandom()
{
    for (int i = 0; i < NUM_LEDS; ++i)
    {
        leds[i] = CHSV(random8(), 255, 255); // hue, saturation, value
        FastLED.show();
    }
}

void showProgramCleanUp(long delayTime)
{
    for (int i = 0; i < NUM_LEDS; ++i)
    {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
    delay(delayTime);
}

void showProgramGray()
{
    for (int i = 0; i < NUM_LEDS; ++i)
    {
        leds[i] = CRGB::DarkSlateGray;
    }
    FastLED.show();
}

void showProgramWhite()
{
    for (int i = 0; i < NUM_LEDS; ++i)
    {
        leds[i] = CRGB::White;
    }
    FastLED.show();
}

void showPolice(long delayTime)
{
    for (int i = 0; i < NUM_LEDS / 2; ++i)
    {
        leds[i] = CRGB::Red;
    }
    for (int i = NUM_LEDS; i > NUM_LEDS / 2; --i)
    {
        leds[i] = CRGB::Blue;
    }
    FastLED.show();
    delay(delayTime);
    for (int i = 0; i < NUM_LEDS / 2; ++i)
    {
        leds[i] = CRGB::Blue;
    }
    for (int i = NUM_LEDS; i > NUM_LEDS / 2; --i)
    {
        leds[i] = CRGB::Red;
    }
    FastLED.show();
    delay(delayTime);
}