#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "AbstractUITask.h"

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE 13

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "6 Jun 2026"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v1.16.0"
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(ESP32)
#include <SPIFFS.h>
#endif

#include "DataStore.h"
#include "NodePrefs.h"
#include "CannedMessages.h"

#include <RTClib.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <target.h>

/* ---------------------------------- CONFIGURATION ------------------------------------- */

#ifndef LORA_FREQ
#define LORA_FREQ 915.0
#endif
#ifndef LORA_BW
#define LORA_BW 250
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20
#endif
#ifndef MAX_LORA_TX_POWER
#define MAX_LORA_TX_POWER LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
#define MAX_CONTACTS 100
#endif

#ifndef OFFLINE_QUEUE_SIZE
#define OFFLINE_QUEUE_SIZE 16
#endif

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "MeshCore-"
#endif

#include <helpers/BaseChatMesh.h>
#include <helpers/TransportKeyStore.h>

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS             0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE             0x02
#define REQ_TYPE_GET_TELEMETRY_DATA     0x03

struct AdvertPath {
  uint8_t pubkey_prefix[7];
  uint8_t path_len;
  char    name[32];
  uint32_t recv_timestamp;
  uint8_t path[MAX_PATH_SIZE];
};

#ifdef HEADLESS_CANNED_MESSAGES
namespace ringtone_cfg {
  constexpr size_t kMaxToneLen = 192;
  constexpr size_t kMaxDeviceEntries = 12;
  constexpr uint8_t kBlobVersion = 1;
  constexpr size_t kBlobMaxLen = 4096;
}
#endif

class MyMesh : public BaseChatMesh, public DataStoreHost {
public:
  MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui=NULL);

  void begin(bool has_display);
  void startInterface(BaseSerialInterface &serial);

  const char *getNodeName();
  NodePrefs *getNodePrefs();
  uint32_t getBLEPin();
  size_t getHeadlessCannedMessageCount() const { return _cannedMessageCount; }
  const char* getHeadlessCannedMessage(size_t idx) const { return (idx < _cannedMessageCount) ? _cannedMessages[idx] : ""; }
  void logLocalChannelMessage(uint8_t channel_idx, const char* text);

#ifdef HEADLESS_CANNED_MESSAGES
  enum class TapTargetType : uint8_t { Channel = 0, Contact = 1 };
  struct TapTargetState {
    TapTargetType type = TapTargetType::Channel;
    uint8_t channel_idx = 0;
    ChannelDetails channel;
    ContactInfo contact;
  };

  bool resolveTapTarget(TapTargetState& out);
  void describeTapTarget(char* dest, size_t len) const;
#endif

  void loop();
  void handleCmdFrame(size_t len);
  bool advert();
  void enterCLIRescue();

  int  getRecentlyHeard(AdvertPath dest[], int max_num);

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  bool getCADEnabled() const override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet *packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet *packet) override;
  uint8_t getExtraAckTransmitCount() const override;
  bool filterRecvFloodPacket(mesh::Packet* packet) override;
  bool allowPacketForward(const mesh::Packet* packet) override;

  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis);
  void sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis=0) override;
  void sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis=0) override;

  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
  bool isAutoAddEnabled() const override;
  bool shouldAutoAddContactType(uint8_t type) const override;
  bool shouldOverwriteWhenFull() const override;
  uint8_t getAutoAddMaxHops() const override;
  void onContactsFull() override;
  void onContactOverwrite(const uint8_t* pub_key) override;
  bool onContactPathRecv(ContactInfo& from, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) override;
  void onContactPathUpdated(const ContactInfo &contact) override;
  ContactInfo* processAck(const uint8_t *data) override;
  void queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt, uint32_t sender_timestamp,
                    const uint8_t *extra, int extra_len, const char *text);

  void onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                     const char *text) override;
  void onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                         const char *text) override;
  void onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const uint8_t *sender_prefix, const char *text) override;
  void onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                            const char *text) override;
  void onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                         const uint8_t *data, size_t data_len) override;

  uint8_t onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                           uint8_t len, uint8_t *reply) override;
  void onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) override;
  void onControlDataRecv(mesh::Packet *packet) override;
  void onRawDataRecv(mesh::Packet *packet) override;
  void onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) override;

  uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
  uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;
  void onSendTimeout() override;

  // DataStoreHost methods
  bool onContactLoaded(const ContactInfo& contact) override { return addContact(contact); }
  bool getContactForSave(uint32_t idx, ContactInfo& contact) override { return getContactByIdx(idx, contact); }
  bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) override { return setChannel(channel_idx, ch); }
  bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) override { return getChannel(channel_idx, ch); }

  void clearPendingReqs() {
    pending_login = pending_status = pending_telemetry = pending_discovery = pending_req = 0;
  }

public:
  void savePrefs() { _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon); }

#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() {
    sensors.setSettingValue("gps", _prefs.gps_enabled ? "1" : "0");
    if (_prefs.gps_interval > 0) {
      char interval_str[12];  // Max: 24 hours = 86400 seconds (5 digits + null)
      sprintf(interval_str, "%u", _prefs.gps_interval);
      sensors.setSettingValue("gps_interval", interval_str);
    }
  }
#endif

  // To check if there is pending work
  bool hasPendingWork() const;

private:
  void writeOKFrame();
  void writeErrFrame(uint8_t err_code);
  void writeDisabledFrame();
  void writeContactRespFrame(uint8_t code, const ContactInfo &contact);
  void updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len);
  void addToOfflineQueue(const uint8_t frame[], int len);
  int getFromOfflineQueue(uint8_t frame[]);
  int getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) override { 
    return _store->getBlobByKey(key, key_len, dest_buf);
  }
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], int len) override {
    return _store->putBlobByKey(key, key_len, src_buf, len);
  }

  void checkCLIRescueCmd();
  void checkSerialInterface();
  void loadHeadlessCannedMessages();
  void persistHeadlessCannedMessages();
  bool parseCannedMessageList(const char* input, char dest[][canned::kMaxMessageLen], size_t& count);
  bool handleLocalChannelCommand(uint8_t channel_idx, const char* text, const ChannelDetails& channel);
  void emitChannelMessageToApp(uint8_t channel_idx, uint8_t path_len, uint32_t timestamp, const char* text, int8_t snr_quarter, const char* display_name = nullptr, uint8_t txt_type = TXT_TYPE_PLAIN);
  void summarizeCannedMessages(uint8_t channel_idx);
#ifdef HEADLESS_CANNED_MESSAGES
  struct DeviceRingtone {
    bool in_use;
    uint8_t pub_key[PUB_KEY_SIZE];
    char tone[ringtone_cfg::kMaxToneLen];
    uint32_t updated_at;
  };
  void loadHeadlessRingtones();
  void persistHeadlessRingtones();
  bool handleIncomingRingtoneCommand(const ContactInfo* from, int channel_idx, const mesh::GroupChannel* channel, const char* text, bool local_only = false);
  void maybePlayRingtone(const ContactInfo* from, const char* text, uint8_t mode_override = 0xFF);
  const char* getRingtoneFor(const ContactInfo* from) const;
  DeviceRingtone* findRingtoneEntry(const uint8_t* pub_key);
  DeviceRingtone* allocateRingtoneEntry(const uint8_t* pub_key);
  void clearAllRingtones();
  void sendRingtoneChannelReply(int channel_idx, const char* text);
  void summarizeRingtoneStatus(const ContactInfo* from, int channel_idx, bool local_only);
  void sendLocalChannelSystemMessage(uint8_t channel_idx, const char* text);
  bool handleLocalBuzzerCommand(uint8_t channel_idx, const char* text);
  bool handleLocalWpmCommand(uint8_t channel_idx, const char* text);
#ifdef ENABLE_FLIP_MUTE
  bool handleLocalFlipmuteCommand(uint8_t channel_idx, const char* text);
#endif
  bool handleLocalSensorCommand(uint8_t channel_idx, const char* text);
  bool handleLocalPlayCommand(uint8_t channel_idx, const char* text);
  const char* describeNotifyMode(uint8_t mode) const;
  void setDefaultBuzzerPrefs();
  void loadBuzzerPrefs();
  void persistBuzzerPrefs();
  bool tryParseNotifyModeToken(const char* token, uint8_t& out_mode) const;
  uint8_t resolveChannelNotifyMode(uint8_t channel_idx) const;
  void summarizeBuzzerStatus(uint8_t channel_idx, bool include_all_channels);
  bool hasGlobalBuzzerFlag(const char* args) const;
  bool extractFirstBuzzerToken(const char* args, char* token, size_t token_len) const;
  void loadTapTargetPrefs();
  void persistTapTargetPrefs();
  void setDefaultTapTarget();
  void setTapTargetChannel(uint8_t channel_idx, const ChannelDetails& channel);
  void setTapTargetContact(const ContactInfo& contact);
  void sendTapStatusToApp(int channel_idx, const char* message);
  bool handleTapCommandForContact(const ContactInfo& contact, const char* text);
#endif

  // Low-battery auto-notify (/lobat here)
  void initLobat();
  void checkLobat();
  void sendLobatMessage();
  const char* describeLobatStatus() const;

  // Geofence (/mark)
  static constexpr uint8_t  kMarkMaxPoints = 12;
  void initMark();
  bool addMarkPoint(double lat, double lon);
  void clearMarkPoints();
  void setMarkEnabled(bool on);
  void setMarkChannel(uint8_t channel_idx);
  void clearGeofenceFile();
  bool isMarkEnabled() const { return _mark_enabled; }
  uint8_t getMarkPointCount() const { return _mark_point_count; }
  void getMarkPoint(uint8_t idx, double& lat, double& lon) const;
  void saveMarkPrefs();
  bool loadMarkPrefs();
  bool isPointInsideMark(double lat, double lon) const;
  const char* describeMarkStatus() const;
  void checkMark();
  void sendMarkAlert();
  void sendMarkInsideAlert();
  void sendMarkLocationAdvert();
  // Returns the two nearest fence-point indices (closest, second-closest).
  void nearestMarkPoints(double lat, double lon, uint8_t& out_idx0, uint8_t& out_idx1) const;
  // Build a 7x7 ASCII art map centered on the device.
  void buildMarkMap(double lat, double lon, char* out, size_t out_size) const;

  bool computePacketHash(const mesh::Packet* pkt, uint8_t out_hash[MAX_HASH_SIZE]) const;
  bool isRecentDuplicate(const uint8_t hash[MAX_HASH_SIZE]);
  bool isDuplicateGroupMessage(const uint8_t hash[MAX_HASH_SIZE]);
  bool shouldNotifyForPacket(const uint8_t hash[MAX_HASH_SIZE]);
  bool isValidClientRepeatFreq(uint32_t f) const;

  // helpers, short-cuts
  void saveChannels() { _store->saveChannels(this); }
  void saveContacts();

  DataStore* _store;
  NodePrefs _prefs;
  uint32_t pending_login;
  uint32_t pending_status;
  uint32_t pending_telemetry, pending_discovery;   // pending _TELEMETRY_REQ
  uint32_t pending_req;   // pending _BINARY_REQ
  BaseSerialInterface *_serial;
  AbstractUITask* _ui;

  ContactsIterator _iter;
  uint32_t _iter_filter_since;
  uint32_t _most_recent_lastmod;
  uint32_t _active_ble_pin;
  bool _iter_started;
  bool _cli_rescue;
  bool send_unscoped;   // force un-scoped flood (instead of using send_scope)
  char cli_command[80];
  uint8_t app_target_ver;
  uint8_t *sign_data;
  uint32_t sign_data_len;
  unsigned long dirty_contacts_expiry;
  size_t _cannedMessageCount;
  char _cannedMessages[canned::kMaxMessages][canned::kMaxMessageLen];

  TransportKey send_scope;

#ifdef HEADLESS_CANNED_MESSAGES
  char _globalRingtone[ringtone_cfg::kMaxToneLen];
  uint8_t _ringtoneRepeatCount;  // Number of times to repeat RTTTL ringtone (1-10, default 1)
  DeviceRingtone _deviceRingtones[ringtone_cfg::kMaxDeviceEntries];
  BuzzerPrefs _buzzerPrefs;
  TapTargetPrefs _tapTarget;
#endif
  // Low-battery auto-notify state
  static constexpr uint32_t kLobatCheckIntervalMs = 180000; // 3 minutes
  static constexpr uint8_t  kLobatDefaultThresholdPct = 20;  // default 20%
  static constexpr uint8_t  kLobatMaxCount       = 3;       // max 3 alerts
  uint8_t  _lobat_channel_idx;   // 0xFF = not set, otherwise the target channel
  uint8_t  _lobat_count;         // sends so far
  uint32_t _lobat_next_check_ms; // next check timestamp (millis)
  uint8_t  _lobat_threshold_pct; // threshold percentage, default kLobatDefaultThresholdPct

  // Geofence (/mark) state
  static constexpr uint32_t kMarkCheckIntervalMs = 180000; // 3 minutes
  bool     _mark_enabled;
  bool     _mark_outside;
  uint8_t  _mark_point_count;
  double   _mark_lats[kMarkMaxPoints];
  double   _mark_lons[kMarkMaxPoints];
  uint32_t _mark_next_check_ms;
  double   _mark_last_alert_lat;   // last position we sent an alert from
  double   _mark_last_alert_lon;
  uint8_t  _mark_channel_idx;   // 0xFF = not set, target channel for alerts

  uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
  uint8_t out_frame[MAX_FRAME_SIZE + 1];
  CayenneLPP telemetry;

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];

    bool isChannelMsg() const;
  };
  int offline_queue_len;
  Frame offline_queue[OFFLINE_QUEUE_SIZE];

  // Group message replay guard: keyed by packet hash.
  static constexpr uint8_t kRecentGrpSize = 24;
  static constexpr uint32_t kRecentGrpTtlMs = 120000; // 2 minutes
  struct RecentGroupEntry {
    bool valid;
    uint8_t hash[MAX_HASH_SIZE];
    uint32_t seen_at_ms;
  };
  RecentGroupEntry _recent_grp[kRecentGrpSize];
  uint8_t _recent_grp_idx;

  // Short recent-packet cache to suppress duplicate flood/forward copies.
  static constexpr uint8_t kRecentRxSize = 16;
  static constexpr uint32_t kRecentRxTtlMs = 60000; // 60s window is plenty for replays
  struct RecentRxEntry {
    bool valid;
    uint8_t hash[MAX_HASH_SIZE];
    uint32_t seen_at_ms;
  };
  RecentRxEntry _recent_rx[kRecentRxSize];
  uint8_t _recent_rx_idx;

  // Prevent repeated ringtone starts for the same packet hash.
  struct LastNotify {
    bool valid;
    uint8_t hash[MAX_HASH_SIZE];
    uint32_t seen_at_ms;
  };
  LastNotify _last_notify;

  // CW-mode: suppress repeat playback for identical packet hashes.
  bool _last_cw_group_valid;
  uint8_t _last_cw_group_hash[MAX_HASH_SIZE];

  struct AckTableEntry {
    unsigned long msg_sent;
    uint32_t ack;
    ContactInfo* contact;
  };
  #define EXPECTED_ACK_TABLE_SIZE 8
  AckTableEntry expected_ack_table[EXPECTED_ACK_TABLE_SIZE]; // circular table
  int next_ack_idx;

  #define ADVERT_PATH_TABLE_SIZE   16
  AdvertPath advert_paths[ADVERT_PATH_TABLE_SIZE]; // circular table
};

extern MyMesh the_mesh;
