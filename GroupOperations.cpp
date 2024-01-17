#include <ArduinoJson.h>
#include <FS.h>
#include <FFat.h>
#include "GroupOperations.h"
#include <string>
#include <time.h>

// Where each value is located in input csv string.
#define OPERATION_CODE_POSITION 0
#define GROUP_POSITION 1
#define NICKNAME_POSITION 2
#define INTENSITY_POSITION 3

// Max number of group structs. If you'd like to change this, alter the size of Json config at your own peril.
#define MAX_GROUPS 10
#define MAX_CHANNELS 32
#define MAX_CIRCUIT 512

// Used for JsonArduino.
GroupConfig * config = new GroupConfig;


// Convert input Group struct into string and return it.
String GroupOperations::stringifyGroup(Group group) {
  String strBuffer = String(group.id);
  strBuffer = strBuffer + "," + group.nickname;
  strBuffer = strBuffer + "," + String(group.intensity);
  for (int i = 0; i < MAX_CHANNELS; i++) {
    String groupChannelBuffer = String(group.channel[i]);
    strBuffer = strBuffer + ',' + groupChannelBuffer;
  }
  strBuffer = strBuffer + ',';
  return strBuffer;
}

// Create a new group.
Group GroupOperations::groupCreator(int groupId) {
  Group *newGroup = new Group;
  newGroup->id = groupId;
  newGroup->intensity = 0;
  for (int i = 0; i < MAX_CHANNELS; i++) {
    newGroup->channel[i] = 0;
  }
  return *newGroup;
}

// Write configuration file.
void GroupOperations::writeConfig(fs::FS &fs, const char * filename) {
  Serial.printf("Writing file: %s\n", filename);
  File file = fs.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Allocate temporary JSON document.
  DynamicJsonDocument doc(64000);

  // Convert variables in struct GroupConfig to be saved using ArduinoJson
  for (int i = 0; i < MAX_GROUPS; i++) {
    // Buffer for converting ints to char*'s.
    char charBuffer[32] = {};
    
    itoa(config->group[i].id, charBuffer, 10);
    doc["group"][i]["id"] = charBuffer;

    /*
     * I don't think I need this code... But just in case, it's here.
      String buffer = config->group[i].nickname;
      buffer.toCharArray(charBuffer, sizeof(buffer));
    */
    
    doc["group"][i]["nickname"] = config->group[i].nickname;

    itoa(config->group[i].intensity, charBuffer, 10);
    doc["group"][i]["intensity"] = charBuffer;

    for (int j = 0; j < MAX_CHANNELS; j++) {
      itoa(config->group[i].channel[j], charBuffer, 10);
      doc["group"][i]["channel"][j] = charBuffer;
    }
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to file");
  }
  else {
    Serial.println("File successfully written");
  }

  file.close();
}

// Create empty groups with distinct ID's.
void GroupOperations::initialGroupSetup(const char * filename) {
  Serial.println("Going through initialGroupSetup");
  for (int i = 0; i < MAX_GROUPS; i++) {
    config->group[i] = groupCreator(i);
  };
  Serial.println("About to write config");
  writeConfig(FFat, filename);
}

GroupConfig * GroupOperations::readConfig(fs::FS &fs, const char *filename) {
  Serial.printf("Reading file: %s\n", filename);
  Serial.print("Size of config: ");
  Serial.println(sizeof(*config));
  File file = fs.open(filename);
  if (!file) {
    Serial.println("Failed to open file for reading, writing default config.");
    initialGroupSetup(filename);
    file = fs.open(filename);
  }

  DynamicJsonDocument doc(64000);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("Failed to read file using ArduinoJson"));
    Serial.println(error.f_str());
    return config;
  }

  // Copy values from file to config
  char *buffer;
  for (int i = 0; i < MAX_GROUPS; i++) {
    config->group[i].id = doc["group"][i]["id"];
    config->group[i].nickname = String((const char *)doc["group"][i]["nickname"]);
    config->group[i].intensity = doc["group"][i]["intensity"];
    for (int j = 0; j < MAX_CHANNELS; j++) {
      config->group[i].channel[j] = (int)doc["group"][i]["channel"][j];
    }
  }
  file.close();
  return config;
}

void GroupOperations::deleteConfig(fs::FS &fs, const char * filename) {
  Serial.printf("Deleting file: %s\n", filename);
  if (fs.remove(filename)) {
    Serial.println("File deleted");
  } 
  else {
    Serial.println("Delete failed");
  }
}

void GroupOperations::printConfig(fs::FS &fs, const char * filename) {
  File file = fs.open(filename);
  if (!file) {
    Serial.println("Failed to open file for printing");
    return;
  }
  while (file.available()) {
    Serial.write((char)file.read());
  }
}
