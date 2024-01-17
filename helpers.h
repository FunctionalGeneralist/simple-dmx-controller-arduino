#include "GroupOperations.h"

#ifndef helpers_h
#define helpers_h

class helpers {
public:
  bool noDelayTimer(int waitInterval, int startTime);
  String extractCsv(std::string str, int targetIndex);
  void csvChansToArr(std::string str, int groupId, GroupConfig *groupConfig);
};

#endif
