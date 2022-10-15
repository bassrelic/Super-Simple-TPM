//TPM set_key 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F
// Todo : add command to get status of TPM (Basically giving status OK back, does have general structure f set_key command to tip people of)

#include <StaticSerialCommands.h>

void cmd_help(SerialCommands& sender, Args& args);
void cmd_calc(SerialCommands& sender, Args& args);
void cmd_calc_add(SerialCommands& sender, Args& args);
void cmd_calc_mul(SerialCommands& sender, Args& args);
void cmd_tpm(SerialCommands& sender, Args& args);
void cmd_tpm_set(SerialCommands& sender, Args& args);

/*
COMMAND macro is used to create Command object.
It takes the following arguments:
    COMMAND(function, command)
    COMMAND(function, command, subcommands)
    COMMAND(function, command, subcommands, description)
    COMMAND(function, command, arguments..., subcommands, description)
*/

Command subCommands[] {
        COMMAND(cmd_calc_add, "+", ArgType::Int, nullptr, "add numbers"),
        COMMAND(cmd_calc_mul, "*", ArgType::Int, nullptr, "multiply numbers"),
};

Command tpmSubCommands[]{
        COMMAND(cmd_tpm_set, "set_key", ArgType::String, nullptr, ""),
};

Command commands[] {
        COMMAND(cmd_help, "help", nullptr, "list commands"),
        COMMAND(cmd_calc, "calc", ArgType::Int, subCommands, "calculator"),
        COMMAND(cmd_tpm, "TPM", tpmSubCommands, "TPM"),
};

//SerialCommands serialCommands(Serial, commands, sizeof(commands) / sizeof(Command));

// if default buffer size (64) is too small pass a buffer through constructor
 char buffer[512];
 SerialCommands serialCommands(Serial, commands, sizeof(commands) / sizeof(Command), buffer, sizeof(buffer));

void setup() {
    Serial.begin(9600);

    serialCommands.listAllCommands();
}

void loop() {
    serialCommands.readSerial();
}

void cmd_help(SerialCommands& sender, Args& args) {
    sender.listAllCommands();
}

void cmd_tpm(SerialCommands& sender, Args& args){
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

  Serial.println("AAAHHHHH!!!");
  return 0;  // Not a valid hexadecimal character
}

void cmd_tpm_set(SerialCommands& sender, Args& args){
  String key;
  String keypart;
  char cKeypart;
  byte aesKey[32]={0};
  byte bytes[5];
  int startByte = 0;
  int endByte = 0;
    key = args[0].getString();
    //Serial.println(key);
    Serial.println(key);
    Serial.println(key.length());
    if (key.length() == 159){
      //hierweiter!
      Serial.println("Extracted Bytes:");
      for(int i = 0; i < 32; i++)
      {
        startByte = i * 5 + 2;
        endByte = startByte + 2;
        keypart = key.substring(startByte, endByte-1);
        Serial.print("Keypart 1 is: ");
        Serial.println(keypart);
        cKeypart = keypart[0];
        Serial.print("cKeypart 1 is: ");
        Serial.println(cKeypart);
        aesKey[i] = nibble(cKeypart) << 4;
        Serial.print("AES Key1: ");
        Serial.println(aesKey[i], BIN);
        keypart = key.substring(startByte+1, endByte);
        Serial.print("Keypart 2 is: ");
        Serial.println(keypart);
        cKeypart = keypart[0];

        aesKey[i] = aesKey[i] + nibble(cKeypart);
        //keypart.getBytes(aesKey[i], 1);
        //Serial.print(keypart);
        //TPM set_key 0xAB,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F
        Serial.print("AES Key2: ");
        Serial.print(aesKey[i], BIN);
        Serial.println();
        Serial.print("Should look like this: ");
        Serial.print(keypart);
        Serial.println();
      }
      keypart = key.substring(2,4);
      Serial.println("Extracted Part:");
      Serial.println(keypart);
      key.getBytes(bytes, 5);
      Serial.println(bytes[0]);
      Serial.println(bytes[1]);
    }
    else{
      Serial.println("Invalid Keylength");
    }
}

void cmd_calc(SerialCommands& sender, Args& args) {
    sender.listAllCommands(subCommands, sizeof(subCommands) / sizeof(Command));
}

void cmd_calc_add(SerialCommands& sender, Args& args) {
    auto number1 = args[0].getInt();
    auto number2 = args[1].getInt();
    sender.getSerial().println(number1 + number2);
}

void cmd_calc_mul(SerialCommands& sender, Args& args) {
    auto number1 = args[0].getInt();
    auto number2 = args[1].getInt();
    sender.getSerial().println(number1 * number2);
}