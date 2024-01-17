#include <FS.h>
#include <FFat.h>

#ifndef GroupFileOperations_h
#define GroupFileOperations_h

#define MAX_GROUPS 10
#define MAX_CHANNELS 32
#define MAX_CIRCUIT 512

// Structs to organize groups.
typedef struct {
  int id;
  String nickname;
  int intensity;
  int channel[MAX_CHANNELS];
} Group;

typedef struct {
  Group group[MAX_GROUPS];
} GroupConfig;

// ---- Methods ----

class GroupOperations {
public:
  String stringifyGroup(Group group);
  Group groupCreator(int groupId);
  String extractCsv(String str, int targetIndex);
  GroupConfig *readConfig(fs::FS &fs, const char * filename);
  void writeConfig(fs::FS &fs, const char * filename);
  void deleteConfig(fs::FS &fs, const char * filename);
  void printConfig(fs::FS &fs, const char * filename);
private:
  void initialGroupSetup(const char * filename);
};

#endif
