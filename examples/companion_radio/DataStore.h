#pragma once

#include <helpers/IdentityStore.h>
#include <helpers/ContactInfo.h>
#include <helpers/ChannelDetails.h>
#include "NodePrefs.h"
#include "CannedMessages.h"

#ifdef HEADLESS_CANNED_MESSAGES
struct TapTargetPrefs {
  static constexpr uint8_t kVersion = 1;
  uint8_t version = kVersion;
  uint8_t type = 0;          // 0 = channel, 1 = contact
  uint8_t channel_idx = 0;
  uint8_t reserved = 0;
  uint8_t contact_pub_key[PUB_KEY_SIZE] = {0};
  char name[32] = {0};
};

struct BuzzerPrefs {
  static constexpr uint8_t kVersion = 1;
  static constexpr uint8_t kMaxStoredChannels = 32;
  uint8_t version = kVersion;
  uint8_t global_override = 0;
  uint8_t channel_set[kMaxStoredChannels] = {0};
  uint8_t channel_mode[kMaxStoredChannels] = {0};
};
#endif

class DataStoreHost {
public:
  virtual bool onContactLoaded(const ContactInfo& contact) =0;
  virtual bool getContactForSave(uint32_t idx, ContactInfo& contact) =0;
  virtual bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) =0;
  virtual bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) =0;
};

class DataStore {
  FILESYSTEM* _fs;
  FILESYSTEM* _fsExtra;
  mesh::RTCClock* _clock;
  IdentityStore identity_store;

  void loadPrefsInt(const char *filename, NodePrefs& prefs, double& node_lat, double& node_lon);
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  void checkAdvBlobFile();
#endif

public:
  DataStore(FILESYSTEM& fs, mesh::RTCClock& clock);
  DataStore(FILESYSTEM& fs, FILESYSTEM& fsExtra, mesh::RTCClock& clock);
  void begin();
  bool formatFileSystem();
  FILESYSTEM* getPrimaryFS() const { return _fs; }
  FILESYSTEM* getSecondaryFS() const { return _fsExtra; }
  bool loadMainIdentity(mesh::LocalIdentity &identity);
  bool saveMainIdentity(const mesh::LocalIdentity &identity);
  void loadPrefs(NodePrefs& prefs, double& node_lat, double& node_lon);
  void savePrefs(const NodePrefs& prefs, double node_lat, double node_lon);
  void loadContacts(DataStoreHost* host);
  void saveContacts(DataStoreHost* host);
  void loadChannels(DataStoreHost* host);
  void saveChannels(DataStoreHost* host);
  bool loadCannedMessages(char dest[][canned::kMaxMessageLen], size_t max_count, size_t& out_count);
  bool saveCannedMessages(const char src[][canned::kMaxMessageLen], size_t count);
  bool loadRingtoneBlob(uint8_t* dest, size_t max_len, size_t& out_len);
  bool saveRingtoneBlob(const uint8_t* src, size_t len);
#ifdef HEADLESS_CANNED_MESSAGES
  bool loadTapTarget(TapTargetPrefs& prefs);
  bool saveTapTarget(const TapTargetPrefs& prefs);
  bool loadBuzzerPrefs(BuzzerPrefs& prefs);
  bool saveBuzzerPrefs(const BuzzerPrefs& prefs);
#endif
  void migrateToSecondaryFS();
  uint8_t getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]);
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len);
  File openRead(const char* filename);
  File openRead(FILESYSTEM* fs, const char* filename);
  bool removeFile(const char* filename);
  bool removeFile(FILESYSTEM* fs, const char* filename);
  uint32_t getStorageUsedKb() const;
  uint32_t getStorageTotalKb() const;

private:
  FILESYSTEM* _getContactsChannelsFS() const { if (_fsExtra) return _fsExtra; return _fs;};
};
