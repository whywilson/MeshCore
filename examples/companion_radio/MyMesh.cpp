#include "MyMesh.h"

#include <Arduino.h> // needed for PlatformIO
#include <Mesh.h>
#include <cctype>
#include <cstring>

#define CMD_APP_START                     1
#define CMD_SEND_TXT_MSG                  2
#define CMD_SEND_CHANNEL_TXT_MSG          3
#define CMD_GET_CONTACTS                  4 // with optional 'since' (for efficient sync)
#define CMD_GET_DEVICE_TIME               5
#define CMD_SET_DEVICE_TIME               6
#define CMD_SEND_SELF_ADVERT              7
#define CMD_SET_ADVERT_NAME               8
#define CMD_ADD_UPDATE_CONTACT            9
#define CMD_SYNC_NEXT_MESSAGE             10
#define CMD_SET_RADIO_PARAMS              11
#define CMD_SET_RADIO_TX_POWER            12
#define CMD_RESET_PATH                    13
#define CMD_SET_ADVERT_LATLON             14
#define CMD_REMOVE_CONTACT                15
#define CMD_SHARE_CONTACT                 16
#define CMD_EXPORT_CONTACT                17
#define CMD_IMPORT_CONTACT                18
#define CMD_REBOOT                        19
#define CMD_GET_BATT_AND_STORAGE          20 // was CMD_GET_BATTERY_VOLTAGE
#define CMD_SET_TUNING_PARAMS             21
#define CMD_DEVICE_QUERY                  22
#define CMD_EXPORT_PRIVATE_KEY            23
#define CMD_IMPORT_PRIVATE_KEY            24
#define CMD_SEND_RAW_DATA                 25
#define CMD_SEND_LOGIN                    26
#define CMD_SEND_STATUS_REQ               27
#define CMD_HAS_CONNECTION                28
#define CMD_LOGOUT                        29 // 'Disconnect'
#define CMD_GET_CONTACT_BY_KEY            30
#define CMD_GET_CHANNEL                   31
#define CMD_SET_CHANNEL                   32
#define CMD_SIGN_START                    33
#define CMD_SIGN_DATA                     34
#define CMD_SIGN_FINISH                   35
#define CMD_SEND_TRACE_PATH               36
#define CMD_SET_DEVICE_PIN                37
#define CMD_SET_OTHER_PARAMS              38
#define CMD_SEND_TELEMETRY_REQ            39 // can deprecate this
#define CMD_GET_CUSTOM_VARS               40
#define CMD_SET_CUSTOM_VAR                41
#define CMD_GET_ADVERT_PATH               42
#define CMD_GET_TUNING_PARAMS             43
// NOTE: CMD range 44..49 parked, potentially for WiFi operations
#define CMD_SEND_BINARY_REQ           50
#define CMD_FACTORY_RESET             51
#define CMD_SEND_PATH_DISCOVERY_REQ   52
#define CMD_SET_FLOOD_SCOPE_KEY       54   // v8+
#define CMD_SEND_CONTROL_DATA         55   // v8+
#define CMD_GET_STATS                 56   // v8+, second byte is stats type
#define CMD_SEND_ANON_REQ             57
#define CMD_SET_AUTOADD_CONFIG        58
#define CMD_GET_AUTOADD_CONFIG        59
#define CMD_GET_ALLOWED_REPEAT_FREQ   60
#define CMD_SET_PATH_HASH_MODE        61
#define CMD_SEND_CHANNEL_DATA         62
#define CMD_SET_DEFAULT_FLOOD_SCOPE   63
#define CMD_GET_DEFAULT_FLOOD_SCOPE   64
#define CMD_SEND_RAW_PACKET           65

// Stats sub-types for CMD_GET_STATS
#define STATS_TYPE_CORE               0
#define STATS_TYPE_RADIO              1
#define STATS_TYPE_PACKETS             2

#define RESP_CODE_OK                  0
#define RESP_CODE_ERR                 1
#define RESP_CODE_CONTACTS_START      2  // first reply to CMD_GET_CONTACTS
#define RESP_CODE_CONTACT             3  // multiple of these (after CMD_GET_CONTACTS)
#define RESP_CODE_END_OF_CONTACTS     4  // last reply to CMD_GET_CONTACTS
#define RESP_CODE_SELF_INFO           5  // reply to CMD_APP_START
#define RESP_CODE_SENT                6  // reply to CMD_SEND_TXT_MSG
#define RESP_CODE_CONTACT_MSG_RECV    7  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CHANNEL_MSG_RECV    8  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CURR_TIME           9  // a reply to CMD_GET_DEVICE_TIME
#define RESP_CODE_NO_MORE_MESSAGES    10 // a reply to CMD_SYNC_NEXT_MESSAGE
#define RESP_CODE_EXPORT_CONTACT      11
#define RESP_CODE_BATT_AND_STORAGE    12 // a reply to a CMD_GET_BATT_AND_STORAGE
#define RESP_CODE_DEVICE_INFO         13 // a reply to CMD_DEVICE_QUERY
#define RESP_CODE_PRIVATE_KEY         14 // a reply to CMD_EXPORT_PRIVATE_KEY
#define RESP_CODE_DISABLED            15
#define RESP_CODE_CONTACT_MSG_RECV_V3 16 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_MSG_RECV_V3 17 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_INFO        18 // a reply to CMD_GET_CHANNEL
#define RESP_CODE_SIGN_START          19
#define RESP_CODE_SIGNATURE           20
#define RESP_CODE_CUSTOM_VARS         21
#define RESP_CODE_ADVERT_PATH         22
#define RESP_CODE_TUNING_PARAMS       23
#define RESP_CODE_STATS               24   // v8+, second byte is stats type
#define RESP_CODE_AUTOADD_CONFIG      25
#define RESP_ALLOWED_REPEAT_FREQ      26
#define RESP_CODE_CHANNEL_DATA_RECV   27
#define RESP_CODE_DEFAULT_FLOOD_SCOPE 28

#define MAX_CHANNEL_DATA_LENGTH       (MAX_FRAME_SIZE - 9)

#define SEND_TIMEOUT_BASE_MILLIS        500
#define FLOOD_SEND_TIMEOUT_FACTOR       16.0f
#define DIRECT_SEND_PERHOP_FACTOR       6.0f
#define DIRECT_SEND_PERHOP_EXTRA_MILLIS 250
#define LAZY_CONTACTS_WRITE_DELAY       5000

#define PUBLIC_GROUP_PSK                "izOH6cXN6mrJ5e26oRXNcg=="

// these are _pushed_ to client app at any time
#define PUSH_CODE_ADVERT                  0x80
#define PUSH_CODE_PATH_UPDATED            0x81
#define PUSH_CODE_SEND_CONFIRMED          0x82
#define PUSH_CODE_MSG_WAITING             0x83
#define PUSH_CODE_RAW_DATA                0x84
#define PUSH_CODE_LOGIN_SUCCESS           0x85
#define PUSH_CODE_LOGIN_FAIL              0x86
#define PUSH_CODE_STATUS_RESPONSE         0x87
#define PUSH_CODE_LOG_RX_DATA             0x88
#define PUSH_CODE_TRACE_DATA              0x89
#define PUSH_CODE_NEW_ADVERT              0x8A
#define PUSH_CODE_TELEMETRY_RESPONSE      0x8B
#define PUSH_CODE_BINARY_RESPONSE         0x8C
#define PUSH_CODE_PATH_DISCOVERY_RESPONSE 0x8D
#define PUSH_CODE_CONTROL_DATA            0x8E // v8+
#define PUSH_CODE_CONTROL_DATA          0x8E   // v8+
#define PUSH_CODE_CONTACT_DELETED       0x8F // used to notify client app of deleted contact when overwriting oldest
#define PUSH_CODE_CONTACTS_FULL         0x90 // used to notify client app that contacts storage is full

#define ERR_CODE_UNSUPPORTED_CMD          1
#define ERR_CODE_NOT_FOUND                2
#define ERR_CODE_TABLE_FULL               3
#define ERR_CODE_BAD_STATE                4
#define ERR_CODE_FILE_IO_ERROR            5
#define ERR_CODE_ILLEGAL_ARG              6

#define MAX_SIGN_DATA_LEN                 (8 * 1024) // 8K

// Auto-add config bitmask
// Bit 0: If set, overwrite oldest non-favourite contact when contacts file is full
// Bits 1-4: these indicate which contact types to auto-add when manual_contact_mode = 0x01
#define AUTO_ADD_OVERWRITE_OLDEST (1 << 0)  // 0x01 - overwrite oldest non-favourite when full
#define AUTO_ADD_CHAT             (1 << 1)  // 0x02 - auto-add Chat (Companion) (ADV_TYPE_CHAT)
#define AUTO_ADD_REPEATER         (1 << 2)  // 0x04 - auto-add Repeater (ADV_TYPE_REPEATER)
#define AUTO_ADD_ROOM_SERVER      (1 << 3)  // 0x08 - auto-add Room Server (ADV_TYPE_ROOM)
#define AUTO_ADD_SENSOR           (1 << 4)  // 0x10 - auto-add Sensor (ADV_TYPE_SENSOR)

void MyMesh::writeOKFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_OK;
  _serial->writeFrame(buf, 1);
}
void MyMesh::writeErrFrame(uint8_t err_code) {
  uint8_t buf[2];
  buf[0] = RESP_CODE_ERR;
  buf[1] = err_code;
  _serial->writeFrame(buf, 2);
}

void MyMesh::writeDisabledFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_DISABLED;
  _serial->writeFrame(buf, 1);
}

void MyMesh::writeContactRespFrame(uint8_t code, const ContactInfo &contact) {
  int i = 0;
  out_frame[i++] = code;
  memcpy(&out_frame[i], contact.id.pub_key, PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  out_frame[i++] = contact.type;
  out_frame[i++] = contact.flags;
  out_frame[i++] = contact.out_path_len;
  memcpy(&out_frame[i], contact.out_path, MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  StrHelper::strzcpy((char *)&out_frame[i], contact.name, 32);
  i += 32;
  memcpy(&out_frame[i], &contact.last_advert_timestamp, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lat, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lon, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.lastmod, 4);
  i += 4;
  _serial->writeFrame(out_frame, i);
}

void MyMesh::updateContactFromFrame(ContactInfo &contact, uint32_t &last_mod, const uint8_t *frame, int len) {
  int i = 0;
  uint8_t code = frame[i++]; // eg. CMD_ADD_UPDATE_CONTACT
  memcpy(contact.id.pub_key, &frame[i], PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  contact.type = frame[i++];
  contact.flags = frame[i++];
  contact.out_path_len = frame[i++];
  memcpy(contact.out_path, &frame[i], MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  memcpy(contact.name, &frame[i], 32);
  i += 32;
  memcpy(&contact.last_advert_timestamp, &frame[i], 4);
  i += 4;
  if (len >= i + 8) { // optional fields
    memcpy(&contact.gps_lat, &frame[i], 4);
    i += 4;
    memcpy(&contact.gps_lon, &frame[i], 4);
    i += 4;
    if (len >= i + 4) {
      memcpy(&last_mod, &frame[i], 4);
    }
  }
}

bool MyMesh::Frame::isChannelMsg() const {
  return buf[0] == RESP_CODE_CHANNEL_MSG_RECV || buf[0] == RESP_CODE_CHANNEL_MSG_RECV_V3 ||
         buf[0] == RESP_CODE_CHANNEL_DATA_RECV;
}

void MyMesh::addToOfflineQueue(const uint8_t frame[], int len) {
  if (offline_queue_len >= OFFLINE_QUEUE_SIZE) {
    MESH_DEBUG_PRINTLN("WARN: offline_queue is full!");
    int pos = 0;
    while (pos < offline_queue_len) {
      if (offline_queue[pos].isChannelMsg()) {
        for (int i = pos; i < offline_queue_len - 1; i++) { // delete oldest channel msg from queue
          offline_queue[i] = offline_queue[i + 1];
        }
        MESH_DEBUG_PRINTLN("INFO: removed oldest channel message from queue.");
        offline_queue[offline_queue_len - 1].len = len;
        memcpy(offline_queue[offline_queue_len - 1].buf, frame, len);
        return;
      }
      pos++;
    }
    MESH_DEBUG_PRINTLN("INFO: no channel messages to remove from queue.");
  } else {
    offline_queue[offline_queue_len].len = len;
    memcpy(offline_queue[offline_queue_len].buf, frame, len);
    offline_queue_len++;
  }
}

int MyMesh::getFromOfflineQueue(uint8_t frame[]) {
  if (offline_queue_len > 0) {         // check offline queue
    size_t len = offline_queue[0].len; // take from top of queue
    memcpy(frame, offline_queue[0].buf, len);

    offline_queue_len--;
    for (int i = 0; i < offline_queue_len; i++) { // delete top item from queue
      offline_queue[i] = offline_queue[i + 1];
    }
    return len;
  }
  return 0; // queue is empty
}

float MyMesh::getAirtimeBudgetFactor() const {
  return _prefs.airtime_factor;
}

int MyMesh::getInterferenceThreshold() const {
  return 0; // disabled for now, until currentRSSI() problem is resolved
}
bool MyMesh::getCADEnabled() const {
  return true; // hardware CAD before TX (no CLI toggle on companion; enabled by default)
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int)((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * 0.5f);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * 0.2f);
  return getRNG()->nextInt(0, 5*t + 1);
}

uint8_t MyMesh::getExtraAckTransmitCount() const {
  return _prefs.multi_acks;
}

void MyMesh::logRxRaw(float snr, float rssi, const uint8_t raw[], int len) {
  if (_serial->isConnected() && len + 3 <= MAX_FRAME_SIZE) {
    int i = 0;
    out_frame[i++] = PUSH_CODE_LOG_RX_DATA;
    out_frame[i++] = (int8_t)(snr * 4);
    out_frame[i++] = (int8_t)(rssi);
    memcpy(&out_frame[i], raw, len);
    i += len;

    _serial->writeFrame(out_frame, i);
  }
}

bool MyMesh::isAutoAddEnabled() const {
  return (_prefs.manual_add_contacts & 1) == 0;
}

bool MyMesh::shouldAutoAddContactType(uint8_t contact_type) const {
  if ((_prefs.manual_add_contacts & 1) == 0) {
    return true;
  }

  uint8_t type_bit = 0;
  switch (contact_type) {
    case ADV_TYPE_CHAT:
      type_bit = AUTO_ADD_CHAT;
      break;
    case ADV_TYPE_REPEATER:
      type_bit = AUTO_ADD_REPEATER;
      break;
    case ADV_TYPE_ROOM:
      type_bit = AUTO_ADD_ROOM_SERVER;
      break;
    case ADV_TYPE_SENSOR:
      type_bit = AUTO_ADD_SENSOR;
      break;
    default:
      return false;  // Unknown type, don't auto-add
  }

  return (_prefs.autoadd_config & type_bit) != 0;
}

bool MyMesh::shouldOverwriteWhenFull() const {
  return (_prefs.autoadd_config & AUTO_ADD_OVERWRITE_OLDEST) != 0;
}

uint8_t MyMesh::getAutoAddMaxHops() const {
  return _prefs.autoadd_max_hops;
}

void MyMesh::onContactOverwrite(const uint8_t* pub_key) {
    _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE); // delete from storage
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACT_DELETED;
    memcpy(&out_frame[1], pub_key, PUB_KEY_SIZE);
    _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
  }
}

void MyMesh::onContactsFull() {
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACTS_FULL;
    _serial->writeFrame(out_frame, 1);
  }
}

void MyMesh::onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) {
  if (_serial->isConnected()) {
    if (is_new) {
      writeContactRespFrame(PUSH_CODE_NEW_ADVERT, contact);
    } else {
      out_frame[0] = PUSH_CODE_ADVERT;
      memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
      _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
    }
  } else {
#if defined(DISPLAY_CLASS) && !defined(HEADLESS_CANNED_MESSAGES)
    if (_ui) _ui->notify(UIEventType::newContactMessage);
#endif
  }

  // add inbound-path to mem cache
  if (path && mesh::Packet::isValidPathLen(path_len)) {  // check path is valid
    AdvertPath* p = advert_paths;
    uint32_t oldest = 0xFFFFFFFF;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) { // check if already in table, otherwise evict oldest
      if (memcmp(advert_paths[i].pubkey_prefix, contact.id.pub_key, sizeof(AdvertPath::pubkey_prefix)) == 0) {
        p = &advert_paths[i]; // found
        break;
      }
      if (advert_paths[i].recv_timestamp < oldest) {
        oldest = advert_paths[i].recv_timestamp;
        p = &advert_paths[i];
      }
    }

    memcpy(p->pubkey_prefix, contact.id.pub_key, sizeof(p->pubkey_prefix));
    strcpy(p->name, contact.name);
    p->recv_timestamp = getRTCClock()->getCurrentTime();
    p->path_len = mesh::Packet::copyPath(p->path, path, path_len);
  }

  if (!is_new) dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY); // only schedule lazy write for contacts that are in contacts[]
}

static int sort_by_recent(const void *a, const void *b) {
  return ((AdvertPath *)b)->recv_timestamp - ((AdvertPath *)a)->recv_timestamp;
}

int MyMesh::getRecentlyHeard(AdvertPath dest[], int max_num) {
  if (max_num > ADVERT_PATH_TABLE_SIZE) max_num = ADVERT_PATH_TABLE_SIZE;
  qsort(advert_paths, ADVERT_PATH_TABLE_SIZE, sizeof(advert_paths[0]), sort_by_recent);

  for (int i = 0; i < max_num; i++) {
    dest[i] = advert_paths[i];
  }
  return max_num;
}

void MyMesh::onContactPathUpdated(const ContactInfo &contact) {
  out_frame[0] = PUSH_CODE_PATH_UPDATED;
  memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
  _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE); // NOTE: app may not be connected

  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
}

ContactInfo *MyMesh::processAck(const uint8_t *data) {
  // see if matches any in a table
  for (int i = 0; i < EXPECTED_ACK_TABLE_SIZE; i++) {
    if (memcmp(data, &expected_ack_table[i].ack, 4) == 0) { // got an ACK from recipient
      out_frame[0] = PUSH_CODE_SEND_CONFIRMED;
      memcpy(&out_frame[1], data, 4);
      uint32_t trip_time = _ms->getMillis() - expected_ack_table[i].msg_sent;
      memcpy(&out_frame[5], &trip_time, 4);
      _serial->writeFrame(out_frame, 9);

      // NOTE: the same ACK can be received multiple times!
      expected_ack_table[i].ack = 0; // clear expected hash, now that we have received ACK
      return expected_ack_table[i].contact;
    }
  }
  return checkConnectionsAck(data);
}

void MyMesh::queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt,
                          uint32_t sender_timestamp, const uint8_t *extra, int extra_len, const char *text) {
  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV;
  }
  memcpy(&out_frame[i], from.id.pub_key, 6);
  i += 6; // just 6-byte prefix
  uint8_t path_len = out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  out_frame[i++] = txt_type;
  memcpy(&out_frame[i], &sender_timestamp, 4);
  i += 4;
  if (extra_len > 0) {
    memcpy(&out_frame[i], extra, extra_len);
    i += extra_len;
  }
  int tlen = strlen(text); // TODO: UTF-8 ??
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = MAX_FRAME_SIZE - i;
  }
  memcpy(&out_frame[i], text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }
#ifdef DISPLAY_CLASS
  // we only want to show text messages on display, not cli data
  bool should_display = txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN;
  if (should_display && _ui) {
    _ui->newMsg(path_len, from.name, text, offline_queue_len);
#if !defined(HEADLESS_CANNED_MESSAGES)
    if (!_serial->isConnected()) {
      _ui->notify(UIEventType::contactMessage);
    }
#endif
  }
#endif
  // For headless TapTap-style builds, play CW/RTTTL for
  // all direct (contact) messages here so both signed and
  // plain texts go through a single notification path.
#ifdef HEADLESS_CANNED_MESSAGES
  maybePlayRingtone(&from, text);
#endif
}

void MyMesh::emitChannelMessageToApp(uint8_t channel_idx, uint8_t path_len, uint32_t timestamp,
                                     const char *text, int8_t snr_quarter, const char *display_name,
                                     uint8_t txt_type) {
  if (!text) return;

  const char *payload = text;
  char composed[320];
  if (display_name && *display_name) {
    size_t name_len = strlen(display_name);
    bool already_prefixed = false;
    if (strlen(text) > name_len + 2) {
      if (strncmp(text, display_name, name_len) == 0 && text[name_len] == ':' && text[name_len + 1] == ' ') {
        already_prefixed = true;
      }
    }
    if (!already_prefixed) {
      snprintf(composed, sizeof(composed), "%s: %s", display_name, text);
      payload = composed;
    }
  }

  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
    out_frame[i++] = snr_quarter;
    out_frame[i++] = 0;
    out_frame[i++] = 0;
  } else {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV;
  }

  out_frame[i++] = channel_idx;
  out_frame[i++] = path_len;
  out_frame[i++] = txt_type;
  memcpy(&out_frame[i], &timestamp, 4);
  i += 4;
  int tlen = strlen(payload);
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = MAX_FRAME_SIZE - i;
  }
  memcpy(&out_frame[i], payload, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1] = { PUSH_CODE_MSG_WAITING };
    _serial->writeFrame(frame, 1);
  } else {
#if defined(DISPLAY_CLASS) && !defined(HEADLESS_CANNED_MESSAGES)
    if (_ui) _ui->notify(UIEventType::channelMessage);
#endif
  }

#ifdef DISPLAY_CLASS
  const char *channel_name = display_name ? display_name : "Unknown";
  if (!display_name) {
    ChannelDetails channel_details;
    if (getChannel(channel_idx, channel_details)) {
      channel_name = channel_details.name;
    }
  }
  if (_ui) _ui->newMsg(path_len, channel_name, text, offline_queue_len);
#endif
}

void MyMesh::logLocalChannelMessage(uint8_t channel_idx, const char *text) {
  if (!text) return;
  // Use path_len 0 to indicate a local-origin message so the app can treat it as outbound.
  emitChannelMessageToApp(channel_idx, 0, getRTCClock()->getCurrentTimeUnique(), text, 0, _prefs.node_name,
                          TXT_TYPE_PLAIN);
}

#ifdef HEADLESS_CANNED_MESSAGES
static inline const char *skipWhitespace(const char *ptr);
static inline bool isNotifyDelimiter(char c) {
  return c == 0 || isspace((unsigned char)c) || c == ',' || c == ';' || c == '|' || c == '\\' || c == '/';
}

namespace {
static inline bool isSpaceLike(unsigned char c) {
  return isspace(c) || c == 0xA0 || c == 0xC2;
}

static const char *trimLeft(const char *ptr) {
  while (ptr && *ptr && isSpaceLike((unsigned char)*ptr)) {
    ++ptr;
  }
  return ptr;
}

static bool matchesToken(const char *text, const char *token) {
  size_t i = 0;
  while (token[i] && text[i]) {
    if (tolower((unsigned char)text[i]) != tolower((unsigned char)token[i])) {
      return false;
    }
    ++i;
  }
  if (token[i] != 0) return false;
  return text[i] == 0 || isspace((unsigned char)text[i]);
}

static size_t copyTrimmed(const char *src, char *dest, size_t dest_size, bool &truncated) {
  truncated = false;
  if (dest_size == 0) return 0;
  const char *start = trimLeft(src);
  const char *end = start + strlen(start);
  while (end > start && isSpaceLike((unsigned char)*(end - 1))) {
    --end;
  }
  size_t actual_len = (size_t)(end - start);
  size_t copy_len = actual_len;
  if (copy_len >= dest_size) {
    copy_len = dest_size - 1;
    truncated = true;
  }
  if (copy_len > 0) {
    memcpy(dest, start, copy_len);
  }
  dest[copy_len] = 0;
  return actual_len;
}

static uint8_t *ringtoneScratch() {
  static uint8_t blob[ringtone_cfg::kBlobMaxLen];
  return blob;
}
} // namespace

// Standard Morse: mapping to '.' (dot) and '-' (dash)
static const char *getMorseCode(char c) {
  c = toupper((unsigned char)c);
  switch (c) {
  case 'A':
    return ".-";
  case 'B':
    return "-...";
  case 'C':
    return "-.-.";
  case 'D':
    return "-..";
  case 'E':
    return ".";
  case 'F':
    return "..-.";
  case 'G':
    return "--.";
  case 'H':
    return "....";
  case 'I':
    return "..";
  case 'J':
    return ".---";
  case 'K':
    return "-.-";
  case 'L':
    return ".-..";
  case 'M':
    return "--";
  case 'N':
    return "-.";
  case 'O':
    return "---";
  case 'P':
    return ".--.";
  case 'Q':
    return "--.-";
  case 'R':
    return ".-.";
  case 'S':
    return "...";
  case 'T':
    return "-";
  case 'U':
    return "..-";
  case 'V':
    return "...-";
  case 'W':
    return ".--";
  case 'X':
    return "-..-";
  case 'Y':
    return "-.--";
  case 'Z':
    return "--..";
  case '0':
    return "-----";
  case '1':
    return ".----";
  case '2':
    return "..---";
  case '3':
    return "...--";
  case '4':
    return "....-";
  case '5':
    return ".....";
  case '6':
    return "-....";
  case '7':
    return "--...";
  case '8':
    return "---..";
  case '9':
    return "----.";
  default:
    return NULL;
  }
}

// Build RTTTL string approximating Morse timing
// Returns true if at least one dot/dash was emitted
// dot  = 1 unit beep, dash ≈ 4 units (slightly longer for clarity)
// intra-element gap = 1 unit silence, inter-letter gap ≈ 3 units, word gap ≈ 7 units
static bool buildMorseRtttl(const char *message, char *rtttl_out, size_t max_len, uint8_t wpm = 15) {
  if (!message || !rtttl_out || max_len == 0) return false;
  if (wpm < 5) wpm = 5;
  if (wpm > 60) wpm = 60;
  // BPM for RTTTL 8th note = PARIS dit duration: bpm = 25 * wpm
  int bpm = 25 * wpm;

  size_t pos = 0;
  char header[32];
  snprintf(header, sizeof(header), "CW:d=8,o=5,b=%d:", bpm);
  size_t hdr_len = strlen(header);
  if (hdr_len >= max_len) {
    rtttl_out[0] = 0;
    return false;
  }
  memcpy(rtttl_out, header, hdr_len);
  pos = hdr_len;

  bool needComma = false;
  bool emitted = false;

  // Add ~1s of initial silence so CW starts after a short delay
  // At b=360, "2p" (~0.33s) + ",4p" (~0.66s) ≈ 1s total.
  if (pos + 5 < max_len) {
    rtttl_out[pos++] = '2';
    rtttl_out[pos++] = 'p';
    rtttl_out[pos++] = ',';
    rtttl_out[pos++] = '4';
    rtttl_out[pos++] = 'p';
    needComma = true;
  }

  for (size_t i = 0; message[i] && pos + 4 < max_len; ++i) {
    char ch = message[i];
    if (ch == ' ') {
      // word gap ~7 units: use 7p
      if (needComma && pos < max_len - 1) rtttl_out[pos++] = ',';
      if (pos + 2 >= max_len) break;
      rtttl_out[pos++] = '7';
      rtttl_out[pos++] = 'p';
      needComma = true;
      continue;
    }

    const char *code = getMorseCode(ch);
    if (!code) {
      continue; // skip unsupported chars
    }

    // Explicit durations required by RTTTL spec
    static const char kDotNote[] = "8c#";  // single unit (~dot)
    static const char kDashNote[] = "2c#"; // ~4 units (~dash)
    for (size_t j = 0; code[j] && pos + 5 < max_len; ++j) {
      // beep: dot or dash
      if (needComma && pos < max_len - 1) rtttl_out[pos++] = ',';
      if (code[j] == '.') {
        // dot: 1 unit of ~550Hz tone
        size_t dot_len = sizeof(kDotNote) - 1;
        if (pos + dot_len >= max_len) break;
        memcpy(&rtttl_out[pos], kDotNote, dot_len);
        pos += dot_len;
        emitted = true;
      } else {
        // dash: 3 units of ~550Hz tone
        size_t dash_len = sizeof(kDashNote) - 1;
        if (pos + dash_len >= max_len) break;
        memcpy(&rtttl_out[pos], kDashNote, dash_len);
        pos += dash_len;
        emitted = true;
      }
      needComma = true;

      // intra-element gap: 1 unit of silence (explicit 8th rest)
      if (pos + 3 >= max_len) break;
      rtttl_out[pos++] = ',';
      rtttl_out[pos++] = '8';
      rtttl_out[pos++] = 'p';
      needComma = true;
    }

    // extra inter-letter gap of ~2 units (we already had 1 unit from last rest)
    if (pos + 3 >= max_len) break;
    rtttl_out[pos++] = ',';
    rtttl_out[pos++] = '4';
    rtttl_out[pos++] = 'p';
    needComma = true;
  }

  if (pos < max_len) {
    rtttl_out[pos] = 0;
  } else {
    rtttl_out[max_len - 1] = 0;
  }
  return emitted;
}

// Compute a lightweight hash of text content for notification dedupe (ignores routing/path changes).
static bool computeTextHash(const char *text, uint8_t out_hash[MAX_HASH_SIZE]) {
  if (!text || !out_hash) return false;
  // 32-bit FNV-1a then expand to 16 bytes by repetition
  uint32_t h = 2166136261u;
  for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
    h ^= *p;
    h *= 16777619u;
  }
  for (int i = 0; i < MAX_HASH_SIZE; ++i) {
    out_hash[i] = (uint8_t)((h >> ((i % 4) * 8)) & 0xFF);
  }
  return true;
}

// For CW, truncate overly long RTTTL to reduce overrun; ensure trailing null.
static void clampRtttl(char *buf, size_t max_len) {
  if (!buf || max_len == 0) return;
  size_t len = strlen(buf);
  if (len >= max_len) {
    buf[max_len - 1] = '\0';
  }
}

static uint16_t countRtttlTokens(const char *s) {
  if (!s) return 0;
  uint16_t cnt = 0;
  bool in_token = false;
  for (const char *p = s; *p; ++p) {
    if (*p == ',') {
      if (in_token) ++cnt;
      in_token = false;
    } else {
      in_token = true;
    }
  }
  if (in_token) ++cnt;
  return cnt;
}

void MyMesh::sendLocalChannelSystemMessage(uint8_t channel_idx, const char *text) {
  if (text == NULL) return;
  logLocalChannelMessage(channel_idx, text);
}

void MyMesh::setDefaultTapTarget() {
  _tapTarget.version = TapTargetPrefs::kVersion;
  _tapTarget.type = static_cast<uint8_t>(TapTargetType::Channel);
  _tapTarget.channel_idx = 0;
  _tapTarget.reserved = 0;
  memset(_tapTarget.contact_pub_key, 0, sizeof(_tapTarget.contact_pub_key));
  StrHelper::strzcpy(_tapTarget.name, "Public", sizeof(_tapTarget.name));
}

void MyMesh::persistTapTargetPrefs() {
  _store->saveTapTarget(_tapTarget);
}

void MyMesh::loadTapTargetPrefs() {
  setDefaultTapTarget();

  TapTargetPrefs stored;
  if (_store->loadTapTarget(stored) && stored.version == TapTargetPrefs::kVersion) {
    _tapTarget = stored;
  }

  if (_tapTarget.type == static_cast<uint8_t>(TapTargetType::Contact)) {
    ContactInfo *found = lookupContactByPubKey(_tapTarget.contact_pub_key, PUB_KEY_SIZE);
    if (found) {
      StrHelper::strzcpy(_tapTarget.name, found->name, sizeof(_tapTarget.name));
      return;
    }
  }

  _tapTarget.type = static_cast<uint8_t>(TapTargetType::Channel);
  ChannelDetails channel;
  if (getChannel(_tapTarget.channel_idx, channel)) {
    StrHelper::strzcpy(_tapTarget.name, channel.name, sizeof(_tapTarget.name));
  } else if (getChannel(0, channel)) {
    _tapTarget.channel_idx = 0;
    StrHelper::strzcpy(_tapTarget.name, channel.name, sizeof(_tapTarget.name));
  }

  persistTapTargetPrefs();
}

void MyMesh::setTapTargetChannel(uint8_t channel_idx, const ChannelDetails &channel) {
  _tapTarget.version = TapTargetPrefs::kVersion;
  _tapTarget.type = static_cast<uint8_t>(TapTargetType::Channel);
  _tapTarget.channel_idx = channel_idx;
  memset(_tapTarget.contact_pub_key, 0, sizeof(_tapTarget.contact_pub_key));
  StrHelper::strzcpy(_tapTarget.name, channel.name, sizeof(_tapTarget.name));
  persistTapTargetPrefs();
}

void MyMesh::setTapTargetContact(const ContactInfo &contact) {
  _tapTarget.version = TapTargetPrefs::kVersion;
  _tapTarget.type = static_cast<uint8_t>(TapTargetType::Contact);
  _tapTarget.channel_idx = 0;
  memcpy(_tapTarget.contact_pub_key, contact.id.pub_key, sizeof(_tapTarget.contact_pub_key));
  StrHelper::strzcpy(_tapTarget.name, contact.name, sizeof(_tapTarget.name));
  persistTapTargetPrefs();
}

void MyMesh::sendTapStatusToApp(int channel_idx, const char *message) {
  if (!message || !*message) return;
  if (channel_idx >= 0) {
    sendLocalChannelSystemMessage((uint8_t)channel_idx, message);
  } else {
    emitChannelMessageToApp(0, 0, getRTCClock()->getCurrentTimeUnique(), message, 0, "System",
                            TXT_TYPE_CLI_DATA);
  }
}

bool MyMesh::handleTapCommandForContact(const ContactInfo &contact, const char *text) {
  if (!text) return false;
  const char *cmd = trimLeft(text);
  if (strncmp(cmd, "/tap", 4) != 0) {
    return false;
  }

  const char *args = trimLeft(cmd + 4);
  if (*args && matchesToken(args, "here")) {
    setTapTargetContact(contact);
  }

  char response[96];
  describeTapTarget(response, sizeof(response));
  sendTapStatusToApp(-1, response);
  return true;
}

bool MyMesh::resolveTapTarget(TapTargetState &out) {
  if (_tapTarget.type == static_cast<uint8_t>(TapTargetType::Contact)) {
    ContactInfo *found = lookupContactByPubKey(_tapTarget.contact_pub_key, PUB_KEY_SIZE);
    if (found) {
      out.type = TapTargetType::Contact;
      out.contact = *found;
      out.channel_idx = 0;
      return true;
    }
  }

  ChannelDetails channel;
  uint8_t idx = _tapTarget.channel_idx;
  if (!getChannel(idx, channel)) {
    if (!getChannel(0, channel)) {
      return false;
    }
    idx = 0;
  }

  out.type = TapTargetType::Channel;
  out.channel_idx = idx;
  out.channel = channel;

  bool changed = false;
  if (_tapTarget.type != static_cast<uint8_t>(TapTargetType::Channel)) {
    _tapTarget.type = static_cast<uint8_t>(TapTargetType::Channel);
    changed = true;
  }
  if (_tapTarget.channel_idx != idx || strncmp(_tapTarget.name, channel.name, sizeof(_tapTarget.name)) != 0) {
    _tapTarget.channel_idx = idx;
    StrHelper::strzcpy(_tapTarget.name, channel.name, sizeof(_tapTarget.name));
    changed = true;
  }

  if (changed) {
    persistTapTargetPrefs();
  }

  return true;
}

void MyMesh::describeTapTarget(char *dest, size_t len) const {
  if (!dest || len == 0) return;
  TapTargetState state;
  MyMesh *self = const_cast<MyMesh *>(this);
  if (self->resolveTapTarget(state)) {
    const char *name = (state.type == TapTargetType::Channel) ? state.channel.name : state.contact.name;
    const char *kind = (state.type == TapTargetType::Channel) ? "channel" : "device";
    snprintf(dest, len, "Tap target: %s (%s)", (name && *name) ? name : "Unknown", kind);
  } else {
    snprintf(dest, len, "Tap target: unavailable");
  }
}

const char *MyMesh::describeNotifyMode(uint8_t mode) const {
  switch (mode) {
  case NOTIFY_MODE_CW:
    return "cw";
  case NOTIFY_MODE_OFF:
    return "off";
  case NOTIFY_MODE_RTTTL:
  default:
    return "rtttl";
  }
}

void MyMesh::setDefaultBuzzerPrefs() {
  _buzzerPrefs.version = BuzzerPrefs::kVersion;
  _buzzerPrefs.global_override = 0;
  memset(_buzzerPrefs.channel_set, 0, sizeof(_buzzerPrefs.channel_set));
  memset(_buzzerPrefs.channel_mode, 0, sizeof(_buzzerPrefs.channel_mode));
}

void MyMesh::persistBuzzerPrefs() {
  _store->saveBuzzerPrefs(_buzzerPrefs);
}

void MyMesh::loadBuzzerPrefs() {
  setDefaultBuzzerPrefs();
  BuzzerPrefs stored;
  if (_store->loadBuzzerPrefs(stored) && stored.version == BuzzerPrefs::kVersion) {
    _buzzerPrefs = stored;
  }

  bool changed = false;
  _buzzerPrefs.global_override = _buzzerPrefs.global_override ? 1 : 0;
  for (size_t i = 0; i < BuzzerPrefs::kMaxStoredChannels; ++i) {
    _buzzerPrefs.channel_set[i] = _buzzerPrefs.channel_set[i] ? 1 : 0;
    if (_buzzerPrefs.channel_mode[i] > NOTIFY_MODE_OFF) {
      _buzzerPrefs.channel_mode[i] = NOTIFY_MODE_RTTTL;
      _buzzerPrefs.channel_set[i] = 0;
      changed = true;
    }
  }
  if (changed) {
    persistBuzzerPrefs();
  }
}

bool MyMesh::tryParseNotifyModeToken(const char *token, uint8_t &out_mode) const {
  if (!token || !*token) return false;
  char c = (char)tolower((unsigned char)token[0]);
  if (c == 'r') {
    out_mode = NOTIFY_MODE_RTTTL;
    return true;
  }
  if (c == 'c') {
    out_mode = NOTIFY_MODE_CW;
    return true;
  }
  if (c == 'o') {
    out_mode = NOTIFY_MODE_OFF;
    return true;
  }
  return false;
}

bool MyMesh::hasGlobalBuzzerFlag(const char *args) const {
  if (!args) return false;
  const char *cursor = skipWhitespace(args);
  while (*cursor) {
    const char *start = cursor;
    while (*cursor && !isspace((unsigned char)*cursor)) {
      ++cursor;
    }
    size_t len = (size_t)(cursor - start);
    if (len == 8 && strncmp(start, "--global", 8) == 0) {
      return true;
    }
    if (len == 3 && strncmp(start, "--g", 3) == 0) {
      return true;
    }
    cursor = skipWhitespace(cursor);
  }
  return false;
}

bool MyMesh::extractFirstBuzzerToken(const char *args, char *token, size_t token_len) const {
  if (!token || token_len == 0) return false;
  token[0] = 0;
  if (!args) return false;
  const char *cursor = skipWhitespace(args);
  while (*cursor) {
    const char *start = cursor;
    while (*cursor && !isspace((unsigned char)*cursor)) {
      ++cursor;
    }
    size_t len = (size_t)(cursor - start);
    if (len > 0) {
      bool is_flag = false;
      if (len == 3 && strncmp(start, "--g", 3) == 0) {
        is_flag = true;
      } else if (len == 8 && strncmp(start, "--global", 8) == 0) {
        is_flag = true;
      }
      if (!is_flag) {
        size_t copy_len = len < (token_len - 1) ? len : (token_len - 1);
        memcpy(token, start, copy_len);
        token[copy_len] = 0;
        return true;
      }
    }
    cursor = skipWhitespace(cursor);
  }
  return false;
}

uint8_t MyMesh::resolveChannelNotifyMode(uint8_t channel_idx) const {
  if (_buzzerPrefs.global_override) {
    return _prefs.notify_mode;
  }
  if (channel_idx < BuzzerPrefs::kMaxStoredChannels && _buzzerPrefs.channel_set[channel_idx]) {
    return _buzzerPrefs.channel_mode[channel_idx];
  }
  return _prefs.notify_mode;
}

void MyMesh::summarizeBuzzerStatus(uint8_t channel_idx, bool include_all_channels) {
  if (include_all_channels) {
    char response[1024];
    size_t pos = 0;
    pos += snprintf(response + pos, sizeof(response) - pos, "Buzzer all channels, global=%s",
                    describeNotifyMode(_prefs.notify_mode));

    bool any_channel = false;
    for (uint8_t idx = 0; idx < MAX_GROUP_CHANNELS; ++idx) {
      ChannelDetails channel;
      if (!getChannel(idx, channel)) continue;
      bool valid_channel = channel.name[0] != 0;
      if (!valid_channel) {
        for (size_t i = 0; i < sizeof(channel.channel.secret); ++i) {
          if (channel.channel.secret[i] != 0) {
            valid_channel = true;
            break;
          }
        }
      }
      if (!valid_channel) continue;
      any_channel = true;
      const char *local_mode = "default";
      if (idx < BuzzerPrefs::kMaxStoredChannels && _buzzerPrefs.channel_set[idx]) {
        local_mode = describeNotifyMode(_buzzerPrefs.channel_mode[idx]);
      }
      if (pos < sizeof(response) - 1) {
        pos += snprintf(response + pos, sizeof(response) - pos, "\nBuzzer CH%u(%s): %s, global=%s", idx,
                        channel.name, local_mode, describeNotifyMode(_prefs.notify_mode));
        if (pos >= sizeof(response)) {
          response[sizeof(response) - 1] = 0;
          break;
        }
      }
    }
    if (!any_channel) {
      if (pos < sizeof(response) - 1) {
        pos += snprintf(response + pos, sizeof(response) - pos, "\nNo channels configured");
      }
    }
    sendLocalChannelSystemMessage(channel_idx, response);
    return;
  }

  const char *local_mode = "default";
  if (channel_idx < BuzzerPrefs::kMaxStoredChannels && _buzzerPrefs.channel_set[channel_idx]) {
    local_mode = describeNotifyMode(_buzzerPrefs.channel_mode[channel_idx]);
  }
  const char *channel_name = "Unknown";
  ChannelDetails channel;
  if (getChannel(channel_idx, channel) && channel.name[0]) {
    channel_name = channel.name;
  }
  char response[160];
  snprintf(response, sizeof(response),
           "Buzzer CH%u(%s): %s, global=%s\nUsage: /buz rtttl|cw|off [--global|--g]",
           channel_idx, channel_name, local_mode, describeNotifyMode(_prefs.notify_mode));
  sendLocalChannelSystemMessage(channel_idx, response);
}

bool MyMesh::handleLocalPlayCommand(uint8_t channel_idx, const char *text) {
  if (!text || strncmp(text, "/play", 5) != 0) {
    return false;
  }
  const char *args = text + 5;
  args = skipWhitespace(args);

  if (*args == 0) {
    sendLocalChannelSystemMessage(channel_idx, "/play usage: /play <rtttl>");
    return true;
  }

  if (_ui == NULL) {
    sendLocalChannelSystemMessage(channel_idx, "Audio unavailable");
    return true;
  }

  _ui->playRingtone(args);
  sendLocalChannelSystemMessage(channel_idx, "Playing RTTTL locally");
  return true;
}

bool MyMesh::handleLocalBuzzerCommand(uint8_t channel_idx, const char *text) {
  if (!text || strncmp(text, "/buz", 4) != 0) {
    return false;
  }

  const char *args = text + 4;
  args = skipWhitespace(args);

  bool global_flag = hasGlobalBuzzerFlag(args);
  char mode_token[24];
  bool has_mode_token = extractFirstBuzzerToken(args, mode_token, sizeof(mode_token));

  if (!has_mode_token) {
    summarizeBuzzerStatus(channel_idx, global_flag);
    return true;
  }

  uint8_t new_mode = NOTIFY_MODE_RTTTL;
  if (!tryParseNotifyModeToken(mode_token, new_mode)) {
    summarizeBuzzerStatus(channel_idx, global_flag);
    return true;
  }

  if (global_flag) {
    bool changed = (_prefs.notify_mode != new_mode) || (_buzzerPrefs.global_override == 0);
    _prefs.notify_mode = new_mode;
    _buzzerPrefs.global_override = 1;
    savePrefs();
    persistBuzzerPrefs();
    if (_ui) {
      _ui->onNotifyModeChanged(new_mode);
    }

    char response[96];
    if (changed) {
      snprintf(response, sizeof(response), "Global buzzer override set to %s", describeNotifyMode(new_mode));
    } else {
      snprintf(response, sizeof(response), "Global buzzer override already %s", describeNotifyMode(new_mode));
    }
    sendLocalChannelSystemMessage(channel_idx, response);
    return true;
  }

  if (channel_idx >= BuzzerPrefs::kMaxStoredChannels) {
    sendLocalChannelSystemMessage(channel_idx, "Channel buzzer setting unsupported for this channel index");
    return true;
  }

  bool already_same = _buzzerPrefs.channel_set[channel_idx] && _buzzerPrefs.channel_mode[channel_idx] == new_mode;
  _buzzerPrefs.channel_set[channel_idx] = 1;
  _buzzerPrefs.channel_mode[channel_idx] = new_mode;
  persistBuzzerPrefs();

  char response[128];
  if (already_same) {
    snprintf(response, sizeof(response), "Channel buzzer already %s", describeNotifyMode(new_mode));
  } else {
    uint8_t effective_mode = resolveChannelNotifyMode(channel_idx);
    if (_buzzerPrefs.global_override) {
      snprintf(response, sizeof(response), "Channel buzzer set to %s (currently overridden by global=%s)",
               describeNotifyMode(new_mode), describeNotifyMode(_prefs.notify_mode));
    } else {
      snprintf(response, sizeof(response), "Channel buzzer set to %s", describeNotifyMode(effective_mode));
    }
  }
  sendLocalChannelSystemMessage(channel_idx, response);
  return true;
}

bool MyMesh::handleLocalWpmCommand(uint8_t channel_idx, const char *text) {
  if (!text || strncmp(text, "/wpm", 4) != 0) return false;

  const char *args = text + 4;
  args = skipWhitespace(args);

  if (*args == 0) {
    char response[64];
    snprintf(response, sizeof(response), "CW speed: %u WPM (range 5-60)\nUsage: /wpm [5-60]",
             _prefs.cw_wpm ? _prefs.cw_wpm : 15);
    sendLocalChannelSystemMessage(channel_idx, response);
    return true;
  }

  char *end = nullptr;
  long val = strtol(args, &end, 10);
  if (end == args || val < 5 || val > 60) {
    char response[64];
    snprintf(response, sizeof(response), "Invalid WPM. Use a value between 5 and 60.");
    sendLocalChannelSystemMessage(channel_idx, response);
    return true;
  }

  _prefs.cw_wpm = (uint8_t)val;
  savePrefs();

  // Reset CW dedup so the next message rings with the new speed
  _last_cw_group_valid = false;
  memset(_last_cw_group_hash, 0, sizeof(_last_cw_group_hash));

  // Play a short CW test tone at the new speed ("HI" = .... ..)
  if (_ui) {
    char testBuf[256];
    if (buildMorseRtttl("HI", testBuf, sizeof(testBuf), _prefs.cw_wpm)) {
      _ui->playRingtone(testBuf);
    }
  }

  char response[64];
  snprintf(response, sizeof(response), "CW speed set to %u WPM", _prefs.cw_wpm);
  sendLocalChannelSystemMessage(channel_idx, response);
  return true;
}

#ifdef ENABLE_FLIP_MUTE
bool MyMesh::handleLocalFlipmuteCommand(uint8_t channel_idx, const char *text) {
  if (!text || strncmp(text, "/flipmute", 9) != 0) {
    return false;
  }

  const char *args = text + 9;
  args = skipWhitespace(args);

  // Space-delimited: empty -> query.
  if (*args == 0) {
    char response[96];
    snprintf(response, sizeof(response), "Flip to mute: %s\nUsage: /flipmute on|off",
             _prefs.flipmute_enabled ? "enabled" : "disabled");
    sendLocalChannelSystemMessage(channel_idx, response);
    return true;
  }

  // Robust parse: look for first ASCII letter and decide by it.
  char first_letter = 0;
  const char *p = args;
  while (*p) {
    unsigned char ch = (unsigned char)*p;
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
      first_letter = (char)tolower(ch);
      break;
    }
    ++p;
  }

  if (!first_letter) {
    char response[96];
    snprintf(response, sizeof(response), "Flip to mute: %s\nUsage: /flipmute on|off",
             _prefs.flipmute_enabled ? "enabled" : "disabled");
    sendLocalChannelSystemMessage(channel_idx, response);
    return true;
  }

  uint8_t new_state = _prefs.flipmute_enabled;
  if (first_letter == 'o') {
    // Could be 'on' or 'off', check second letter
    if (args[0] == 'o' && args[1] == 'n') {
      new_state = 1;
    } else if (args[0] == 'o' && args[1] == 'f') {
      new_state = 0;
    } else {
      new_state = (first_letter == 'o' && args[0] == 'n') ? 1 : 0;
    }
  } else if (first_letter == 'e') {
    new_state = 1; // enable
  } else if (first_letter == 'd') {
    new_state = 0; // disable
  } else if (first_letter == 'y') {
    new_state = 1; // yes
  } else if (first_letter == 'n') {
    new_state = 0; // no
  } else {
    char response[96];
    snprintf(response, sizeof(response), "Flip to mute: %s\nUsage: /flipmute on|off",
             _prefs.flipmute_enabled ? "enabled" : "disabled");
    sendLocalChannelSystemMessage(channel_idx, response);
    return true;
  }

  if (new_state == _prefs.flipmute_enabled) {
    char response[96];
    snprintf(response, sizeof(response), "Flip-mute already %s", new_state ? "enabled" : "disabled");
    sendLocalChannelSystemMessage(channel_idx, response);
    return true;
  }

  _prefs.flipmute_enabled = new_state;
  savePrefs();

  Serial.printf("[FlipMute] Setting saved: flipmute_enabled=%d\n", _prefs.flipmute_enabled);

  char response[96];
  snprintf(response, sizeof(response), "Flip-mute %s", new_state ? "enabled" : "disabled");
  sendLocalChannelSystemMessage(channel_idx, response);
  return true;
}
#endif

bool MyMesh::handleLocalSensorCommand(uint8_t channel_idx, const char *text) {
  if (!text || strncmp(text, "/sensor", 7) != 0) {
    return false;
  }

  char response[256];
  int offset = 0;

// Get light sensor data (only on T1000-E)
#ifdef T1000_E
  extern uint32_t t1000e_get_light(void);
  extern bool t1000e_read_accel(int8_t &x, int8_t &y, int8_t &z);

  // Single read: light and accel data
  uint32_t light = t1000e_get_light();
  int8_t x, y, z;
  bool accel_ok = t1000e_read_accel(x, y, z);

  offset += snprintf(&response[offset], sizeof(response) - offset, "Light:%lu lux | ", light);

  if (accel_ok) {
    offset += snprintf(&response[offset], sizeof(response) - offset, "Accel X=%d Y=%d Z=%d", (int)x, (int)y,
                       (int)z);
  } else {
    offset += snprintf(&response[offset], sizeof(response) - offset, "Accel:error");
  }
#else
  snprintf(response, sizeof(response), "Sensors not available on this device");
#endif

  sendLocalChannelSystemMessage(channel_idx, response);
  return true;
}

#endif

bool MyMesh::filterRecvFloodPacket(mesh::Packet *packet) {
  // REVISIT: try to determine which Region (from transport_codes[1]) that Sender is indicating for
  // replies/responses
  //    if unknown, fallback to finding Region from transport_codes[0], the 'scope' used by Sender
  return false;
}

bool MyMesh::computePacketHash(const mesh::Packet *pkt, uint8_t out_hash[MAX_HASH_SIZE]) const {
  if (!pkt || !out_hash) return false;
  pkt->calculatePacketHash(out_hash);
  return true;
}

bool MyMesh::isDuplicateGroupMessage(const uint8_t hash[MAX_HASH_SIZE]) {
  if (!hash) return false;
  uint32_t now = _ms->getMillis();
  for (auto &entry : _recent_grp) {
    if (entry.valid && (now - entry.seen_at_ms > kRecentGrpTtlMs)) {
      entry.valid = false; // expire
    }
    if (entry.valid && memcmp(entry.hash, hash, MAX_HASH_SIZE) == 0) {
      return true; // already seen this group packet hash
    }
  }

  RecentGroupEntry &slot = _recent_grp[_recent_grp_idx];
  slot.valid = true;
  slot.seen_at_ms = now;
  memcpy(slot.hash, hash, MAX_HASH_SIZE);
  _recent_grp_idx = (_recent_grp_idx + 1) % kRecentGrpSize;
  return false;
}

bool MyMesh::isRecentDuplicate(const uint8_t hash[MAX_HASH_SIZE]) {
  if (!hash) return false;

  uint32_t now = _ms->getMillis();

  for (auto &entry : _recent_rx) {
    if (entry.valid && (now - entry.seen_at_ms > kRecentRxTtlMs)) {
      entry.valid = false; // expire old entries
    }
    if (entry.valid && memcmp(entry.hash, hash, MAX_HASH_SIZE) == 0) {
      return true; // seen recently; treat as replay/forward copy
    }
  }

  RecentRxEntry &slot = _recent_rx[_recent_rx_idx];
  slot.valid = true;
  slot.seen_at_ms = now;
  memcpy(slot.hash, hash, MAX_HASH_SIZE);
  _recent_rx_idx = (_recent_rx_idx + 1) % kRecentRxSize;
  return false;
}

bool MyMesh::shouldNotifyForPacket(const uint8_t hash[MAX_HASH_SIZE]) {
  if (!hash) return true;
  uint32_t now = _ms->getMillis();

  // Expire old last-notify record after 2 minutes to avoid blocking fresh content.
  if (_last_notify.valid && (now - _last_notify.seen_at_ms > 120000)) {
    _last_notify.valid = false;
  }

  if (_last_notify.valid && memcmp(_last_notify.hash, hash, MAX_HASH_SIZE) == 0) {
    return false; // already notified for this packet hash
  }

  _last_notify.valid = true;
  _last_notify.seen_at_ms = now;
  memcpy(_last_notify.hash, hash, MAX_HASH_SIZE);
  return true;
}

bool MyMesh::allowPacketForward(const mesh::Packet* packet) {
  return _prefs.client_repeat != 0;
}

void MyMesh::sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis) {
  if (scope.isNull()) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);
  } else {
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;  // REVISIT: set to 'home' Region, for sender/return region?
    sendFlood(pkt, codes, delay_millis, _prefs.path_hash_mode + 1);
  }
}

void MyMesh::sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: dynamic send_scope, depending on recipient and current 'home' Region
  if (send_unscoped) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);  // app has explicitly requested un-scoped
  } else {
    TransportKey default_scope;
    memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));

    auto scope = send_scope.isNull() ? &default_scope : &send_scope;
    sendFloodScoped(*scope, pkt, delay_millis);
  }
}
void MyMesh::sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: have per-channel send_scope
  if (send_unscoped) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);  // app has explicitly requested un-scoped
  } else {
    TransportKey default_scope;
    memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));

    auto scope = send_scope.isNull() ? &default_scope : &send_scope;
    sendFloodScoped(*scope, pkt, delay_millis);
  }
}

void MyMesh::onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const char *text) {
  uint8_t pkt_hash[MAX_HASH_SIZE];
  if (!computePacketHash(pkt, pkt_hash)) return;
  if (isRecentDuplicate(pkt_hash)) return;
  if (!shouldNotifyForPacket(pkt_hash)) return;
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_PLAIN, pkt, sender_timestamp, NULL, 0, text);
}

void MyMesh::onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                               const char *text) {
  uint8_t pkt_hash[MAX_HASH_SIZE];
  if (!computePacketHash(pkt, pkt_hash)) return;
  if (isRecentDuplicate(pkt_hash)) return;
  if (!shouldNotifyForPacket(pkt_hash)) return;
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_CLI_DATA, pkt, sender_timestamp, NULL, 0, text);
}

void MyMesh::onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                                 const uint8_t *sender_prefix, const char *text) {
  uint8_t pkt_hash[MAX_HASH_SIZE];
  if (!computePacketHash(pkt, pkt_hash)) return;
  if (isRecentDuplicate(pkt_hash)) return;
  if (!shouldNotifyForPacket(pkt_hash)) return;
  markConnectionActive(from);
  // from.sync_since change needs to be persisted
  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  queueMessage(from, TXT_TYPE_SIGNED_PLAIN, pkt, sender_timestamp, sender_prefix, 4, text);
}

void MyMesh::onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                                  const char *text) {
  uint8_t pkt_hash[MAX_HASH_SIZE];
  if (!computePacketHash(pkt, pkt_hash)) return;
  // Drop duplicates before we decide about ringing to avoid interrupting ongoing CW.
  bool dup_group = isDuplicateGroupMessage(pkt_hash);
  if (dup_group) {
    Serial.printf("[CHAN] drop duplicate group hash=%02X%02X%02X%02X\n", pkt_hash[0], pkt_hash[1],
                  pkt_hash[2], pkt_hash[3]);
    return; // ignore forwarded duplicates by packet hash
  }

  bool recent_dup = isRecentDuplicate(pkt_hash);
  if (recent_dup) {
    Serial.printf("[CHAN] drop recent dup hash=%02X%02X%02X%02X\n", pkt_hash[0], pkt_hash[1], pkt_hash[2],
                  pkt_hash[3]);
    return; // drop further handling for duplicate copies
  }

  int channel_idx_int = findChannelIdx(channel);
  uint8_t channel_idx = channel_idx_int < 0 ? 0xFF : (uint8_t)channel_idx_int;
  uint8_t notify_mode = resolveChannelNotifyMode(channel_idx);

  // Decide whether to ring once for this hash.
  bool should_ring = false;
  uint8_t notify_hash[MAX_HASH_SIZE];
  const uint8_t *ring_hash = pkt_hash;
  if (notify_mode == NOTIFY_MODE_CW) {
    if (computeTextHash(text, notify_hash)) {
      ring_hash = notify_hash; // dedupe by content so forwarded copies share hash
    }
    should_ring = !_last_cw_group_valid || memcmp(_last_cw_group_hash, ring_hash, MAX_HASH_SIZE) != 0;
  } else {
    should_ring = shouldNotifyForPacket(pkt_hash);
  }

  Serial.printf("[CHAN] hash=%02X%02X%02X%02X ringHash=%02X%02X%02X%02X mode=%u ring=%d textLen=%d path=%u\n",
                pkt_hash[0], pkt_hash[1], pkt_hash[2], pkt_hash[3], ring_hash[0], ring_hash[1], ring_hash[2],
                ring_hash[3], notify_mode, should_ring ? 1 : 0, text ? (int)strlen(text) : -1,
                pkt->isRouteFlood() ? pkt->path_len : 0xFF);

  if (should_ring) {
    maybePlayRingtone(NULL, text, notify_mode); // play once per unique hash
    if (notify_mode == NOTIFY_MODE_CW) {
      _last_cw_group_valid = true;
      memcpy(_last_cw_group_hash, ring_hash, MAX_HASH_SIZE);
    }
  } else if (notify_mode != NOTIFY_MODE_CW) {
    Serial.printf("[CHAN] no-ring mode%u hash=%02X%02X%02X%02X\n", notify_mode, pkt_hash[0],
                  pkt_hash[1], pkt_hash[2], pkt_hash[3]);
  }
  uint8_t path_len = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  int8_t snr_quarter = (int8_t)(pkt->getSNR() * 4);
  emitChannelMessageToApp(channel_idx, path_len, timestamp, text, snr_quarter);
}

void MyMesh::onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                               const uint8_t *data, size_t data_len) {
  if (data_len > MAX_CHANNEL_DATA_LENGTH) {
    MESH_DEBUG_PRINTLN("onChannelDataRecv: dropping payload_len=%d exceeds frame limit=%d",
                       (uint32_t)data_len, (uint32_t)MAX_CHANNEL_DATA_LENGTH);
    return;
  }

  int i = 0;
  out_frame[i++] = RESP_CODE_CHANNEL_DATA_RECV;
  out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
  out_frame[i++] = 0; // reserved1
  out_frame[i++] = 0; // reserved2

  uint8_t channel_idx = findChannelIdx(channel);
  out_frame[i++] = channel_idx;
  out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  out_frame[i++] = (uint8_t)(data_type & 0xFF);
  out_frame[i++] = (uint8_t)(data_type >> 8);
  out_frame[i++] = (uint8_t)data_len;

  int copy_len = (int)data_len;
  if (copy_len > 0) {
    memcpy(&out_frame[i], data, copy_len);
    i += copy_len;
  }
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }
}

uint8_t MyMesh::onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                                 uint8_t len, uint8_t *reply) {
  if (data[0] == REQ_TYPE_GET_TELEMETRY_DATA) {
    uint8_t permissions = 0;
    uint8_t cp = contact.flags >> 1; // LSB used as 'favourite' bit (so only use upper bits)

    if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_ALL) {
      permissions = TELEM_PERM_BASE;
    } else if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_FLAGS) {
      permissions = cp & TELEM_PERM_BASE;
    }

    if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_LOCATION;
    } else if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_LOCATION;
    }

    if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_ENVIRONMENT;
    } else if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_ENVIRONMENT;
    }

    uint8_t perm_mask =
        ~(data[1]); // NEW: first reserved byte (of 4), is now inverse mask to apply to permissions
    permissions &= perm_mask;

    if (permissions & TELEM_PERM_BASE) { // only respond if base permission bit is set
      telemetry.reset();
      telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      // query other sensors -- target specific
      sensors.querySensors(permissions, telemetry);

      memcpy(reply, &sender_timestamp,
             4); // reflect sender_timestamp back in response packet (kind of like a 'tag')

      uint8_t tlen = telemetry.getSize();
      memcpy(&reply[4], telemetry.getBuffer(), tlen);
      return 4 + tlen;
    }
  }
  return 0; // unknown
}

void MyMesh::onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) {
  uint32_t tag;
  memcpy(&tag, data, 4);

  if (pending_login && memcmp(&pending_login, contact.id.pub_key, 4) == 0) { // check for login response
    // yes, is response to pending sendLogin()
    pending_login = 0;

    int i = 0;
    if (memcmp(&data[4], "OK", 2) == 0) { // legacy Repeater login OK response
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = 0; // legacy: is_admin = false
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6;                                     // pub_key_prefix
    } else if (data[4] == RESP_SERVER_LOGIN_OK) { // new login response
      uint16_t keep_alive_secs = ((uint16_t)data[5]) * 16;
      if (keep_alive_secs > 0) {
        startConnection(contact, keep_alive_secs);
      }
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = data[6]; // permissions (eg. is_admin)
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
      memcpy(&out_frame[i], &tag, 4);
      i += 4;                    // NEW: include server timestamp
      out_frame[i++] = data[7];  // NEW (v7): ACL permissions
      out_frame[i++] = data[12]; // FIRMWARE_VER_LEVEL
    } else {
      out_frame[i++] = PUSH_CODE_LOGIN_FAIL;
      out_frame[i++] = 0; // reserved
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
    }
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && // check for status response
             pending_status &&
             memcmp(&pending_status, contact.id.pub_key, 4) == 0 // legacy matching scheme
                                                                 // FUTURE: tag == pending_status
  ) {
    pending_status = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_STATUS_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && tag == pending_telemetry) { // check for matching response tag
    pending_telemetry = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && tag == pending_req) { // check for matching response tag
    pending_req = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_BINARY_RESPONSE;
    out_frame[i++] = 0;             // reserved
    memcpy(&out_frame[i], &tag, 4); // app needs to match this to RESP_CODE_SENT.tag
    i += 4;
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  }
}

bool MyMesh::onContactPathRecv(ContactInfo &contact, uint8_t *in_path, uint8_t in_path_len, uint8_t *out_path,
                               uint8_t out_path_len, uint8_t extra_type, uint8_t *extra, uint8_t extra_len) {
  if (extra_type == PAYLOAD_TYPE_RESPONSE && extra_len > 4) {
    uint32_t tag;
    memcpy(&tag, extra, 4);

    if (tag == pending_discovery) { // check for matching response tag)
      pending_discovery = 0;

      if (!mesh::Packet::isValidPathLen(in_path_len) || !mesh::Packet::isValidPathLen(out_path_len)) {
        MESH_DEBUG_PRINTLN("onContactPathRecv, invalid path sizes: %d, %d", in_path_len, out_path_len);
      } else {
        int i = 0;
        out_frame[i++] = PUSH_CODE_PATH_DISCOVERY_RESPONSE;
        out_frame[i++] = 0; // reserved
        memcpy(&out_frame[i], contact.id.pub_key, 6);
        i += 6; // pub_key_prefix
        out_frame[i++] = out_path_len;
        i += mesh::Packet::writePath(&out_frame[i], out_path, out_path_len);
        out_frame[i++] = in_path_len;
        i += mesh::Packet::writePath(&out_frame[i], in_path, in_path_len);
        // NOTE: telemetry data in 'extra' is discarded at present

        _serial->writeFrame(out_frame, i);
      }
      return false; // DON'T send reciprocal path!
    }
  }
  // let base class handle received path and data
  return BaseChatMesh::onContactPathRecv(contact, in_path, in_path_len, out_path, out_path_len, extra_type,
                                         extra, extra_len);
}

void MyMesh::onControlDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_CONTROL_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = packet->path_len;
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), data received while app offline");
  }
}

void MyMesh::onRawDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_RAW_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = 0xFF; // reserved (possibly path_len in future)
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), data received while app offline");
  }
}

void MyMesh::loadHeadlessCannedMessages() {
  size_t count = 0;
  if (_store->loadCannedMessages(_cannedMessages, canned::kMaxMessages, count) && count > 0) {
    _cannedMessageCount = count;
    return;
  }

  _cannedMessageCount = canned::kDefaultMessageCount;
  for (size_t i = 0; i < _cannedMessageCount; ++i) {
    strncpy(_cannedMessages[i], canned::kDefaultMessages[i], canned::kMaxMessageLen);
    _cannedMessages[i][canned::kMaxMessageLen - 1] = 0;
  }
}

void MyMesh::persistHeadlessCannedMessages() {
  _store->saveCannedMessages(_cannedMessages, _cannedMessageCount);
}

static inline const char *skipWhitespace(const char *ptr) {
  while (ptr && *ptr && isspace((unsigned char)*ptr)) {
    ++ptr;
  }
  return ptr;
}

bool MyMesh::parseCannedMessageList(const char *input, char dest[][canned::kMaxMessageLen], size_t &count) {
  count = 0;
  if (!input) return false;

  const char *cursor = input;
  cursor = skipWhitespace(cursor);
  while (*cursor && count < canned::kMaxMessages) {
    if (*cursor == '|') {
      ++cursor;
      continue;
    }

    const char *start = cursor;
    while (*cursor && *cursor != '|') {
      ++cursor;
    }
    const char *end = cursor;
    while (end > start && isspace((unsigned char)*(end - 1))) {
      --end;
    }
    while (start < end && isspace((unsigned char)*start)) {
      ++start;
    }
    size_t len = end > start ? (size_t)(end - start) : 0;
    if (len > 0) {
      size_t copy_len = len < canned::kMaxMessageLen - 1 ? len : canned::kMaxMessageLen - 1;
      memcpy(dest[count], start, copy_len);
      dest[count][copy_len] = 0;
      count++;
    }
  }
  return count > 0;
}

void MyMesh::summarizeCannedMessages(uint8_t channel_idx) {
  if (_cannedMessageCount == 0) return;

  char summary[256];
  size_t pos = snprintf(summary, sizeof(summary), "/msg ");
  for (size_t i = 0; i < _cannedMessageCount && pos < sizeof(summary) - 1; ++i) {
    if (i > 0 && pos < sizeof(summary) - 1) {
      summary[pos++] = '|';
    }
    size_t remaining = sizeof(summary) - pos - 1;
    size_t msg_len = strnlen(_cannedMessages[i], canned::kMaxMessageLen - 1);
    if (msg_len > remaining) {
      msg_len = remaining;
    }
    memcpy(&summary[pos], _cannedMessages[i], msg_len);
    pos += msg_len;
  }
  summary[pos] = 0;
  emitChannelMessageToApp(channel_idx, 0xFF, getRTCClock()->getCurrentTimeUnique(), summary, 0, "System",
                          TXT_TYPE_CLI_DATA);
}

// Local command registry for easier management/dumping.
enum class LocalCommand : uint8_t { Msg = 0, Buz, Ringtone, Play, Tap, Help, Count };

static constexpr const char *kLocalCommandNames[] = {
  "/msg", "/buz", "/rtttl", "/play", "/tap", "/help",
};

static_assert(static_cast<size_t>(LocalCommand::Count) ==
                  (sizeof(kLocalCommandNames) / sizeof(kLocalCommandNames[0])),
              "LocalCommand enum and name table must stay in sync");

bool MyMesh::handleLocalChannelCommand(uint8_t channel_idx, const char *text, const ChannelDetails &channel) {
  (void)channel;
  if (text == NULL) return false;
  // Trim leading spaces so commands like "  /msg" still match, and allow trailing
  // spaces to be ignored by downstream parsers.
  const char *cmd = skipWhitespace(text);
#ifdef HEADLESS_CANNED_MESSAGES
  if (handleLocalBuzzerCommand(channel_idx, cmd)) return true;
  if (handleLocalWpmCommand(channel_idx, cmd)) return true;
#ifdef ENABLE_FLIP_MUTE
  if (handleLocalFlipmuteCommand(channel_idx, cmd)) return true;
#endif
  if (handleLocalSensorCommand(channel_idx, cmd)) return true;
  if (handleLocalPlayCommand(channel_idx, cmd)) return true;
  if (strncmp(cmd, kLocalCommandNames[static_cast<size_t>(LocalCommand::Tap)], 4) == 0) {
    const char *args = skipWhitespace(cmd + 4);
    if (*args && matchesToken(args, "here")) {
      setTapTargetChannel(channel_idx, channel);
    }
    char response[96];
    describeTapTarget(response, sizeof(response));
    sendTapStatusToApp(channel_idx, response);
    return true;
  }
  // /lobat: low-battery auto-notify
  if (strncmp(cmd, "/lobat", 6) == 0) {
    const char *args = skipWhitespace(cmd + 6);
    if (*args && matchesToken(args, "here")) {
      _lobat_channel_idx = channel_idx;
      initLobat();
    } else if (*args) {
      // /lobat <pct> — set threshold
      int pct = atoi(args);
      if (pct >= 1 && pct <= 100) {
        _lobat_threshold_pct = (uint8_t)pct;
        _store->saveLobatPrefs(_lobat_threshold_pct);
      }
    }
    char response[128];
    snprintf(response, sizeof(response), "Low-battery alert: %s", describeLobatStatus());
    sendLocalChannelSystemMessage(channel_idx, response);
    return true;
  }
  // /mark: geofence
  if (strncmp(cmd, "/mark", 5) == 0) {
    const char *args = skipWhitespace(cmd + 5);
    if (*args == 0) {
      // No args: show status + ASCII map
      char response[128];
      snprintf(response, sizeof(response), "Geofence: %s", describeMarkStatus());
      sendLocalChannelSystemMessage(channel_idx, response);
      // Also render the ASCII map if GPS has a fix
      if (sensors.node_lat != 0.0 || sensors.node_lon != 0.0) {
        char map_buf[512];
        buildMarkMap(sensors.node_lat, sensors.node_lon, map_buf, sizeof(map_buf));
        sendLocalChannelSystemMessage(channel_idx, map_buf);
      }
      return true;
    }
    if (matchesToken(args, "here")) {
      setMarkChannel(channel_idx);
      char response[96];
      snprintf(response, sizeof(response), "Geofence alerts will go to this channel (%s)",
               channel.name[0] ? channel.name : "ch" + channel_idx);
      sendLocalChannelSystemMessage(channel_idx, response);
      return true;
    }
    if (matchesToken(args, "on")) {
      // Enable fence (starts GPS if needed)
      setMarkEnabled(true);
      char response[96];
      snprintf(response, sizeof(response), "Geofence: %s", describeMarkStatus());
      sendLocalChannelSystemMessage(channel_idx, response);
      return true;
    }
    if (matchesToken(args, "off")) {
      setMarkEnabled(false);
      sendLocalChannelSystemMessage(channel_idx, "Geofence disabled");
      return true;
    }
    if (matchesToken(args, "clear")) {
      clearMarkPoints();
      sendLocalChannelSystemMessage(channel_idx, "Geofence points cleared");
      return true;
    }
    // Try to parse as a coordinate pair "lat,lon" or "lat, lon"
    {
      double lat, lon;
      // Flexible parse: skip whitespace, allow ASCII or fullwidth comma
      const char *p = args;
      while (*p == ' ' || *p == '	') p++;
      char *end = NULL;
      lat = strtod(p, &end);
      if (end != p && *end != 0) {
        p = end;
        while (*p == ' ' || *p == '	') p++;
        // Accept ASCII comma, Chinese comma (0xEF 0xBC 0x8C), or semicolon
        if (*p == ',' || *p == ';') {
          p++;
        } else if ((uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBC && (uint8_t)p[2] == 0x8C) {
          p += 3;
        } else {
          p = NULL;
        }
        if (p) {
          while (*p == ' ' || *p == '	') p++;
          lon = strtod(p, &end);
          if (end != p) {
            if (addMarkPoint(lat, lon)) {
              char response[96];
              snprintf(response, sizeof(response), "Mark point %u added: %.4f, %.4f (total %u)",
                       (unsigned)_mark_point_count, lat, lon, (unsigned)_mark_point_count);
              sendLocalChannelSystemMessage(channel_idx, response);
            } else {
              char response[64];
              snprintf(response, sizeof(response), "Mark full or invalid (max %u points)", (unsigned)kMarkMaxPoints);
              sendLocalChannelSystemMessage(channel_idx, response);
            }
            return true;
          }
        }
      }
    }
    sendLocalChannelSystemMessage(channel_idx, "Usage: /mark <lat,lon> | on | off | clear");
    return true;
  }
  // /help: list supported commands
  if (strncmp(cmd, "/help", 5) == 0) {
    const char *help =
      "Basic:\n"
      "/msg /buz /wpm /rtttl\n"
      "Advanced:\n"
      "/tap /lobat /mark /flipmute\n"
      "/mark <lat,lon>|on|off|clr";
    sendLocalChannelSystemMessage(channel_idx, help);
    return true;
  }
#endif

  // Only handle /msg here; other prefixes are handled above.
  if (strncmp(cmd, kLocalCommandNames[static_cast<size_t>(LocalCommand::Msg)], 4) != 0) return false;

  const char *args = cmd + 4;
  // Treat any text starting with "/msg" as a command. If the next char isn't space
  // or terminator, return usage instead of sending as a normal message.
  if (*args && !isspace((unsigned char)*args)) {
    emitChannelMessageToApp(channel_idx, 0xFF, getRTCClock()->getCurrentTimeUnique(),
                            "Usage: /msg <item1|item2|...>", 0, "System", TXT_TYPE_CLI_DATA);
    return true;
  }

  args = skipWhitespace(args);

  if (*args == 0) {
    summarizeCannedMessages(channel_idx);
    return true;
  }

  char updated[canned::kMaxMessages][canned::kMaxMessageLen];
  size_t count = 0;
  if (!parseCannedMessageList(args, updated, count)) {
    emitChannelMessageToApp(channel_idx, 0xFF, getRTCClock()->getCurrentTimeUnique(),
                            "/msg error: no entries", 0, "System", TXT_TYPE_CLI_DATA);
    return true;
  }

  _cannedMessageCount = count;
  for (size_t i = 0; i < count; ++i) {
    strncpy(_cannedMessages[i], updated[i], canned::kMaxMessageLen);
    _cannedMessages[i][canned::kMaxMessageLen - 1] = 0;
  }
  persistHeadlessCannedMessages();
  summarizeCannedMessages(channel_idx);
  return true;
}

#ifdef HEADLESS_CANNED_MESSAGES

void MyMesh::clearAllRingtones() {
  _globalRingtone[0] = 0;
  _ringtoneRepeatCount = 1; // Reset to default
  for (size_t i = 0; i < ringtone_cfg::kMaxDeviceEntries; ++i) {
    _deviceRingtones[i].in_use = false;
    memset(_deviceRingtones[i].pub_key, 0, sizeof(_deviceRingtones[i].pub_key));
    _deviceRingtones[i].tone[0] = 0;
    _deviceRingtones[i].updated_at = 0;
  }
}

void MyMesh::loadHeadlessRingtones() {
  clearAllRingtones();
  uint8_t *blob = ringtoneScratch();
  const size_t scratch_len = ringtone_cfg::kBlobMaxLen;
  size_t len = 0;
  if (!_store->loadRingtoneBlob(blob, scratch_len, len) || len < 2) {
    return;
  }
  const uint8_t *ptr = blob;
  size_t remaining = len;
  uint8_t version = *ptr++;
  remaining--;
  if (version != ringtone_cfg::kBlobVersion) {
    return;
  }
  if (remaining == 0) return;
  uint8_t globalLen = *ptr++;
  remaining--;
  if (globalLen > remaining) return;
  size_t copy_len = globalLen;
  if (copy_len >= sizeof(_globalRingtone)) {
    copy_len = sizeof(_globalRingtone) - 1;
  }
  if (copy_len > 0) {
    memcpy(_globalRingtone, ptr, copy_len);
  }
  _globalRingtone[copy_len] = 0;
  ptr += globalLen;
  remaining -= globalLen;
  if (remaining == 0) return;
  uint8_t entryCount = *ptr++;
  remaining--;
  for (uint8_t idx = 0; idx < entryCount; ++idx) {
    if (remaining < PUB_KEY_SIZE + 5) break;
    DeviceRingtone temp;
    temp.in_use = true;
    memcpy(temp.pub_key, ptr, PUB_KEY_SIZE);
    ptr += PUB_KEY_SIZE;
    remaining -= PUB_KEY_SIZE;
    memcpy(&temp.updated_at, ptr, 4);
    ptr += 4;
    remaining -= 4;
    if (remaining == 0) break;
    uint8_t toneLen = *ptr++;
    remaining--;
    if (toneLen > remaining) break;
    size_t tone_copy = toneLen;
    if (tone_copy >= sizeof(temp.tone)) {
      tone_copy = sizeof(temp.tone) - 1;
    }
    if (tone_copy > 0) {
      memcpy(temp.tone, ptr, tone_copy);
    }
    temp.tone[tone_copy] = 0;
    ptr += toneLen;
    remaining -= toneLen;
    if (idx < ringtone_cfg::kMaxDeviceEntries) {
      _deviceRingtones[idx] = temp;
    }
  }
}

void MyMesh::persistHeadlessRingtones() {
  uint8_t *blob = ringtoneScratch();
  const size_t scratch_len = ringtone_cfg::kBlobMaxLen;
  size_t pos = 0;
  blob[pos++] = ringtone_cfg::kBlobVersion;
  size_t globalLen = strnlen(_globalRingtone, sizeof(_globalRingtone) - 1);
  if (pos + 1 + globalLen > scratch_len) {
    return;
  }
  blob[pos++] = (uint8_t)globalLen;
  if (globalLen > 0) {
    memcpy(&blob[pos], _globalRingtone, globalLen);
    pos += globalLen;
  }
  uint8_t entryCount = 0;
  for (size_t i = 0; i < ringtone_cfg::kMaxDeviceEntries; ++i) {
    if (_deviceRingtones[i].in_use && _deviceRingtones[i].tone[0]) {
      entryCount++;
    }
  }
  if (pos + 1 > scratch_len) {
    return;
  }
  blob[pos++] = entryCount;
  for (size_t i = 0; i < ringtone_cfg::kMaxDeviceEntries; ++i) {
    if (!_deviceRingtones[i].in_use || !_deviceRingtones[i].tone[0]) continue;
    size_t toneLen = strnlen(_deviceRingtones[i].tone, sizeof(_deviceRingtones[i].tone) - 1);
    if (pos + PUB_KEY_SIZE + 5 + toneLen > scratch_len) {
      break;
    }
    memcpy(&blob[pos], _deviceRingtones[i].pub_key, PUB_KEY_SIZE);
    pos += PUB_KEY_SIZE;
    memcpy(&blob[pos], &_deviceRingtones[i].updated_at, 4);
    pos += 4;
    blob[pos++] = (uint8_t)toneLen;
    if (toneLen > 0) {
      memcpy(&blob[pos], _deviceRingtones[i].tone, toneLen);
      pos += toneLen;
    }
  }
  _store->saveRingtoneBlob(blob, pos);
}

MyMesh::DeviceRingtone *MyMesh::findRingtoneEntry(const uint8_t *pub_key) {
  if (!pub_key) return NULL;
  for (size_t i = 0; i < ringtone_cfg::kMaxDeviceEntries; ++i) {
    if (_deviceRingtones[i].in_use && memcmp(_deviceRingtones[i].pub_key, pub_key, PUB_KEY_SIZE) == 0) {
      return &_deviceRingtones[i];
    }
  }
  return NULL;
}

MyMesh::DeviceRingtone *MyMesh::allocateRingtoneEntry(const uint8_t *pub_key) {
  if (!pub_key) return NULL;
  DeviceRingtone *existing = findRingtoneEntry(pub_key);
  if (existing) return existing;
  for (size_t i = 0; i < ringtone_cfg::kMaxDeviceEntries; ++i) {
    if (!_deviceRingtones[i].in_use) {
      _deviceRingtones[i].in_use = true;
      memcpy(_deviceRingtones[i].pub_key, pub_key, PUB_KEY_SIZE);
      _deviceRingtones[i].tone[0] = 0;
      _deviceRingtones[i].updated_at = 0;
      return &_deviceRingtones[i];
    }
  }
  size_t oldest_idx = 0;
  uint32_t oldest_time = _deviceRingtones[0].updated_at;
  for (size_t i = 1; i < ringtone_cfg::kMaxDeviceEntries; ++i) {
    if (_deviceRingtones[i].updated_at < oldest_time) {
      oldest_time = _deviceRingtones[i].updated_at;
      oldest_idx = i;
    }
  }
  _deviceRingtones[oldest_idx].in_use = true;
  memcpy(_deviceRingtones[oldest_idx].pub_key, pub_key, PUB_KEY_SIZE);
  _deviceRingtones[oldest_idx].tone[0] = 0;
  _deviceRingtones[oldest_idx].updated_at = 0;
  return &_deviceRingtones[oldest_idx];
}

const char *MyMesh::getRingtoneFor(const ContactInfo *from) const {
  if (from) {
    for (size_t i = 0; i < ringtone_cfg::kMaxDeviceEntries; ++i) {
      if (_deviceRingtones[i].in_use && _deviceRingtones[i].tone[0] &&
          memcmp(_deviceRingtones[i].pub_key, from->id.pub_key, PUB_KEY_SIZE) == 0) {
        return _deviceRingtones[i].tone;
      }
    }
  }
  if (_globalRingtone[0]) {
    return _globalRingtone;
  }
  return NULL;
}

void MyMesh::maybePlayRingtone(const ContactInfo *from, const char *text, uint8_t mode_override) {
  if (_ui == NULL) return;
  uint8_t mode = (mode_override <= NOTIFY_MODE_OFF) ? mode_override : _prefs.notify_mode;
#ifdef MESH_DEBUG
#ifdef ENABLE_FLIP_MUTE
  Serial.printf("[Ringtone] maybePlayRingtone called - mode=%d flipmute_enabled=%d\n", mode, _prefs.flipmute_enabled);
#else
  Serial.printf("[Ringtone] maybePlayRingtone called - mode=%d\n", mode);
#endif
#endif
  if (mode == NOTIFY_MODE_OFF) {
    return;
  }

#ifdef ENABLE_FLIP_MUTE
  // Check FlipMute: if enabled and device is face-down in dark, don't play sound
  if (_prefs.flipmute_enabled) {
#ifdef T1000_E
    extern uint32_t t1000e_get_light(void);
    extern bool t1000e_read_accel(int8_t &x, int8_t &y, int8_t &z);

    uint32_t light = t1000e_get_light();

    extern bool t1000e_is_face_down_in_dark(uint32_t light_threshold_lux);
    if (t1000e_is_face_down_in_dark(5)) {
#ifdef MESH_DEBUG
      Serial.println("[FlipMute] Device face-down and dark in maybePlayRingtone, suppressing sound");
#endif
      return;
    }
#else
    Serial.println("[Ringtone] T1000_E not defined, skipping FlipMute check");
#endif
  } else {
    Serial.printf("[Ringtone] FlipMute is disabled (flipmute_enabled=%d)\n", _prefs.flipmute_enabled);
  }
#endif

  if (mode == NOTIFY_MODE_CW) {
    if (!text || !text[0]) return;

    // 如果消息是「设备名/ID: 内容」这种格式，去掉前面的前缀，只保留真正的内容。
    const char *payload = text;
    if (from && from->name[0]) {
      size_t name_len = strlen(from->name);
      if (strncmp(payload, from->name, name_len) == 0) {
        const char *p = payload + name_len;
        if (*p == ':' && p[1] == ' ') {
          payload = p + 2;
        } else if (*p == ':' || *p == ' ') {
          payload = p + 1;
        }
      }
    }

    if (payload) {
      const char *colon = strchr(payload, ':');
      if (colon) {
        bool has_space = false;
        for (const char *q = payload; q < colon; ++q) {
          if (*q == ' ') {
            has_space = true;
            break;
          }
        }
        if (!has_space) {
          const char *p = colon;
          if (p[1] == ' ') {
            payload = p + 2;
          } else {
            payload = p + 1;
          }
        }
      }
    }

    if (!payload || !payload[0]) return;

    // Larger buffer reduces premature truncation for long CW strings.
    char cwBuffer[1536];
    bool has_morse = buildMorseRtttl(payload, cwBuffer, sizeof(cwBuffer), _prefs.cw_wpm ? _prefs.cw_wpm : 15);
    if (!has_morse) {
      // If the text cannot be converted to Morse (e.g. only non-ASCII chars),
      // still emit a short alert so DMs are not silent in CW mode.
      const char *fallback = getRingtoneFor(from);
      if (!fallback || !fallback[0]) {
        fallback = "d=8,o=5,b=400:4c,4p,4c"; // brief double beep fallback
      }
      _ui->playRingtone(fallback);
      return;
    }
    clampRtttl(cwBuffer, sizeof(cwBuffer));
    _ui->playRingtone(cwBuffer);
  } else {
    // RTTTL mode: play ringtone with repeat count
    const char *tone = getRingtoneFor(from);
    if (tone && tone[0]) {
      // Extract repeat count from tone string if it ends with "xN"
      uint8_t repeat = 1; // default
      size_t tone_len = strlen(tone);

      // Check for "xN" at the end (e.g., "...16c7x3" or "...16c7 x3")
      if (tone_len >= 2) {
        // Try "xN" without space (e.g., "16c7x3")
        char x_char = tone[tone_len - 2];
        char digit_char = tone[tone_len - 1];

        if (x_char == 'x' && isdigit((unsigned char)digit_char)) {
          int digit = digit_char - '0';
          if (digit >= 1 && digit <= 9) {
            repeat = (uint8_t)digit;
          }
        }
      }

      Serial.printf("[Ringtone] RTTTL playback with repeat count: %d (from tone='%s')\n", repeat, tone);

      // Create a copy without the xN suffix for actual playback
      char tone_to_play[ringtone_cfg::kMaxToneLen];
      strncpy(tone_to_play, tone, sizeof(tone_to_play));
      tone_to_play[sizeof(tone_to_play) - 1] = 0;

      // Remove xN suffix if present
      size_t play_len = strlen(tone_to_play);
      if (play_len >= 2) {
        if (tone_to_play[play_len - 2] == 'x' && isdigit((unsigned char)tone_to_play[play_len - 1])) {
          tone_to_play[play_len - 2] = '\0';
        }
      }

      for (uint8_t i = 0; i < repeat; i++) {
        Serial.printf("[Ringtone] Playing repeat %d/%d\n", i + 1, repeat);
        _ui->playRingtone(tone_to_play);

        // Wait for playback to finish
        // IMPORTANT: We must pump the buzzer loop while waiting, otherwise playback stalls!
        uint32_t timeout = millis() + 5000; // Max wait 5s per repetition
        // Wait until buzzer starts playing (it might take a moment)
        delay(10);
        while (_ui->isBuzzerPlaying() && millis() < timeout) {
          _ui->pollBuzzer(); // CRITICAL: Keep the buzzer updating!
          _ui->pollInput();  // Update button state

          // Check for button press to stop playback
          if (_ui->isButtonPressed()) {
            Serial.println("[Ringtone] Button pressed, stopping playback");
            _ui->stopBuzzer();
            return; // Exit function immediately
          }

          delay(5); // Short delay to yield
        }

        // Gap between repeats (interruptible)
        if (i < repeat - 1) {
          uint32_t gap_start = millis();
          while (millis() - gap_start < 1000) {
            _ui->pollInput(); // Update button state
            if (_ui->isButtonPressed()) {
              Serial.println("[Ringtone] Button pressed during gap, stopping playback");
              _ui->stopBuzzer();
              return;
            }
            delay(50);
          }
        }
      }
    } else {
      // Ensure we still emit an audible cue even when no RTTTL is configured.
      _ui->playRingtone("d=8,o=5,b=400:4c,4p,4c");
    }
  }
}

void MyMesh::sendRingtoneChannelReply(int channel_idx, const char *text) {
  if (channel_idx < 0 || text == NULL || !text[0]) return;
  ChannelDetails channel;
  if (!getChannel(channel_idx, channel)) return;
  uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  if (sendGroupMessage(timestamp, channel.channel, _prefs.node_name, text, strlen(text))) {
    logLocalChannelMessage((uint8_t)channel_idx, text);
  }
}

void MyMesh::summarizeRingtoneStatus(const ContactInfo *from, int channel_idx, bool local_only) {
  (void)from;
  char response[256];
  if (_globalRingtone[0]) {
    snprintf(response, sizeof(response), "Ringtone: %s", _globalRingtone);
  } else {
    snprintf(response, sizeof(response), "Ringtone not set");
  }
  if (local_only) {
    if (channel_idx >= 0) {
      sendLocalChannelSystemMessage((uint8_t)channel_idx, response);
    }
  } else {
    sendRingtoneChannelReply(channel_idx, response);
  }
}

bool MyMesh::handleIncomingRingtoneCommand(const ContactInfo *from, int channel_idx,
                                           const mesh::GroupChannel *channel, const char *text,
                                           bool local_only) {
  (void)channel;
  if (from) {
    return false; // ignore private /rtttl commands
  }
  if (!text) return false;
  const char *cmd = trimLeft(text);
  int cmd_len = 0;
  if (strncmp(cmd, "/rtttl", 6) == 0) {
    cmd_len = 6;
  } else if (strncmp(cmd, "/ringtone", 9) == 0) { // legacy alias
    cmd_len = 9;
  } else {
    return false;
  }

  auto reply = [&](const char *message) {
    if (!message) return;
    if (channel_idx >= 0) {
      if (local_only) {
        sendLocalChannelSystemMessage((uint8_t)channel_idx, message);
      } else {
        sendRingtoneChannelReply(channel_idx, message);
      }
    }
  };

  // Space-delimited parsing; empty -> status query.
  const char *args = trimLeft(cmd + cmd_len);
  if (*args == 0 || matchesToken(args, "list")) {
    summarizeRingtoneStatus(from, channel_idx, local_only);
    return true;
  }
  if (matchesToken(args, "reset")) {
    clearAllRingtones();
    persistHeadlessRingtones();
    reply("Ringtone settings cleared");
    return true;
  }
  char tone[ringtone_cfg::kMaxToneLen];
  bool truncated = false;
  size_t actual_len = copyTrimmed(args, tone, sizeof(tone), truncated);
  // If payload is only whitespace, treat as status query instead of update.
  if (actual_len == 0) {
    summarizeRingtoneStatus(from, channel_idx, local_only);
    return true;
  }

  // Require at least one meaningful character (letter/digit/':') in payload; otherwise treat as status.
  bool has_meaningful = false;
  for (size_t i = 0; i < strlen(tone); ++i) {
    unsigned char c = (unsigned char)tone[i];
    if (isalnum(c) || c == ':') {
      has_meaningful = true;
      break;
    }
  }
  if (!has_meaningful) {
    summarizeRingtoneStatus(from, channel_idx, local_only);
    return true;
  }

  if (channel_idx < 0) {
    return true; // unable to determine scope
  }

  // Parse repeat count suffix: "xN" where N is 1-9
  // e.g., "...16c7 x3" or "...16c7x3" -> play 3 times
  // IMPORTANT: We keep the full tone string INCLUDING the xN suffix for storage,
  // so that maybePlayRingtone() can dynamically extract the repeat count on each playback
  uint8_t repeat_count = 1; // default (for confirmation message only)
  size_t tone_len = strlen(tone);

  // Look for "xN" pattern at the end (with or without space before x)
  if (tone_len >= 2) {
    // Check for " xN" format (with space)
    int space_idx = -1;
    for (int p = tone_len - 1; p >= 0; --p) {
      if (tone[p] == ' ') {
        space_idx = p;
        break;
      }
    }

    // Try " xN" pattern first (space + x + digit)
    if (space_idx >= 0 && (tone_len - space_idx) == 3) {
      char x_char = tone[space_idx + 1];
      char digit_char = tone[space_idx + 2];

      if (x_char == 'x' && isdigit((unsigned char)digit_char)) {
        int digit = digit_char - '0';
        if (digit >= 1 && digit <= 9) {
          repeat_count = (uint8_t)digit;
          // Remove the space before x for cleaner storage (e.g., "16c7x3" not "16c7 x3")
          // Copy " xN" to remove the space
          memmove(&tone[space_idx], &tone[space_idx + 1], strlen(&tone[space_idx + 1]) + 1);
        }
      }
    } else if (tone_len >= 2) {
      // Try "xN" pattern without space (e.g., "16c7x3")
      char x_char = tone[tone_len - 2];
      char digit_char = tone[tone_len - 1];

      if (x_char == 'x' && isdigit((unsigned char)digit_char)) {
        int digit = digit_char - '0';
        if (digit >= 1 && digit <= 9) {
          repeat_count = (uint8_t)digit;
        }
      }
    }
  }

  // Save the FULL tone string INCLUDING xN suffix
  strncpy(_globalRingtone, tone, sizeof(_globalRingtone));
  _globalRingtone[sizeof(_globalRingtone) - 1] = 0;
  _ringtoneRepeatCount = repeat_count;

  Serial.printf("[RingtoneCmd] Tone set (repeat_count=%d, stored_tone='%s')\n", repeat_count,
                _globalRingtone);

  persistHeadlessRingtones();

  char response[96];
  if (repeat_count > 1) {
    snprintf(response, sizeof(response), "Global ringtone updated (repeat %d times)%s", repeat_count,
             truncated ? " (truncated)" : "");
  } else {
    snprintf(response, sizeof(response), "Global ringtone updated%s", truncated ? " (truncated)" : "");
  }
  reply(response);
  return true;
}
#endif

void MyMesh::onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                         const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) {
  uint8_t path_sz = flags & 0x03;  // NEW v1.11+
  if (12 + path_len + (path_len >> path_sz) + 1 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onTraceRecv(), path_len is too long: %d", (uint32_t)path_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_TRACE_DATA;
  out_frame[i++] = 0; // reserved
  out_frame[i++] = path_len;
  out_frame[i++] = flags;
  memcpy(&out_frame[i], &tag, 4);
  i += 4;
  memcpy(&out_frame[i], &auth_code, 4);
  i += 4;
  memcpy(&out_frame[i], path_hashes, path_len);
  i += path_len;

  memcpy(&out_frame[i], path_snrs, path_len >> path_sz);
  i += path_len >> path_sz;
  out_frame[i++] = (int8_t)(packet->getSNR() * 4); // extra/final SNR (to this node)

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onTraceRecv(), data received while app offline");
  }
}

uint32_t MyMesh::calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const {
  return SEND_TIMEOUT_BASE_MILLIS + (FLOOD_SEND_TIMEOUT_FACTOR * pkt_airtime_millis);
}
uint32_t MyMesh::calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const {
  uint8_t path_hash_count = path_len & 63;
  return SEND_TIMEOUT_BASE_MILLIS +
         ((pkt_airtime_millis * DIRECT_SEND_PERHOP_FACTOR + DIRECT_SEND_PERHOP_EXTRA_MILLIS) *
          (path_hash_count + 1));
}

void MyMesh::onSendTimeout() {}

MyMesh::MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables,
               DataStore &store, AbstractUITask *ui)
    : BaseChatMesh(radio, *new ArduinoMillis(), rng, rtc, *new StaticPoolPacketManager(16), tables),
      _serial(NULL), telemetry(MAX_PACKET_PAYLOAD - 4), _store(&store), _ui(ui), _iter(0) {
  _iter_started = false;
  _cli_rescue = false;
  _recent_grp_idx = 0;
  for (auto &entry : _recent_grp) {
    entry.valid = false;
    entry.seen_at_ms = 0;
    memset(entry.hash, 0, sizeof(entry.hash));
  }
  _recent_rx_idx = 0;
  for (auto &entry : _recent_rx) {
    entry.valid = false;
    entry.seen_at_ms = 0;
    memset(entry.hash, 0, sizeof(entry.hash));
  }
  _last_notify.valid = false;
  _last_notify.seen_at_ms = 0;
  memset(_last_notify.hash, 0, sizeof(_last_notify.hash));
  _last_cw_group_valid = false;
  memset(_last_cw_group_hash, 0, sizeof(_last_cw_group_hash));
  offline_queue_len = 0;
  app_target_ver = 0;
  clearPendingReqs();
  next_ack_idx = 0;
  sign_data = NULL;
  dirty_contacts_expiry = 0;
  memset(advert_paths, 0, sizeof(advert_paths));
  memset(send_scope.key, 0, sizeof(send_scope.key));
  _cannedMessageCount = 0;
  memset(_cannedMessages, 0, sizeof(_cannedMessages));
#ifdef HEADLESS_CANNED_MESSAGES
  _globalRingtone[0] = 0;
  _ringtoneRepeatCount = 1; // Default: play once
  for (size_t i = 0; i < ringtone_cfg::kMaxDeviceEntries; ++i) {
    _deviceRingtones[i].in_use = false;
    _deviceRingtones[i].tone[0] = 0;
    memset(_deviceRingtones[i].pub_key, 0, sizeof(_deviceRingtones[i].pub_key));
    _deviceRingtones[i].updated_at = 0;
  }
  setDefaultBuzzerPrefs();
  setDefaultTapTarget();
#endif
  send_unscoped = false;

  // defaults
  memset(&_prefs, 0, sizeof(_prefs));
  _prefs.airtime_factor = 1.0;
  strcpy(_prefs.node_name, "NONAME");
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.gps_enabled = 0;       // GPS disabled by default
  _prefs.gps_interval = 0;      // No automatic GPS updates by default
  //_prefs.rx_delay_base = 10.0f;  enable once new algo fixed
#if defined(USE_SX1262) || defined(USE_SX1268)
#ifdef SX126X_RX_BOOSTED_GAIN
  _prefs.rx_boosted_gain = SX126X_RX_BOOSTED_GAIN;
#else
  _prefs.rx_boosted_gain = 1; // enabled by default
#endif
#endif
}


// ---- Geofence (/mark) ----

void MyMesh::initMark() {
  // Set defaults; loadMarkPrefs() later in begin() overwrites persisted state.
  _mark_enabled = false;
  _mark_point_count = 0;
  _mark_channel_idx = 0xFF;
  _mark_outside = false;
  _mark_next_check_ms = 0;
  _mark_last_alert_lat = 0;
  _mark_last_alert_lon = 0;
}

bool MyMesh::addMarkPoint(double lat, double lon) {
  if (_mark_point_count >= kMarkMaxPoints) return false;
  // Reject obviously invalid coordinates
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return false;
  _mark_lats[_mark_point_count] = lat;
  _mark_lons[_mark_point_count] = lon;
  _mark_point_count++;
  saveMarkPrefs();
  return true;
}

void MyMesh::clearMarkPoints() {
  _mark_point_count = 0;
  _mark_outside = false;
  _mark_next_check_ms = 0;
  memset(_mark_lats, 0, sizeof(_mark_lats));
  memset(_mark_lons, 0, sizeof(_mark_lons));
  saveMarkPrefs();
}

void MyMesh::setMarkEnabled(bool on) {
  _mark_enabled = on;
  if (on) {
    _mark_outside = false;
    _mark_next_check_ms = millis() + 10000; // first check in 10s (allow GPS fix)
    // Start GPS if available
    if (sensors.node_lat == 0.0 && sensors.node_lon == 0.0) {
      sensors.setSettingValue("gps", "1");
    }
  } else {
    _mark_outside = false;
    _mark_next_check_ms = 0;
  }
}

void MyMesh::setMarkChannel(uint8_t channel_idx) {
  _mark_channel_idx = channel_idx;
  saveMarkPrefs();
}

void MyMesh::clearGeofenceFile() {
  _store->saveGeofence((const uint8_t*)"", 0);
}

// Ray-casting algorithm: is point inside polygon?
bool MyMesh::isPointInsideMark(double lat, double lon) const {
  if (_mark_point_count < 3) return false; // need a polygon
  bool inside = false;
  uint8_t j = _mark_point_count - 1;
  for (uint8_t i = 0; i < _mark_point_count; i++) {
    if ((_mark_lons[i] > lon) != (_mark_lons[j] > lon) &&
        (lat < (_mark_lats[j] - _mark_lats[i]) * (lon - _mark_lons[i]) / (_mark_lons[j] - _mark_lons[i]) + _mark_lats[i])) {
      inside = !inside;
    }
    j = i;
  }
  return inside;
}

void MyMesh::nearestMarkPoints(double lat, double lon, uint8_t& out_idx0, uint8_t& out_idx1) const {
  out_idx0 = 0;
  out_idx1 = (_mark_point_count > 1) ? 1 : 0;
  double best0 = 1e99, best1 = 1e99;
  for (uint8_t i = 0; i < _mark_point_count; i++) {
    double dlat = _mark_lats[i] - lat;
    double dlon = _mark_lons[i] - lon;
    double dist = dlat * dlat + dlon * dlon; // squared, fine for ordering nearby points
    if (dist < best0) {
      best1 = best0; out_idx1 = out_idx0;
      best0 = dist;  out_idx0 = i;
    } else if (dist < best1) {
      best1 = dist; out_idx1 = i;
    }
  }
}

void MyMesh::buildMarkMap(double lat, double lon, char* out, size_t out_size) const {
  if (_mark_point_count < 3) {
    snprintf(out, out_size, "Geofence: need >= 3 pts (%u)", (unsigned)_mark_point_count);
    return;
  }
  if ((lat > -0.001 && lat < 0.001) && (lon > -0.001 && lon < 0.001)) {
    snprintf(out, out_size, "Geofence %u pts | GPS: no fix", (unsigned)_mark_point_count);
    return;
  }

  double min_lat = 90, max_lat = -90, min_lon = 180, max_lon = -180;
  for (uint8_t i = 0; i < _mark_point_count; i++) {
    if (_mark_lats[i] < min_lat) min_lat = _mark_lats[i];
    if (_mark_lats[i] > max_lat) max_lat = _mark_lats[i];
    if (_mark_lons[i] < min_lon) min_lon = _mark_lons[i];
    if (_mark_lons[i] > max_lon) max_lon = _mark_lons[i];
  }
  if (lat < min_lat) min_lat = lat;
  if (lat > max_lat) max_lat = lat;
  if (lon < min_lon) min_lon = lon;
  if (lon > max_lon) max_lon = lon;

  // Tight padding
  double pad = (max_lat - min_lat) * 0.12;
  double pad_lon = (max_lon - min_lon) * 0.12;
  if (pad < 0.00005) pad = 0.00005;
  if (pad_lon < 0.00005) pad_lon = 0.00005;
  min_lat -= pad; max_lat += pad;
  min_lon -= pad_lon; max_lon += pad_lon;

  const int G = 5;  // compact 5x5
  double step_lat = (max_lat - min_lat) / (G - 1);
  double step_lon = (max_lon - min_lon) / (G - 1);
  if (step_lat < 1e-10) step_lat = 1e-6;
  if (step_lon < 1e-10) step_lon = 1e-6;

  char buf[192];
  int pos = 0;
  bool dev_inside = isPointInsideMark(lat, lon);
  int dx = (int)round((lon - min_lon) / step_lon);
  int dy = (int)round((lat - min_lat) / step_lat);

  // Vertex cells
  bool vt[G][G]; memset(vt, 0, sizeof(vt));
  for (uint8_t i = 0; i < _mark_point_count; i++) {
    int gx = (int)round((_mark_lons[i] - min_lon) / step_lon);
    int gy = (int)round((_mark_lats[i] - min_lat) / step_lat);
    if (gx >= 0 && gx < G && gy >= 0 && gy < G) vt[gy][gx] = true;
  }

  // Inside cells
  bool in[G][G];
  for (int y = 0; y < G; y++) {
    double clat = min_lat + y * step_lat;
    for (int x = 0; x < G; x++) {
      in[y][x] = isPointInsideMark(clat, min_lon + x * step_lon);
    }
  }

  for (int y = G - 1; y >= 0; y--) {
    for (int x = 0; x < G; x++) {
      if (x == dx && y == dy) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "*");
      } else if (vt[y][x]) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "X");
      } else {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%c", in[y][x] ? '.' : ' ');
      }
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "\n");
  }
  pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", dev_inside ? "IN" : "OUT");
  snprintf(out, out_size, "%s", buf);
}

void MyMesh::getMarkPoint(uint8_t idx, double& lat, double& lon) const {
  if (idx < _mark_point_count) {
    lat = _mark_lats[idx];
    lon = _mark_lons[idx];
  } else {
    lat = 0; lon = 0;
  }
}

void MyMesh::saveMarkPrefs() {
  // Layout: [flags][channel_idx][count][lat0][lon0]...
  //   flags bit 0 = enabled
  uint8_t buf[1 + 1 + 1 + kMarkMaxPoints * 16];
  uint8_t* p = buf;
  *p++ = _mark_enabled ? 0x01 : 0;
  *p++ = _mark_channel_idx;
  uint8_t count = _mark_point_count;
  if (count > kMarkMaxPoints) count = kMarkMaxPoints;
  *p++ = count;
  for (uint8_t i = 0; i < count; i++) {
    memcpy(p, &_mark_lats[i], 8); p += 8;
    memcpy(p, &_mark_lons[i], 8); p += 8;
  }
  _store->saveGeofence(buf, p - buf);
}

bool MyMesh::loadMarkPrefs() {
  uint8_t buf[1 + 1 + 1 + kMarkMaxPoints * 16];
  uint8_t n = 0;
  if (!_store->loadGeofence(buf, sizeof(buf), n)) return false;
  if (n < 3) return false;
  uint8_t* p = buf;
  _mark_enabled = (*p++ & 0x01) != 0;
  _mark_channel_idx = *p++;
  uint8_t count = *p++;
  if (count > kMarkMaxPoints) count = kMarkMaxPoints;
  uint8_t expected = 3 + count * 16;
  if (n < expected) count = (n - 3) / 16;
  _mark_point_count = 0;
  for (uint8_t i = 0; i < count; i++) {
    if ((size_t)(p - buf + 16) > n) break;
    memcpy(&_mark_lats[i], p, 8); p += 8;
    memcpy(&_mark_lons[i], p, 8); p += 8;
    _mark_point_count++;
  }
  return _mark_point_count > 0;
}


static double approxMeters(double dlat, double dlon, double ref_lat_rad) {
  double dy = dlat * 111320.0;
  double dx = dlon * 111320.0 * cos(ref_lat_rad);
  return sqrt(dx * dx + dy * dy);
}
void MyMesh::checkMark() {

  if (!_mark_enabled || _mark_point_count < 3) return;
  if (_mark_next_check_ms == 0 || !millisHasNowPassed(_mark_next_check_ms)) return;

  double lat = sensors.node_lat;
  double lon = sensors.node_lon;
  // Reject invalid GPS: both lat and lon near zero means no fix
  if ((lat > -0.001 && lat < 0.001) && (lon > -0.001 && lon < 0.001)) {
    _mark_next_check_ms = millis() + 30000;
    return;
  }

  bool inside = isPointInsideMark(lat, lon);
  if (inside) {
    if (_mark_outside) {
      _mark_outside = false;
      sendMarkInsideAlert();
    }
    _mark_next_check_ms = millis() + kMarkCheckIntervalMs;
    return;
  }

  // Outside fence: only send if position has changed significantly (> ~10m)
  double lat_rad = lat * (M_PI / 180.0);
  double move = approxMeters(lat - _mark_last_alert_lat, lon - _mark_last_alert_lon, lat_rad);
  if (move > 10.0 || !_mark_outside) {
    _mark_outside = true;
    _mark_last_alert_lat = lat;
    _mark_last_alert_lon = lon;
    sendMarkAlert();
  }
  _mark_next_check_ms = millis() + kMarkCheckIntervalMs;
}


void MyMesh::sendMarkAlert() {
  double lat = sensors.node_lat;
  double lon = sensors.node_lon;
  double lat_rad = lat * (M_PI / 180.0);

  // Find two nearest fence points
  uint8_t near0, near1;
  nearestMarkPoints(lat, lon, near0, near1);
  double d0 = approxMeters(_mark_lats[near0] - lat, _mark_lons[near0] - lon, lat_rad);
  double d1 = approxMeters(_mark_lats[near1] - lat, _mark_lons[near1] - lon, lat_rad);

  // Build alert message
  char msg[128];
  char dir[4] = "";
  double dlat = _mark_lats[near0] - lat;
  double dlon = _mark_lons[near0] - lon;
  if (fabs(dlat) > 1e-8 || fabs(dlon) > 1e-8) {
    if (fabs(dlat) > fabs(dlon) * 1.5) {
      snprintf(dir, sizeof(dir), " %c", (dlat >= 0) ? 'N' : 'S');
    } else if (fabs(dlon) > fabs(dlat) * 1.5) {
      snprintf(dir, sizeof(dir), " %c", (dlon >= 0) ? 'E' : 'W');
    } else {
      snprintf(dir, sizeof(dir), " %c%c", (dlat >= 0) ? 'N' : 'S', (dlon >= 0) ? 'E' : 'W');
    }
  }
  snprintf(msg, sizeof(msg), "[Geofence] OUTSIDE %s%.0fm", dir, d0);

  // Send via LoRa to target channel, also log locally so the app sees it
  if (_mark_channel_idx != 0xFF) {
    uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
    ChannelDetails ch;
    if (getChannel(_mark_channel_idx, ch)) {
      sendGroupMessage(timestamp, ch.channel, _prefs.node_name, msg, strlen(msg));
    }
    logLocalChannelMessage(_mark_channel_idx, msg);
  }
}

void MyMesh::sendMarkInsideAlert() {
  char msg[128];
  snprintf(msg, sizeof(msg), "[Geofence] INSIDE");
  if (_mark_channel_idx != 0xFF) {
    uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
    ChannelDetails ch;
    if (getChannel(_mark_channel_idx, ch)) {
      sendGroupMessage(timestamp, ch.channel, _prefs.node_name, msg, strlen(msg));
    }
    logLocalChannelMessage(_mark_channel_idx, msg);
  }
}

const char* MyMesh::describeMarkStatus() const {
  static char buf[128];
  const char* target_str = "ch0";
  {
    uint8_t ch = _mark_channel_idx;
    if (ch == 0xFF) ch = 0;
    snprintf(buf + 100, 28, "ch%u", (unsigned)ch);
    target_str = buf + 100;
  }
  if (!_mark_enabled) {
    snprintf(buf, sizeof(buf), "disabled (%u pts) | target: %s",
             (unsigned)_mark_point_count,
             _mark_channel_idx == 0xFF ? "not set" : target_str);
  } else if (_mark_point_count < 3) {
    snprintf(buf, sizeof(buf), "enabled, need %u more pts | target: %s",
             (unsigned)(3 - _mark_point_count),
             _mark_channel_idx == 0xFF ? "not set" : target_str);
  } else {
    double lat = sensors.node_lat;
    double lon = sensors.node_lon;
    bool inside = isPointInsideMark(lat, lon);
    snprintf(buf, sizeof(buf), "%s (%u pts) dev=%s | target: %s",
             _mark_outside ? "ALERT" : "OK",
             (unsigned)_mark_point_count, inside ? "INSIDE" : "OUTSIDE",
             _mark_channel_idx == 0xFF ? "ch0 (default)" : target_str);
  }
  return buf;
}

// ---- Low-battery auto-notify (/lobat) ----

void MyMesh::initLobat() {
  _lobat_count = 0;
  _lobat_next_check_ms = millis() + kLobatCheckIntervalMs;
}

void MyMesh::sendLobatMessage() {
  uint16_t mv = board.getBattMilliVolts();
  // Rough percent: 3.0V = 0%, 4.2V = 100% (typical LiPo)
  uint8_t pct = (mv <= 3000) ? 0 : (mv >= 4200) ? 100 : (uint8_t)((mv - 3000) * 100UL / 1200);
  unsigned long uptime_h = millis() / 3600000UL;
  char msg[160];
  snprintf(msg, sizeof(msg), "Batt %u%% (%umV) | up %luh | alert %u/%u",
           pct, mv, uptime_h, (unsigned)_lobat_count + 1, (unsigned)kLobatMaxCount);
  // Send via LoRa to the armed channel so all nodes in that channel see it
  uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  ChannelDetails ch;
  if (getChannel(_lobat_channel_idx, ch)) {
    sendGroupMessage(timestamp, ch.channel, _prefs.node_name, msg, strlen(msg));
  }
  _lobat_count++;
  _lobat_next_check_ms = millis() + kLobatCheckIntervalMs;
}

void MyMesh::checkLobat() {
  if (_lobat_channel_idx == 0xFF) return;           // not armed
  if (_lobat_count >= kLobatMaxCount) return;        // max alerts reached
  if (!millisHasNowPassed(_lobat_next_check_ms)) return; // not yet

  uint16_t mv = board.getBattMilliVolts();
  uint8_t pct = (mv <= 3000) ? 0 : (mv >= 4200) ? 100 : (uint8_t)((mv - 3000) * 100UL / 1200);

  if (pct < _lobat_threshold_pct) {
    sendLobatMessage();
  } else {
    // Battery recovered above threshold, reset timer and keep watching
    _lobat_next_check_ms = millis() + kLobatCheckIntervalMs;
  }
}

const char* MyMesh::describeLobatStatus() const {
  if (_lobat_channel_idx == 0xFF) return "not armed. Use /lobat here to arm.";
  static char buf[80];
  uint16_t mv = board.getBattMilliVolts();
  uint8_t pct = (mv <= 3000) ? 0 : (mv >= 4200) ? 100 : (uint8_t)((mv - 3000) * 100UL / 1200);
  snprintf(buf, sizeof(buf), "armed on ch%u, sent %u/%u, thresh %u%%, batt %u%% (%u mV)",
           (unsigned)_lobat_channel_idx, (unsigned)_lobat_count, (unsigned)kLobatMaxCount,
           (unsigned)_lobat_threshold_pct,
           (unsigned)pct, (unsigned)mv);
  return buf;
}

void MyMesh::begin(bool has_display) {
  BaseChatMesh::begin();
  _lobat_threshold_pct = kLobatDefaultThresholdPct;
  // Restore persisted state (loadLobatPrefs + loadMarkPrefs moved below after initMark/initLobat)

  if (!_store->loadMainIdentity(self_id)) {
    self_id = radio_new_identity(); // create new random identity
    int count = 0;
    while (count < 10 && (self_id.pub_key[0] == 0x00 || self_id.pub_key[0] == 0xFF)) { // reserved id hashes
      self_id = radio_new_identity();
      count++;
    }
    _store->saveMainIdentity(self_id);
  }

// if name is provided as a build flag, use that as default node name instead
#ifdef ADVERT_NAME
  strcpy(_prefs.node_name, ADVERT_NAME);
#else
  // use hex of first 4 bytes of identity public key as default node name
  char pub_key_hex[10];
  mesh::Utils::toHex(pub_key_hex, self_id.pub_key, 4);
  strcpy(_prefs.node_name, pub_key_hex);
#endif

  // if build provides default-scope, init with that
#ifdef DEFAULT_FLOOD_SCOPE_NAME
  strcpy(_prefs.default_scope_name, DEFAULT_FLOOD_SCOPE_NAME);
  {
    TransportKeyStore temp;
    TransportKey key;
    temp.getAutoKeyFor(0, "#" DEFAULT_FLOOD_SCOPE_NAME, key);
    memcpy(_prefs.default_scope_key, key.key, sizeof(key.key));
  }
#endif

  // load persisted prefs
  _store->loadPrefs(_prefs, sensors.node_lat, sensors.node_lon);

  // sanitise bad pref values
  _prefs.rx_delay_base = constrain(_prefs.rx_delay_base, 0, 20.0f);
  _prefs.airtime_factor = constrain(_prefs.airtime_factor, 0, 9.0f);
  _prefs.freq = constrain(_prefs.freq, 150.0f, 2500.0f);
  _prefs.bw = constrain(_prefs.bw, 7.8f, 500.0f);
  _prefs.sf = constrain(_prefs.sf, 5, 12);
  _prefs.cr = constrain(_prefs.cr, 5, 8);
  _prefs.tx_power_dbm = constrain(_prefs.tx_power_dbm, 1, MAX_LORA_TX_POWER);
  // Only clamp invalid values; keep RTTTL or CW if they were persisted.
  if (_prefs.notify_mode > NOTIFY_MODE_OFF) {
    _prefs.notify_mode = NOTIFY_MODE_RTTTL;
  }
  _prefs.tx_power_dbm = constrain(_prefs.tx_power_dbm, -9, MAX_LORA_TX_POWER);
  _prefs.gps_enabled = constrain(_prefs.gps_enabled, 0, 1);  // Ensure boolean 0 or 1
  _prefs.gps_interval = constrain(_prefs.gps_interval, 0, 86400);  // Max 24 hours
  if (_prefs.cw_wpm < 5 || _prefs.cw_wpm > 60) _prefs.cw_wpm = 15;  // CW speed default 15 WPM

#ifdef BLE_PIN_CODE // 123456 by default
  if (_prefs.ble_pin == 0) {
#ifdef DISPLAY_CLASS
    if (has_display && BLE_PIN_CODE == 123456) {
      StdRNG rng;
      _active_ble_pin = rng.nextInt(100000, 999999); // random pin each session
    } else {
      _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
    }
#else
    _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
#endif
  } else {
    _active_ble_pin = _prefs.ble_pin;
  }
#else
  _active_ble_pin = 0;
#endif

  resetContacts();
  _store->loadContacts(this);
  bootstrapRTCfromContacts();
  addChannel("Public", PUBLIC_GROUP_PSK); // pre-configure Andy's public channel
  _store->loadChannels(this);
  loadHeadlessCannedMessages();
#ifdef HEADLESS_CANNED_MESSAGES
  loadHeadlessRingtones();
  loadBuzzerPrefs();
  loadTapTargetPrefs();
#endif

  initLobat();
  initMark();
  // Load persisted data (AFTER initMark/initLobat so they override defaults)
  _store->loadLobatPrefs(_lobat_threshold_pct);
  loadMarkPrefs();

  radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
  radio_driver.setTxPower(_prefs.tx_power_dbm);
  radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);
  MESH_DEBUG_PRINTLN("RX Boosted Gain Mode: %s",
                     radio_driver.getRxBoostedGainMode() ? "Enabled" : "Disabled");
}

const char *MyMesh::getNodeName() {
  return _prefs.node_name;
}
NodePrefs *MyMesh::getNodePrefs() {
  return &_prefs;
}
uint32_t MyMesh::getBLEPin() {
  return _active_ble_pin;
}

struct FreqRange {
  uint32_t lower_freq, upper_freq;
};

static FreqRange repeat_freq_ranges[] = {
  #ifdef ALLOWED_REPEAT_FREQ_RANGE
  ALLOWED_REPEAT_FREQ_RANGE
  #else
  { 433000, 433000 },
  { 869495, 869495 },
  { 918000, 918000 }
  #endif
};

bool MyMesh::isValidClientRepeatFreq(uint32_t f) const {
  for (int i = 0; i < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]); i++) {
    auto r = &repeat_freq_ranges[i];
    if (f >= r->lower_freq && f <= r->upper_freq) return true;
  }
  return false;
}

void MyMesh::startInterface(BaseSerialInterface &serial) {
  _serial = &serial;
  serial.enable();
}

void MyMesh::handleCmdFrame(size_t len) {
  if (cmd_frame[0] == CMD_DEVICE_QUERY && len >= 2) { // sent when app establishes connection
    app_target_ver = cmd_frame[1];                    // which version of protocol does app understand

    int i = 0;
    out_frame[i++] = RESP_CODE_DEVICE_INFO;
    out_frame[i++] = FIRMWARE_VER_CODE;
    out_frame[i++] = MAX_CONTACTS / 2;   // v3+
    out_frame[i++] = MAX_GROUP_CHANNELS; // v3+
    memcpy(&out_frame[i], &_prefs.ble_pin, 4);
    i += 4;
    memset(&out_frame[i], 0, 12);
    strcpy((char *)&out_frame[i], FIRMWARE_BUILD_DATE);
    i += 12;
    StrHelper::strzcpy((char *)&out_frame[i], board.getManufacturerName(), 40);
    i += 40;
    StrHelper::strzcpy((char *)&out_frame[i], FIRMWARE_VERSION, 20);
    i += 20;
    out_frame[i++] = _prefs.client_repeat;   // v9+
    out_frame[i++] = _prefs.path_hash_mode;  // v10+
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_APP_START &&
             len >= 8) { // sent when app establishes connection, respond with node ID
    //  cmd_frame[1..7]  reserved future
    char *app_name = (char *)&cmd_frame[8];
    cmd_frame[len] = 0; // make app_name null terminated
    MESH_DEBUG_PRINTLN("App %s connected", app_name);

    _iter_started = false; // stop any left-over ContactsIterator
    int i = 0;
    out_frame[i++] = RESP_CODE_SELF_INFO;
    out_frame[i++] = ADV_TYPE_CHAT; // what this node Advert identifies as (maybe node's pronouns too?? :-)
    out_frame[i++] = _prefs.tx_power_dbm;
    out_frame[i++] = MAX_LORA_TX_POWER;
    memcpy(&out_frame[i], self_id.pub_key, PUB_KEY_SIZE);
    i += PUB_KEY_SIZE;

    int32_t lat, lon;
    lat = (sensors.node_lat * 1000000.0);
    lon = (sensors.node_lon * 1000000.0);
    memcpy(&out_frame[i], &lat, 4);
    i += 4;
    memcpy(&out_frame[i], &lon, 4);
    i += 4;
    out_frame[i++] = _prefs.multi_acks; // new v7+
    out_frame[i++] = _prefs.advert_loc_policy;
    out_frame[i++] = (_prefs.telemetry_mode_env << 4) | (_prefs.telemetry_mode_loc << 2) |
                     (_prefs.telemetry_mode_base); // v5+
    out_frame[i++] = _prefs.manual_add_contacts;

    uint32_t freq = _prefs.freq * 1000;
    memcpy(&out_frame[i], &freq, 4);
    i += 4;
    uint32_t bw = _prefs.bw * 1000;
    memcpy(&out_frame[i], &bw, 4);
    i += 4;
    out_frame[i++] = _prefs.sf;
    out_frame[i++] = _prefs.cr;

    int tlen = strlen(_prefs.node_name); // revisit: UTF_8 ??
    memcpy(&out_frame[i], _prefs.node_name, tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_TXT_MSG && len >= 14) {
    int i = 1;
    uint8_t txt_type = cmd_frame[i++];
    uint8_t attempt = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    uint8_t *pub_key_prefix = &cmd_frame[i];
    i += 6;
    ContactInfo *recipient = lookupContactByPubKey(pub_key_prefix, 6);
    if (recipient && (txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_CLI_DATA)) {
      char *text = (char *)&cmd_frame[i];
      int tlen = len - i;
      uint32_t est_timeout;
      text[tlen] = 0; // ensure null
#ifdef HEADLESS_CANNED_MESSAGES
      if (txt_type == TXT_TYPE_PLAIN && handleTapCommandForContact(*recipient, text)) {
        writeOKFrame();
        return;
      }
#endif
      int result;
      uint32_t expected_ack;
      if (txt_type == TXT_TYPE_CLI_DATA) {
        msg_timestamp = getRTCClock()->getCurrentTimeUnique(); // Use node's RTC instead of app timestamp to avoid tripping replay protection
        result = sendCommandData(*recipient, msg_timestamp, attempt, text, est_timeout);
        expected_ack = 0; // no Ack expected
      } else {
        result = sendMessage(*recipient, msg_timestamp, attempt, text, expected_ack, est_timeout);
      }
      // TODO: add expected ACK to table
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        if (expected_ack) {
          expected_ack_table[next_ack_idx].msg_sent = _ms->getMillis(); // add to circular table
          expected_ack_table[next_ack_idx].ack = expected_ack;
          expected_ack_table[next_ack_idx].contact = recipient;
          next_ack_idx = (next_ack_idx + 1) % EXPECTED_ACK_TABLE_SIZE;
        }

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &expected_ack, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(recipient == NULL
                        ? ERR_CODE_NOT_FOUND
                        : ERR_CODE_UNSUPPORTED_CMD); // unknown recipient, or unsupported TXT_TYPE_*
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_TXT_MSG) { // send GroupChannel text msg
    int i = 1;
    uint8_t txt_type = cmd_frame[i++]; // should be TXT_TYPE_PLAIN
    uint8_t channel_idx = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    char *text = (char *)&cmd_frame[i];
    int tlen = len - i;
    // Ensure the incoming text payload is null-terminated so command parsers
    // (e.g. /rtttl) don't read past the frame when the app sends no payload.
    if (tlen >= 0 && i + tlen < (int)sizeof(cmd_frame)) {
      text[tlen] = 0;
    } else {
      // Fallback: clamp to buffer end to avoid UB; parsers will treat as empty.
      cmd_frame[sizeof(cmd_frame) - 1] = 0;
    }

    ChannelDetails channel;
    bool success = getChannel(channel_idx, channel);
    if (!success) {
      writeErrFrame(ERR_CODE_NOT_FOUND);
#ifdef HEADLESS_CANNED_MESSAGES
    } else if (txt_type == TXT_TYPE_PLAIN &&
               handleIncomingRingtoneCommand(NULL, (int)channel_idx, &channel.channel, text, true)) {
      writeOKFrame();
#endif
    } else if (handleLocalChannelCommand(channel_idx, text, channel)) {
      writeOKFrame();
    } else if (txt_type != TXT_TYPE_PLAIN) {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    } else {
      if (sendGroupMessage(msg_timestamp, channel.channel, _prefs.node_name, text, len - i)) {
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
      }
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_DATA) { // send GroupChannel datagram
    if (len < 4) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }
    int i = 1;
    uint8_t channel_idx = cmd_frame[i++];
    uint8_t path_len = cmd_frame[i++];

    // validate path len, allowing 0xFF for flood
    if (!mesh::Packet::isValidPathLen(path_len) && path_len != OUT_PATH_UNKNOWN) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA invalid path size: %d", path_len);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }

    // parse provided path if not flood
    uint8_t path[MAX_PATH_SIZE];
    if (path_len != OUT_PATH_UNKNOWN) {
      i += mesh::Packet::writePath(path, &cmd_frame[i], path_len);
    }

    uint16_t data_type = ((uint16_t)cmd_frame[i]) | (((uint16_t)cmd_frame[i + 1]) << 8);
    i += 2;
    const uint8_t *payload = &cmd_frame[i];
    int payload_len = (len > (size_t)i) ? (int)(len - i) : 0;

    ChannelDetails channel;
    if (!getChannel(channel_idx, channel)) {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    } else if (data_type == DATA_TYPE_RESERVED) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (payload_len > MAX_CHANNEL_DATA_LENGTH) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA payload too long: %d > %d", payload_len, MAX_CHANNEL_DATA_LENGTH);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (sendGroupData(channel.channel, path, path_len, data_type, payload, payload_len)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACTS) { // get Contact list
    if (_iter_started) {
      writeErrFrame(ERR_CODE_BAD_STATE); // iterator is currently busy
    } else {
      if (len >= 5) { // has optional 'since' param
        memcpy(&_iter_filter_since, &cmd_frame[1], 4);
      } else {
        _iter_filter_since = 0;
      }

      uint8_t reply[5];
      reply[0] = RESP_CODE_CONTACTS_START;
      uint32_t count = getNumContacts(); // total, NOT filtered count
      memcpy(&reply[1], &count, 4);
      _serial->writeFrame(reply, 5);

      // start iterator
      _iter = startContactsIterator();
      _iter_started = true;
      _most_recent_lastmod = 0;
    }
  } else if (cmd_frame[0] == CMD_SET_ADVERT_NAME && len >= 2) {
    int nlen = len - 1;
    if (nlen > sizeof(_prefs.node_name) - 1) nlen = sizeof(_prefs.node_name) - 1; // max len
    memcpy(_prefs.node_name, &cmd_frame[1], nlen);
    _prefs.node_name[nlen] = 0; // null terminator
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_ADVERT_LATLON && len >= 9) {
    int32_t lat, lon, alt = 0;
    memcpy(&lat, &cmd_frame[1], 4);
    memcpy(&lon, &cmd_frame[5], 4);
    if (len >= 13) {
      memcpy(&alt, &cmd_frame[9], 4); // for FUTURE support
    }
    if (lat <= 90 * 1E6 && lat >= -90 * 1E6 && lon <= 180 * 1E6 && lon >= -180 * 1E6) {
      sensors.node_lat = ((double)lat) / 1000000.0;
      sensors.node_lon = ((double)lon) / 1000000.0;
      savePrefs();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid geo coordinate
    }
  } else if (cmd_frame[0] == CMD_GET_DEVICE_TIME) {
    uint8_t reply[5];
    reply[0] = RESP_CODE_CURR_TIME;
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply[1], &now, 4);
    _serial->writeFrame(reply, 5);
  } else if (cmd_frame[0] == CMD_SET_DEVICE_TIME && len >= 5) {
    uint32_t secs;
    memcpy(&secs, &cmd_frame[1], 4);
    uint32_t curr = getRTCClock()->getCurrentTime();
    if (secs >= curr) {
      getRTCClock()->setCurrentTime(secs);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SEND_SELF_ADVERT) {
    mesh::Packet *pkt;
    if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
      pkt = createSelfAdvert(_prefs.node_name);
    } else {
      pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
    }
    if (pkt) {
      if (len >= 2 && cmd_frame[1] == 1) { // optional param (1 = flood, 0 = zero hop)
        unsigned long delay_millis = 0;
        TransportKey default_scope;
        memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));
        sendFloodScoped(default_scope, pkt, delay_millis);
      } else {
        sendZeroHop(pkt);
      }
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_RESET_PATH && len >= 1 + 32) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      // recipient->lastmod = ??   shouldn't be needed, app already has this version of contact
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // unknown contact
    }
  } else if (cmd_frame[0] == CMD_ADD_UPDATE_CONTACT && len >= 1 + 32 + 2 + 1) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    uint32_t last_mod = getRTCClock()->getCurrentTime(); // fallback value if not present in cmd_frame
    if (recipient) {
      updateContactFromFrame(*recipient, last_mod, cmd_frame, len);
      recipient->lastmod = last_mod;
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      ContactInfo contact;
      updateContactFromFrame(contact, last_mod, cmd_frame, len);
      contact.lastmod = last_mod;
      contact.sync_since = 0;
      if (addContact(contact)) {
        dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_REMOVE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient && removeContact(*recipient)) {
      _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE);
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found, or unable to remove
    }
  } else if (cmd_frame[0] == CMD_SHARE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      if (shareContactZeroHop(*recipient)) {
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // unable to send
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACT_BY_KEY) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *contact = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (contact) {
      writeContactRespFrame(RESP_CODE_CONTACT, *contact);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found
    }
  } else if (cmd_frame[0] == CMD_EXPORT_CONTACT) {
    if (len < 1 + PUB_KEY_SIZE) {
      // export SELF
      mesh::Packet *pkt;
      if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
        pkt = createSelfAdvert(_prefs.node_name);
      } else {
        pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
      }
      if (pkt) {
        pkt->header |= ROUTE_TYPE_FLOOD; // would normally be sent in this mode

        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        uint8_t out_len = pkt->writeTo(&out_frame[1]);
        releasePacket(pkt); // undo the obtainNewPacket()
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // Error
      }
    } else {
      uint8_t *pub_key = &cmd_frame[1];
      ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
      uint8_t out_len;
      if (recipient && (out_len = exportContact(*recipient, &out_frame[1])) > 0) {
        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // not found
      }
    }
  } else if (cmd_frame[0] == CMD_IMPORT_CONTACT && len > 2 + 32 + 64) {
    if (importContact(&cmd_frame[1], len - 1)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SYNC_NEXT_MESSAGE) {
    int out_len;
    if ((out_len = getFromOfflineQueue(out_frame)) > 0) {
      _serial->writeFrame(out_frame, out_len);
#ifdef DISPLAY_CLASS
      if (_ui) _ui->msgRead(offline_queue_len);
#endif
    } else {
      out_frame[0] = RESP_CODE_NO_MORE_MESSAGES;
      _serial->writeFrame(out_frame, 1);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_PARAMS) {
    int i = 1;
    uint32_t freq;
    memcpy(&freq, &cmd_frame[i], 4);
    i += 4;
    uint32_t bw;
    memcpy(&bw, &cmd_frame[i], 4);
    i += 4;
    uint8_t sf = cmd_frame[i++];
    uint8_t cr = cmd_frame[i++];
    uint8_t repeat = 0;  // default - false
    if (len > i) {
      repeat = cmd_frame[i++];   // FIRMWARE_VER_CODE  9+
    }

    if (repeat && !isValidClientRepeatFreq(freq)) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (freq >= 150000 && freq <= 2500000 && sf >= 5 && sf <= 12 && cr >= 5 && cr <= 8 && bw >= 7000 &&
        bw <= 500000) {
      _prefs.sf = sf;
      _prefs.cr = cr;
      _prefs.freq = (float)freq / 1000.0;
      _prefs.bw = (float)bw / 1000.0;
      _prefs.client_repeat = repeat;
      savePrefs();

      radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
      MESH_DEBUG_PRINTLN("OK: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);

      writeOKFrame();
    } else {
      MESH_DEBUG_PRINTLN("Error: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_TX_POWER) {
    int8_t power = (int8_t)cmd_frame[1];
    if (power < -9 || power > MAX_LORA_TX_POWER) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.tx_power_dbm = power;
      savePrefs();
      radio_driver.setTxPower(_prefs.tx_power_dbm);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SET_TUNING_PARAMS) {
    int i = 1;
    uint32_t rx, af;
    memcpy(&rx, &cmd_frame[i], 4);
    i += 4;
    memcpy(&af, &cmd_frame[i], 4);
    i += 4;
    _prefs.rx_delay_base = ((float)rx) / 1000.0f;
    _prefs.airtime_factor = ((float)af) / 1000.0f;
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_TUNING_PARAMS) {
    uint32_t rx = _prefs.rx_delay_base * 1000, af = _prefs.airtime_factor * 1000;
    int i = 0;
    out_frame[i++] = RESP_CODE_TUNING_PARAMS;
    memcpy(&out_frame[i], &rx, 4);
    i += 4;
    memcpy(&out_frame[i], &af, 4);
    i += 4;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SET_OTHER_PARAMS) {
    _prefs.manual_add_contacts = cmd_frame[1];
    if (len >= 3) {
      _prefs.telemetry_mode_base = cmd_frame[2] & 0x03; // v5+
      _prefs.telemetry_mode_loc = (cmd_frame[2] >> 2) & 0x03;
      _prefs.telemetry_mode_env = (cmd_frame[2] >> 4) & 0x03;

      if (len >= 4) {
        _prefs.advert_loc_policy = cmd_frame[3];
        if (len >= 5) {
          _prefs.multi_acks = cmd_frame[4];
        }
      }
    }
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_PATH_HASH_MODE && cmd_frame[1] == 0 && len >= 3) {
    if (cmd_frame[2] >= 3) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.path_hash_mode = cmd_frame[2];
      savePrefs();
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_REBOOT && memcmp(&cmd_frame[1], "reboot", 6) == 0) {
    if (dirty_contacts_expiry) { // is there are pending dirty contacts write needed?
      saveContacts();
    }
    board.reboot();
  } else if (cmd_frame[0] == CMD_GET_BATT_AND_STORAGE) {
    uint8_t reply[11];
    int i = 0;
    reply[i++] = RESP_CODE_BATT_AND_STORAGE;
    uint16_t battery_millivolts = board.getBattMilliVolts();
    uint32_t used = _store->getStorageUsedKb();
    uint32_t total = _store->getStorageTotalKb();
    memcpy(&reply[i], &battery_millivolts, 2);
    i += 2;
    memcpy(&reply[i], &used, 4);
    i += 4;
    memcpy(&reply[i], &total, 4);
    i += 4;
    _serial->writeFrame(reply, i);
  } else if (cmd_frame[0] == CMD_EXPORT_PRIVATE_KEY) {
#if ENABLE_PRIVATE_KEY_EXPORT
    uint8_t reply[65];
    reply[0] = RESP_CODE_PRIVATE_KEY;
    self_id.writeTo(&reply[1], 64);
    _serial->writeFrame(reply, 65);
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_IMPORT_PRIVATE_KEY && len >= 65) {
#if ENABLE_PRIVATE_KEY_IMPORT
    if (!mesh::LocalIdentity::validatePrivateKey(&cmd_frame[1])) {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid key
    } else {
        mesh::LocalIdentity identity;
        identity.readFrom(&cmd_frame[1], 64);
        if (_store->saveMainIdentity(identity)) {
          self_id = identity;
          writeOKFrame();
          // re-load contacts, to invalidate ecdh shared_secrets
          resetContacts();
          _store->loadContacts(this);
        } else {
          writeErrFrame(ERR_CODE_FILE_IO_ERROR);
        }
    }
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_SEND_RAW_DATA && len >= 6) {
    int i = 1;
    int8_t path_len = cmd_frame[i++];
    if (path_len >= 0 && i + path_len + 4 <= len) { // minimum 4 byte payload
      uint8_t *path = &cmd_frame[i];
      i += path_len;
      auto pkt = createRawData(&cmd_frame[i], len - i);
      if (pkt) {
        sendDirect(pkt, path, path_len);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    } else {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // flood, not supported (yet)
    }
  } else if (cmd_frame[0] == CMD_SEND_LOGIN && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    char *password = (char *)&cmd_frame[1 + PUB_KEY_SIZE];
    cmd_frame[len] = 0; // ensure null terminator in password
    if (recipient) {
      uint32_t est_timeout;
      int result = sendLogin(*recipient, password, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        memcpy(&pending_login, recipient->id.pub_key, 4); // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &pending_login, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_ANON_REQ && len > 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    ContactInfo anon;
    if (recipient == NULL) { // FIRMWARE_VER_CODE 13+,  allow non-contact requests
      memset(&anon, 0, sizeof(anon));
      memcpy(anon.id.pub_key, pub_key, PUB_KEY_SIZE);
      anon.out_path_len = 0;   // default to zero-hop direct
      anon.type = ADV_TYPE_NONE;  // unknown
      anon.lastmod = getRTCClock()->getCurrentTime();

      if (addContact(anon)) recipient = &anon;
    }
    uint8_t *data = &cmd_frame[1 + PUB_KEY_SIZE];
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendAnonReq(*recipient, data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL); // contacts full
    }
  } else if (cmd_frame[0] == CMD_SEND_STATUS_REQ && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_STATUS, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        // FUTURE:  pending_status = tag;  // match this in onContactResponse()
        memcpy(&pending_status, recipient->id.pub_key, 4); // legacy matching scheme
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_PATH_DISCOVERY_REQ && cmd_frame[1] == 0 && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[2];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      // 'Path Discovery' is just a special case of flood + Telemetry req
      uint8_t req_data[9];
      req_data[0] = REQ_TYPE_GET_TELEMETRY_DATA;
      req_data[1] = ~(TELEM_PERM_BASE);    // NEW: inverse permissions mask (ie. we only want BASE telemetry)
      memset(&req_data[2], 0, 3);          // reserved
      getRNG()->random(&req_data[5], 4);   // random blob to help make packet-hash unique
      auto save = recipient->out_path_len;    // temporarily force sendRequest() to flood
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      int result = sendRequest(*recipient, req_data, sizeof(req_data), tag, est_timeout);
      recipient->out_path_len = save;
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_discovery = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ &&
             len >= 4 + PUB_KEY_SIZE) { // can deprecate, in favour of CMD_SEND_BINARY_REQ
    uint8_t *pub_key = &cmd_frame[4];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_TELEMETRY_DATA, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_telemetry = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len == 4) { // 'self' telemetry request
    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
    // query other sensors -- target specific
    sensors.querySensors(0xFF, telemetry);

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], self_id.pub_key, 6);
    i += 6; // pub_key_prefix
    uint8_t tlen = telemetry.getSize();
    memcpy(&out_frame[i], telemetry.getBuffer(), tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_BINARY_REQ && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint8_t *req_data = &cmd_frame[1 + PUB_KEY_SIZE];
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, req_data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_HAS_CONNECTION && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    if (hasConnectionTo(pub_key)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_LOGOUT && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    stopConnection(pub_key);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_CHANNEL && len >= 2) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    if (getChannel(channel_idx, channel)) {
      int i = 0;
      out_frame[i++] = RESP_CODE_CHANNEL_INFO;
      out_frame[i++] = channel_idx;
      strcpy((char *)&out_frame[i], channel.name);
      i += 32;
      memcpy(&out_frame[i], channel.channel.secret, 16);
      i += 16; // NOTE: only 128-bit supported
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 32) {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // not supported (yet)
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 16) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    StrHelper::strncpy(channel.name, (char *)&cmd_frame[2], 32);
    memset(channel.channel.secret, 0, sizeof(channel.channel.secret));
    memcpy(channel.channel.secret, &cmd_frame[2 + 32], 16); // NOTE: only 128-bit supported
    if (setChannel(channel_idx, channel)) {
      saveChannels();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    }
  } else if (cmd_frame[0] == CMD_SIGN_START) {
    out_frame[0] = RESP_CODE_SIGN_START;
    out_frame[1] = 0; // reserved
    uint32_t len = MAX_SIGN_DATA_LEN;
    memcpy(&out_frame[2], &len, 4);
    _serial->writeFrame(out_frame, 6);

    if (sign_data) {
      free(sign_data);
    }
    sign_data = (uint8_t *)malloc(MAX_SIGN_DATA_LEN);
    sign_data_len = 0;
  } else if (cmd_frame[0] == CMD_SIGN_DATA && len > 1) {
    if (sign_data == NULL || sign_data_len + (len - 1) > MAX_SIGN_DATA_LEN) {
      writeErrFrame(sign_data == NULL ? ERR_CODE_BAD_STATE : ERR_CODE_TABLE_FULL); // error: too long
    } else {
      memcpy(&sign_data[sign_data_len], &cmd_frame[1], len - 1);
      sign_data_len += (len - 1);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SIGN_FINISH) {
    if (sign_data) {
      self_id.sign(&out_frame[1], sign_data, sign_data_len);

      free(sign_data); // don't need sign_data now
      sign_data = NULL;

      out_frame[0] = RESP_CODE_SIGNATURE;
      _serial->writeFrame(out_frame, 1 + SIGNATURE_SIZE);
    } else {
      writeErrFrame(ERR_CODE_BAD_STATE);
    }
  } else if (cmd_frame[0] == CMD_SEND_TRACE_PATH && len > 10 && len - 10 < MAX_PACKET_PAYLOAD-5) {
    uint8_t path_len = len - 10;
    uint8_t flags = cmd_frame[9];
    uint8_t path_sz = flags & 0x03;  // NEW v1.11+
    if ((path_len >> path_sz) > MAX_PATH_SIZE || (path_len % (1 << path_sz)) != 0) { // make sure is multiple of path_sz
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      uint32_t tag, auth;
      memcpy(&tag, &cmd_frame[1], 4);
      memcpy(&auth, &cmd_frame[5], 4);
      auto pkt = createTrace(tag, auth, flags);
      if (pkt) {
        sendDirect(pkt, &cmd_frame[10], path_len);

        uint32_t t = _radio->getEstAirtimeFor(pkt->payload_len + pkt->path_len + 2);
        uint32_t est_timeout = calcDirectTimeoutMillisFor(t, path_len >> path_sz);

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_SET_DEVICE_PIN && len >= 5) {

    // get pin from command frame
    uint32_t pin;
    memcpy(&pin, &cmd_frame[1], 4);

    // ensure pin is zero, or a valid 6 digit pin
    if (pin == 0 || (pin >= 100000 && pin <= 999999)) {
      _prefs.ble_pin = pin;
      savePrefs();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_CUSTOM_VARS) {
    out_frame[0] = RESP_CODE_CUSTOM_VARS;
    char *dp = (char *)&out_frame[1];
    for (int i = 0; i < sensors.getNumSettings() && dp - (char *)&out_frame[1] < 140; i++) {
      if (i > 0) {
        *dp++ = ',';
      }
      strcpy(dp, sensors.getSettingName(i));
      dp = strchr(dp, 0);
      *dp++ = ':';
      strcpy(dp, sensors.getSettingValue(i));
      dp = strchr(dp, 0);
    }
    _serial->writeFrame(out_frame, dp - (char *)out_frame);
  } else if (cmd_frame[0] == CMD_SET_CUSTOM_VAR && len >= 4) {
    cmd_frame[len] = 0;
    char *sp = (char *)&cmd_frame[1];
    char *np = strchr(sp, ':'); // look for separator char
    if (np) {
      *np++ = 0; // modify 'cmd_frame', replace ':' with null
      bool success = sensors.setSettingValue(sp, np);
      if (success) {
        #if ENV_INCLUDE_GPS == 1
        // Update node preferences for GPS settings
        if (strcmp(sp, "gps") == 0) {
          _prefs.gps_enabled = (np[0] == '1') ? 1 : 0;
          savePrefs();
        } else if (strcmp(sp, "gps_interval") == 0) {
          uint32_t interval_seconds = atoi(np);
          _prefs.gps_interval = constrain(interval_seconds, 0, 86400);
          savePrefs();
        }
        #endif
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_ADVERT_PATH && len >= PUB_KEY_SIZE + 2) {
    // FUTURE use:  uint8_t reserved = cmd_frame[1];
    uint8_t *pub_key = &cmd_frame[2];
    AdvertPath *found = NULL;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {
      auto p = &advert_paths[i];
      if (memcmp(p->pubkey_prefix, pub_key, sizeof(p->pubkey_prefix)) == 0) {
        found = p;
        break;
      }
    }
    if (found) {
      int i = 0;
      out_frame[i++] = RESP_CODE_ADVERT_PATH;
      memcpy(&out_frame[i], &found->recv_timestamp, 4); i += 4;
      out_frame[i++] = found->path_len;
      i += mesh::Packet::writePath(&out_frame[i], found->path, found->path_len);
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_STATS && len >= 2) {
    uint8_t stats_type = cmd_frame[1];
    if (stats_type == STATS_TYPE_CORE) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_CORE;
      uint16_t battery_mv = board.getBattMilliVolts();
      uint32_t uptime_secs = _ms->getMillis() / 1000;
      uint8_t queue_len = (uint8_t)_mgr->getOutboundTotal();
      memcpy(&out_frame[i], &battery_mv, 2); i += 2;
      memcpy(&out_frame[i], &uptime_secs, 4); i += 4;
      memcpy(&out_frame[i], &_err_flags, 2); i += 2;
      out_frame[i++] = queue_len;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_RADIO) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_RADIO;
      int16_t noise_floor = (int16_t)_radio->getNoiseFloor();
      int8_t last_rssi = (int8_t)radio_driver.getLastRSSI();
      int8_t last_snr = (int8_t)(radio_driver.getLastSNR() * 4); // scaled by 4 for 0.25 dB precision
      uint32_t tx_air_secs = getTotalAirTime() / 1000;
      uint32_t rx_air_secs = getReceiveAirTime() / 1000;
      memcpy(&out_frame[i], &noise_floor, 2); i += 2;
      out_frame[i++] = last_rssi;
      out_frame[i++] = last_snr;
      memcpy(&out_frame[i], &tx_air_secs, 4); i += 4;
      memcpy(&out_frame[i], &rx_air_secs, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_PACKETS) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_PACKETS;
      uint32_t recv = radio_driver.getPacketsRecv();
      uint32_t sent = radio_driver.getPacketsSent();
      uint32_t n_sent_flood = getNumSentFlood();
      uint32_t n_sent_direct = getNumSentDirect();
      uint32_t n_recv_flood = getNumRecvFlood();
      uint32_t n_recv_direct = getNumRecvDirect();
      uint32_t n_recv_errors = radio_driver.getPacketsRecvErrors();
      memcpy(&out_frame[i], &recv, 4); i += 4;
      memcpy(&out_frame[i], &sent, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_errors, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid stats sub-type
    }
  } else if (cmd_frame[0] == CMD_FACTORY_RESET && memcmp(&cmd_frame[1], "reset", 5) == 0) {
    if (_serial) {
      MESH_DEBUG_PRINTLN("Factory reset: disabling serial interface to prevent reconnects (BLE/WiFi)");
      _serial->disable(); // Phone app disconnects before we can send OK frame so it's safe here
    }
    bool success = _store->formatFileSystem();
    if (success) {
      writeOKFrame();
      delay(1000);
      board.reboot(); // doesn't return
    } else {
      writeErrFrame(ERR_CODE_FILE_IO_ERROR);
    }
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 0) {
    if (len >= 2 + 16) {
      memcpy(send_scope.key, &cmd_frame[2], sizeof(send_scope.key));  // set scope override TransportKey
    } else {
      memset(send_scope.key, 0, sizeof(send_scope.key));  // reset scope override
    }
    send_unscoped = false;
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 1) {  // ver 12+
    send_unscoped = true;
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_DEFAULT_FLOOD_SCOPE && len >= 1) {
    if (len >= 1+31+16) {
      int n = strlen((char *) &cmd_frame[1]);
      if (n > 0 && n < 31) {
        strcpy(_prefs.default_scope_name, (char *) &cmd_frame[1]);
        memcpy(_prefs.default_scope_key, &cmd_frame[1+31], 16);
        savePrefs();
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      memset(_prefs.default_scope_name, 0, sizeof(_prefs.default_scope_name));  // set default scope to null
      memset(_prefs.default_scope_key, 0, sizeof(_prefs.default_scope_key));
      savePrefs();
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_GET_DEFAULT_FLOOD_SCOPE) {
    out_frame[0] = RESP_CODE_DEFAULT_FLOOD_SCOPE;
    if (strlen(_prefs.default_scope_name) > 0) {
      memcpy(&out_frame[1], _prefs.default_scope_name, 31);
      memcpy(&out_frame[1+31], _prefs.default_scope_key, 16);
      _serial->writeFrame(out_frame, 1+31+16);
    } else {
      _serial->writeFrame(out_frame, 1);   // no name or key means null
    }
  } else if (cmd_frame[0] == CMD_SEND_CONTROL_DATA && len >= 2 && (cmd_frame[1] & 0x80) != 0) {
    auto resp = createControlData(&cmd_frame[1], len - 1);
    if (resp) {
      sendZeroHop(resp);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_SET_AUTOADD_CONFIG) {
    _prefs.autoadd_config = cmd_frame[1];
    if (len >= 3) {
      _prefs.autoadd_max_hops = min(cmd_frame[2], (uint8_t)64);
    }
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_AUTOADD_CONFIG) {
    int i = 0;
    out_frame[i++] = RESP_CODE_AUTOADD_CONFIG;
    out_frame[i++] = _prefs.autoadd_config;
    out_frame[i++] = _prefs.autoadd_max_hops;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_GET_ALLOWED_REPEAT_FREQ) {
    int i = 0;
    out_frame[i++] = RESP_ALLOWED_REPEAT_FREQ;
    for (int k = 0; k < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]) && i + 8 < sizeof(out_frame); k++) {
      auto r = &repeat_freq_ranges[k];
      memcpy(&out_frame[i], &r->lower_freq, 4); i += 4;
      memcpy(&out_frame[i], &r->upper_freq, 4); i += 4;
    }
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_RAW_PACKET && len >= 4) {
    auto pkt = obtainNewPacket();
    if (pkt) {
      uint8_t priority = cmd_frame[1];
      if (tryParsePacket(pkt, &cmd_frame[2], len - 2)) {
        sendPacket(pkt, priority, 0);
        writeOKFrame();
      } else {
        releasePacket(pkt);
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    MESH_DEBUG_PRINTLN("ERROR: unknown command: %02X", cmd_frame[0]);
  }
}

static bool save_filter(const ContactInfo& c) {
  return c.type != ADV_TYPE_NONE;   // don't save the transient/anon entries
}

void MyMesh::saveContacts() {
  _store->saveContacts(this, save_filter);
}

void MyMesh::enterCLIRescue() {
  _cli_rescue = true;
  cli_command[0] = 0;
  Serial.println("========= CLI Rescue =========");
}

void MyMesh::checkCLIRescueCmd() {
  int len = strlen(cli_command);
  while (Serial.available() && len < sizeof(cli_command) - 1) {
    char c = Serial.read();
    if (c != '\n') {
      cli_command[len++] = c;
      cli_command[len] = 0;
    }
    Serial.print(c); // echo
  }
  if (len == sizeof(cli_command) - 1) { // command buffer full
    cli_command[sizeof(cli_command) - 1] = '\r';
  }

  if (len > 0 && cli_command[len - 1] == '\r') { // received complete line
    cli_command[len - 1] = 0;                    // replace newline with C string null terminator

    if (memcmp(cli_command, "set ", 4) == 0) {
      const char *config = &cli_command[4];
      if (memcmp(config, "pin ", 4) == 0) {
        _prefs.ble_pin = atoi(&config[4]);
        savePrefs();
        Serial.printf("  > pin is now %06d\n", _prefs.ble_pin);
      } else {
        Serial.printf("  Error: unknown config: %s\n", config);
      }
    } else if (strcmp(cli_command, "rebuild") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        _store->saveMainIdentity(self_id);
        savePrefs();
        saveContacts();
        saveChannels();
        Serial.println("  > erase and rebuild done");
      } else {
        Serial.println("  Error: erase failed");
      }
    } else if (strcmp(cli_command, "erase") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        Serial.println("  > erase done");
      } else {
        Serial.println("  Error: erase failed");
      }
    } else if (memcmp(cli_command, "ls", 2) == 0) {

      // get path from command e.g: "ls /adafruit"
      const char *path = &cli_command[3];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }
      Serial.printf("Listing files in %s\n", path);

      // log each file and directory
      File root = _store->openRead(path);
      if (is_fs2 == false) {
        if (root) {
          File file = root.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  UserData%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] UserData%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root.openNextFile();
          }
          root.close();
        }
      }

      if (is_fs2 == true || strlen(path) == 0 || strcmp(path, "/") == 0) {
        if (_store->getSecondaryFS() != nullptr) {
          File root2 = _store->openRead(_store->getSecondaryFS(), path);
          File file = root2.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  ExtraFS%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] ExtraFS%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root2.openNextFile();
          }
          root2.close();
        }
      }
    } else if (memcmp(cli_command, "cat", 3) == 0) {

      // get path from command e.g: "cat /contacts3"
      const char *path = &cli_command[4];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      } else {
        Serial.println("Invalid path provided, must start with UserData/ or ExtraFS/");
        cli_command[0] = 0;
        return;
      }

      // log file content as hex
      File file = _store->openRead(path);
      if (is_fs2 == true) {
        file = _store->openRead(_store->getSecondaryFS(), path);
      }
      if (file) {

        // get file content
        int file_size = file.available();
        uint8_t buffer[file_size];
        file.read(buffer, file_size);

        // print hex
        mesh::Utils::printHex(Serial, buffer, file_size);
        Serial.print("\n");

        file.close();
      }

    } else if (memcmp(cli_command, "rm ", 3) == 0) {
      // get path from command e.g: "rm /adv_blobs"
      const char *path = &cli_command[3];
      MESH_DEBUG_PRINTLN("Removing file: %s", path);
      // ensure path is not empty, or root dir
      if (!path || strlen(path) == 0 || strcmp(path, "/") == 0) {
        Serial.println("Invalid path provided");
      } else {
        bool is_fs2 = false;
        if (memcmp(path, "UserData/", 9) == 0) {
          path += 8; // skip "UserData"
        } else if (memcmp(path, "ExtraFS/", 8) == 0) {
          path += 7; // skip "ExtraFS"
          is_fs2 = true;
        }

        // remove file
        bool removed;
        if (is_fs2) {
          MESH_DEBUG_PRINTLN("Removing file from ExtraFS: %s", path);
          removed = _store->removeFile(_store->getSecondaryFS(), path);
        } else {
          MESH_DEBUG_PRINTLN("Removing file from UserData: %s", path);
          removed = _store->removeFile(path);
        }
        if (removed) {
          Serial.println("File removed");
        } else {
          Serial.println("Failed to remove file");
        }
      }

    } else if (strcmp(cli_command, "reboot") == 0) {
      board.reboot(); // doesn't return
    } else {
      Serial.println("  Error: unknown command");
    }

    cli_command[0] = 0; // reset command buffer
  }
}

void MyMesh::checkSerialInterface() {
  size_t len = _serial->checkRecvFrame(cmd_frame);
  if (len > 0) {
    handleCmdFrame(len);
  } else if (_iter_started              // check if our ContactsIterator is 'running'
             && !_serial->isWriteBusy() // don't spam the Serial Interface too quickly!
  ) {
    ContactInfo contact;
    if (_iter.hasNext(this, contact)) {
      if (contact.lastmod > _iter_filter_since) { // apply the 'since' filter
        writeContactRespFrame(RESP_CODE_CONTACT, contact);
        if (contact.lastmod > _most_recent_lastmod) {
          _most_recent_lastmod = contact.lastmod; // save for the RESP_CODE_END_OF_CONTACTS frame
        }
      }
    } else { // EOF
      out_frame[0] = RESP_CODE_END_OF_CONTACTS;
      memcpy(&out_frame[1], &_most_recent_lastmod,
             4); // include the most recent lastmod, so app can update their 'since'
      _serial->writeFrame(out_frame, 5);
      _iter_started = false;
    }
    //} else if (!_serial->isWriteBusy()) {
    //  checkConnections();    // TODO - deprecate the 'Connections' stuff
  }
}

void MyMesh::loop() {
  BaseChatMesh::loop();

  if (_cli_rescue) {
    checkCLIRescueCmd();
  } else {
    checkSerialInterface();
  }

  // is there are pending dirty contacts write needed?
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    saveContacts();
    dirty_contacts_expiry = 0;
  }

  // Low-battery auto-notify check
  checkLobat();

  // Geofence check
  checkMark();

#ifdef DISPLAY_CLASS
  if (_ui) _ui->setHasConnection(_serial->isConnected());
#endif
}

bool MyMesh::advert() {
  mesh::Packet *pkt;
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
    pkt = createSelfAdvert(_prefs.node_name);
  } else {
    pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
  }
  if (pkt) {
    sendZeroHop(pkt);
    return true;
  } else {
    return false;
  }
}

// To check if there is pending work
bool MyMesh::hasPendingWork() const {
  return _mgr->getOutboundTotal() > 0 || dirty_contacts_expiry != 0;
}
