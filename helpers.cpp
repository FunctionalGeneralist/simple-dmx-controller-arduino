#include <Arduino.h>
#include <string>
#include "GroupOperations.h"
#include "helpers.h"

#define GROUP_POSITION 1
#define NICKNAME_POSITION 2
#define INTENSITY_POSITION 3
#define CHANNEL_START_POSITION 4
#define MAX_GROUPS 10
#define MAX_CHANNELS 32
#define MAX_CIRCUIT 512


bool helpers::noDelayTimer(int waitInterval, int startTime) {
  if (millis() - startTime > waitInterval) {
    return true;
  }
  else {
    return false;
  }
}

String helpers::extractCsv(std::string str, int targetIndex) {
  // Iterate through target string until arriving at correct position.
  int index = 0;
  int i = 0;
  while (index < targetIndex && str[i] != '\0') {
    if (str[i] == ',') {
      index++;
    };
    i++;
  };

  char buffer[32] = {};
  int j = 0;
  while (isalnum(str[i])) {
    buffer[j] = str[i];
    i++;
    j++;
  };
  String returnString = String(buffer);
  return returnString;
}

void helpers::csvChansToArr(std::string str, int groupId, GroupConfig *groupConfig) {
  int bigIndex = 0;
  int smallIndex = 0;

  // Start at the correct spot in CSV.
  while (bigIndex < CHANNEL_START_POSITION && str[smallIndex] != '\0') {
    if (str[smallIndex] == ',') {
      bigIndex++;
    };
    smallIndex++;
  };

  // Convert each following value into the appropriate group inside of groupConfig.
  for (int i = 0; i < MAX_CHANNELS; i++) {
    int bufferIndex = 0;
    char charBuffer[4] = {};
    // Advance forward if not an alphanumeric value.
    if (!isalnum(str[smallIndex])) {
      smallIndex++;
      Serial.println("Non alphanumeric character.");
    }
    while (isalnum(str[smallIndex])) {
      charBuffer[bufferIndex] = str[smallIndex];
      smallIndex++;
      bufferIndex++;
    };

    groupConfig->group[groupId].channel[i] = atoi(charBuffer);
  };
}
