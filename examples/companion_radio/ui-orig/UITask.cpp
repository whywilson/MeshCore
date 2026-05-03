#include "UITask.h"
#include <Arduino.h>
#include <helpers/TxtDataHelpers.h>
#ifdef PIN_STATUS_LED
#include <helpers/ui/MorseCodeInput.h>
#endif
#ifdef HEADLESS_CANNED_MESSAGES
#include "../MyMesh.h"
#endif

#define AUTO_OFF_MILLIS     15000   // 15 seconds
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#ifdef PIN_STATUS_LED
namespace {
constexpr uint16_t kLedTimeUnitMs = 150;
constexpr uint16_t kLedTimeDotMs = kLedTimeUnitMs;
constexpr uint16_t kLedTimeDashMs = kLedTimeUnitMs * 3;
constexpr uint16_t kLedTimeSymbolGapMs = kLedTimeUnitMs;
constexpr uint16_t kLedTimeCharGapMs = kLedTimeUnitMs * 3;
constexpr uint16_t kLedTimeWordGapMs = kLedTimeUnitMs * 7;
}
#endif

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#ifdef HEADLESS_CANNED_MESSAGES
namespace {
#if defined(PIN_BUZZER)
constexpr const char *kToneCannedEnter = "CannedEnter:d=32,o=6,b=220:16c6,16e6,16g6";
constexpr const char *kToneMorseEnter = "MorseEnter:d=32,o=6,b=220:16e6,16g6,16c7";  // Higher pitch for morse mode
constexpr const char *kToneCannedExit = "CannedExit:d=32,o=5,b=180:16g5,16e5,16c5";
constexpr const char *kToneCannedSend = "CannedSend:d=16,o=6,b=200:8g5,8b5,8d6";
constexpr const char *kToneCannedError = "CannedErr:d=32,o=5,b=160:8c5,16p,8c5";
constexpr const char *kToneAdvertSuccess = "AdvertLow:d=8,o=4,b=180:8c4";
#endif
} // namespace
#endif

namespace {
#if defined(PIN_BUZZER)
constexpr const char *kToneGpsOn = "GpsOn:d=32,o=6,b=200:16c6,16e6,16g6";
constexpr const char *kToneGpsOff = "GpsOff:d=32,o=5,b=200:16g5,16e5,16c5";
constexpr const char *kTonePowerDown = "PowerDown:d=16,o=5,b=140:8g5,8e5,8c5";
#else
constexpr const char *kToneGpsOn = nullptr;
constexpr const char *kToneGpsOff = nullptr;
constexpr const char *kTonePowerDown = nullptr;
#endif
}

// 'meshcore', 128x13px
static const uint8_t meshcore_logo [] PROGMEM = {
    0x3c, 0x01, 0xe3, 0xff, 0xc7, 0xff, 0x8f, 0x03, 0x87, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 
    0x3c, 0x03, 0xe3, 0xff, 0xc7, 0xff, 0x8e, 0x03, 0x8f, 0xfe, 0x3f, 0xfe, 0x1f, 0xff, 0x1f, 0xfe, 
    0x3e, 0x03, 0xc3, 0xff, 0x8f, 0xff, 0x0e, 0x07, 0x8f, 0xfe, 0x7f, 0xfe, 0x1f, 0xff, 0x1f, 0xfc, 
    0x3e, 0x07, 0xc7, 0x80, 0x0e, 0x00, 0x0e, 0x07, 0x9e, 0x00, 0x78, 0x0e, 0x3c, 0x0f, 0x1c, 0x00, 
    0x3e, 0x0f, 0xc7, 0x80, 0x1e, 0x00, 0x0e, 0x07, 0x1e, 0x00, 0x70, 0x0e, 0x38, 0x0f, 0x3c, 0x00, 
    0x7f, 0x0f, 0xc7, 0xfe, 0x1f, 0xfc, 0x1f, 0xff, 0x1c, 0x00, 0x70, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x1f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x3f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x1e, 0x3f, 0xfe, 0x3f, 0xf0, 
    0x77, 0x3b, 0x87, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xfc, 0x38, 0x00, 
    0x77, 0xfb, 0x8f, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xf8, 0x38, 0x00, 
    0x73, 0xf3, 0x8f, 0xff, 0x0f, 0xff, 0x1c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x78, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfe, 0x3c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x3c, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfc, 0x3c, 0x0e, 0x1f, 0xf8, 0xff, 0xf8, 0x70, 0x3c, 0x7f, 0xf8, 
};

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;
  clearMsgPreview();
  _node_prefs = node_prefs;
  if (_display != NULL) {
    _display->turnOn();
  }

  // strip off dash and commit hash by changing dash to null terminator
  // e.g: v1.2.3-abcdef -> v1.2.3
  char *version = strdup(FIRMWARE_VERSION);
  char *dash = strchr(version, '-');
  if (dash) {
    *dash = 0;
  }

  // v1.2.3 (1 Jan 2025)
  sprintf(_version_info, "%s (%s)", version, FIRMWARE_BUILD_DATE);

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
  buzzer.startup();
#endif

  // Initialize digital button if available
#ifdef PIN_USER_BTN
  _userButton = new Button(PIN_USER_BTN, USER_BTN_PRESSED);
  _userButton->begin();
  
  // Set up digital button callbacks
  _userButton->onShortPress([this]() { handleButtonShortPress(); });
  _userButton->onDoublePress([this]() { handleButtonDoublePress(); });
  _userButton->onTriplePress([this]() { handleButtonTriplePress(); });
  _userButton->onQuadruplePress([this]() { handleButtonQuadruplePress(); });
  _userButton->onLongPress([this]() { handleButtonLongPress(); });
  _userButton->onLongHold([this]() { handleButtonLongHold(); });
  _userButton->onAnyPress([this]() { handleButtonAnyPress(); });
#endif

  // Initialize analog button if available
#ifdef PIN_USER_BTN_ANA
  _userButtonAnalog = new Button(PIN_USER_BTN_ANA, USER_BTN_PRESSED, true, 20);
  _userButtonAnalog->begin();
  
  // Set up analog button callbacks
  _userButtonAnalog->onShortPress([this]() { handleButtonShortPress(); });
  _userButtonAnalog->onDoublePress([this]() { handleButtonDoublePress(); });
  _userButtonAnalog->onTriplePress([this]() { handleButtonTriplePress(); });
  _userButtonAnalog->onQuadruplePress([this]() { handleButtonQuadruplePress(); });
  _userButtonAnalog->onLongPress([this]() { handleButtonLongPress(); });
  _userButtonAnalog->onLongHold([this]() { handleButtonLongHold(); });
  _userButtonAnalog->onAnyPress([this]() { handleButtonAnyPress(); });
#endif
  ui_started_at = millis();
#ifdef PIN_STATUS_LED
  led_state = 0;
  next_led_change = 0;
  last_led_increment = 0;
  _led_in_msg_select_mode = false;
  _msg_select_blink_expiry = 0;
#endif
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
  // Honor /buz off: suppress all UI beeps when notify_mode is OFF.
  if (_node_prefs && _node_prefs->notify_mode == NOTIFY_MODE_OFF) {
    return;
  }
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif
//  Serial.print("DBG:  Alert user -> ");
//  Serial.println((int) t);
}

void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    clearMsgPreview();
  }
}

void UITask::clearMsgPreview() {
  _origin[0] = 0;
  _msg[0] = 0;
  _need_refresh = true;
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;

  if (path_len == 0xFF) {
    sprintf(_origin, "(F) %s", from_name);
  } else {
    sprintf(_origin, "(%d) %s", (uint32_t) path_len, from_name);
  }
  StrHelper::strncpy(_msg, text, sizeof(_msg));

  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
    _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
    _need_refresh = true;
    }
  }
}

void UITask::renderBatteryIndicator(uint16_t batteryMilliVolts) {
  // Convert millivolts to percentage
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif
  const int minMilliVolts = BATT_MIN_MILLIVOLTS;
  const int maxMilliVolts = BATT_MAX_MILLIVOLTS;
  int batteryPercentage = ((batteryMilliVolts - minMilliVolts) * 100) / (maxMilliVolts - minMilliVolts);
  if (batteryPercentage < 0) batteryPercentage = 0; // Clamp to 0%
  if (batteryPercentage > 100) batteryPercentage = 100; // Clamp to 100%

  // battery icon
  int iconWidth = 24;
  int iconHeight = 12;
  int iconX = _display->width() - iconWidth - 5; // Position the icon near the top-right corner
  int iconY = 0;
  _display->setColor(DisplayDriver::GREEN);

  // battery outline
  _display->drawRect(iconX, iconY, iconWidth, iconHeight);

  // battery "cap"
  _display->fillRect(iconX + iconWidth, iconY + (iconHeight / 4), 3, iconHeight / 2);

  // fill the battery based on the percentage
  int fillWidth = (batteryPercentage * (iconWidth - 4)) / 100;
  _display->fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4);
}

void UITask::renderCurrScreen() {
  if (_display == NULL) return;  // assert() ??

  char tmp[80];
  if (_alert[0]) {
    _display->setTextSize(1.4);
    uint16_t textWidth = _display->getTextWidth(_alert);
    _display->setCursor((_display->width() - textWidth) / 2, 22);
    _display->setColor(DisplayDriver::GREEN);
    _display->print(_alert);
    _alert[0] = 0;
    _need_refresh = true;
    return;
  } else if (_origin[0] && _msg[0]) { // message preview
    // render message preview
    _display->setCursor(0, 0);
    _display->setTextSize(1);
    _display->setColor(DisplayDriver::GREEN);
    _display->print(_node_prefs->node_name);

    _display->setCursor(0, 12);
    _display->setColor(DisplayDriver::YELLOW);
    _display->print(_origin);
    _display->setCursor(0, 24);
    _display->setColor(DisplayDriver::LIGHT);
    _display->print(_msg);

    _display->setCursor(_display->width() - 28, 9);
    _display->setTextSize(2);
    _display->setColor(DisplayDriver::ORANGE);
    sprintf(tmp, "%d", _msgcount);
    _display->print(tmp);
    _display->setColor(DisplayDriver::YELLOW); // last color will be kept on T114
  } else if ((millis() - ui_started_at) < BOOT_SCREEN_MILLIS) { // boot screen
    // meshcore logo
    _display->setColor(DisplayDriver::BLUE);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // version info
    _display->setColor(DisplayDriver::LIGHT);
    _display->setTextSize(1);
    uint16_t textWidth = _display->getTextWidth(_version_info);
    _display->setCursor((_display->width() - textWidth) / 2, 22);
    _display->print(_version_info);
  } else {  // home screen
    // node name
    _display->setCursor(0, 0);
    _display->setTextSize(1);
    _display->setColor(DisplayDriver::GREEN);
    _display->print(_node_prefs->node_name);

    // battery voltage
    renderBatteryIndicator(_board->getBattMilliVolts());

    // freq / sf
    _display->setCursor(0, 20);
    _display->setColor(DisplayDriver::YELLOW);
    sprintf(tmp, "FREQ: %06.3f SF%d", _node_prefs->freq, _node_prefs->sf);
    _display->print(tmp);

    // bw / cr
    _display->setCursor(0, 30);
    sprintf(tmp, "BW: %03.2f CR: %d", _node_prefs->bw, _node_prefs->cr);
    _display->print(tmp);

    // BT pin
    if (!_connected && the_mesh.getBLEPin() != 0) {
      _display->setColor(DisplayDriver::RED);
      _display->setTextSize(2);
      _display->setCursor(0, 43);
      sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
      _display->print(tmp);
      _display->setColor(DisplayDriver::GREEN);
    } else {
      _display->setColor(DisplayDriver::LIGHT); 
    }
  }
  _need_refresh = false;
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  unsigned long cur_time = millis();

  if (_led_time_playback_active) {
    if (cur_time >= _led_time_next_transition) {
      if (_led_time_step_index >= _led_time_step_count) {
        resetLedTimeAnnouncement();
        led_state = 0;
        digitalWrite(PIN_STATUS_LED, 0);
        next_led_change = cur_time;
      } else {
        const auto &step = _led_time_steps[_led_time_step_index++];
        led_state = step.state;
        digitalWrite(PIN_STATUS_LED, step.state ? HIGH : LOW);
        _led_time_next_transition = cur_time + step.duration_ms;
      }
    }
    if (_led_time_playback_active) {
      return;
    }
  }
  
  // Message Selection Mode: LED stays on, blinks on button press
  if (_led_in_msg_select_mode) {
    // Check if we're in a blink-off period (LED should be off)
    if (cur_time < _msg_select_blink_expiry) {
      if (led_state != 0) {
        led_state = 0;
        digitalWrite(PIN_STATUS_LED, 0);  // Turn off during blink period
      }
      return;
    }
    // Blink period expired, turn LED back on
    if (led_state != 1) {
      led_state = 1;
      digitalWrite(PIN_STATUS_LED, 1);  // Turn on after blink
      _msg_select_blink_expiry = 0;  // Clear blink expiry
    }
    return;
  }
  
  // Normal Mode: Original LED blinking behavior
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, state == LED_STATE_ON);
  }
#endif
}

/* 
  hardware-agnostic pre-shutdown activity should be done here 
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    radio_driver.powerOff();
    _board->powerOff();
  }
}

void UITask::loop() {
  #ifdef PIN_USER_BTN
    if (_userButton) {
      _userButton->update();
    }
  #endif
  #ifdef PIN_USER_BTN_ANA
    if (_userButtonAnalog) {
      _userButtonAnalog->update();
    }
  #endif

  // Update LED state based on button press
#ifdef PIN_STATUS_LED
  bool buttonPressed = false;
  #ifdef PIN_USER_BTN
    if (_userButton && _userButton->isPressed()) {
      buttonPressed = true;
    }
  #endif
  #ifdef PIN_USER_BTN_ANA
    if (_userButtonAnalog && _userButtonAnalog->isPressed()) {
      buttonPressed = true;
    }
  #endif
  // LED follows button state: on when pressed, off when released
  // Only apply this logic if NOT in message selection mode (which manages LED itself)
  if (!_led_in_msg_select_mode && !_led_time_playback_active) {
    digitalWrite(PIN_STATUS_LED, buttonPressed ? HIGH : LOW);
  }
#endif
  
  userLedHandler();

#ifdef HEADLESS_CANNED_MESSAGES
  updateCannedMode();
#endif

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  {
    buzzer.loop();
  } else {
    // Drain CW queue when buzzer is free.
    if (_cwCount > 0) {
      buzzer.play(_cwQueue[_cwHead]);
      _cwHead = (_cwHead + 1) % kCwQueueCapacity;
      --_cwCount;
    }
  }
#endif

  if (_display != NULL && _display->isOn()) {
    static bool _firstBoot = true;
    if(_firstBoot && (millis() - ui_started_at) >= BOOT_SCREEN_MILLIS) {
      _need_refresh = true;
      _firstBoot = false;
    }
    if (millis() >= _next_refresh && _need_refresh) {
      _display->startFrame();
      renderCurrScreen();
      _display->endFrame();

      _next_refresh = millis() + 1000;   // refresh every second
    }
    if (millis() > _auto_off) {
      _display->turnOff();
    }
  }
}

#ifdef HEADLESS_CANNED_MESSAGES
void UITask::enterCannedMode() {
#ifdef PIN_STATUS_LED
  resetLedTimeAnnouncement();
#endif
  size_t cannedCount = the_mesh.getHeadlessCannedMessageCount();
  if (cannedCount == 0) {
    return;
  }
  _cannedSelecting = true;
  _morseInputMode = false;  // Start in canned message mode
  _cannedIndex = -1;
  _cannedLastInteraction = millis();
#ifdef PIN_STATUS_LED
  _led_in_msg_select_mode = true;  // Enable LED constant on with key-press blinks
  led_state = 1;  // Force LED on
  digitalWrite(PIN_STATUS_LED, 1);  // Turn on immediately
  _msg_select_blink_expiry = 0;  // Reset blink expiry
#endif

  _morse_is_pressed = false;
  _morse_last_debounce_time = millis();
  _morse_last_reading_state = false;
  _morse_feedback_busy = false;
  _morse_feedback_end = 0;
  
  // Initialize Morse decoding buffers
  _morseInput.begin();
  _morse_message_len = 0;
  _morse_message[0] = '\0';
  
  MESH_DEBUG_PRINTLN("UITask: canned message mode entered");
  playCannedTone(kToneCannedEnter);
  if (_display != NULL) {
    snprintf(_alert, sizeof(_alert), "Canned Msg (%u)",
             (unsigned)cannedCount);
    _need_refresh = true;
  }
}

void UITask::exitCannedMode(bool playTone) {
  if (!_cannedSelecting) {
    return;
  }
  _cannedSelecting = false;
  _morseInputMode = false;
  _cannedIndex = -1;
#ifdef PIN_STATUS_LED
  _led_in_msg_select_mode = false;  // Disable LED message selection mode
#endif
  if (playTone) {
    playCannedTone(kToneCannedExit);
  }
  MESH_DEBUG_PRINTLN("UITask: message mode exited");
}

void UITask::advanceCannedMessage() {
  size_t cannedCount = the_mesh.getHeadlessCannedMessageCount();
  if (!_cannedSelecting || _morseInputMode || cannedCount == 0) {
    return;
  }
  _cannedLastInteraction = millis();
  _cannedIndex = (_cannedIndex + 1) % cannedCount;
  playBinarySelectionTone(_cannedIndex);
  const char* msg = the_mesh.getHeadlessCannedMessage(_cannedIndex);
  MESH_DEBUG_PRINTLN("UITask: canned msg [%d] %s", _cannedIndex + 1, msg);
}

bool UITask::sendCurrentCannedMessage() {
  size_t cannedCount = the_mesh.getHeadlessCannedMessageCount();
  if (!_cannedSelecting || _cannedIndex < 0 ||
      _cannedIndex >= (int)cannedCount || cannedCount == 0) {
    return false;
  }

  MyMesh::TapTargetState target;
  if (!the_mesh.resolveTapTarget(target)) {
    MESH_DEBUG_PRINTLN("UITask: no tap target for canned message");
    playCannedTone(kToneCannedError);
    return false;
  }

  const char *text = the_mesh.getHeadlessCannedMessage(_cannedIndex);
  uint32_t timestamp = the_mesh.getRTCClock()->getCurrentTimeUnique();
  const char *sender = (_node_prefs != NULL) ? _node_prefs->node_name : "";

  bool ok = false;
  if (target.type == MyMesh::TapTargetType::Channel) {
    ok = the_mesh.sendGroupMessage(timestamp, target.channel.channel, sender, text,
                                   strlen(text));
    if (ok) {
      the_mesh.logLocalChannelMessage(target.channel_idx, text);
    }
  } else {
    uint32_t expected_ack = 0;
    uint32_t est_timeout = 0;
    int result = the_mesh.sendMessage(target.contact, timestamp, 0, text, expected_ack, est_timeout);
    ok = result != MSG_SEND_FAILED;
    if (ok) {
      the_mesh.logLocalChannelMessage(0, text);
    }
  }

  if (ok) {
    playCannedTone(kToneCannedSend);
    MESH_DEBUG_PRINTLN("UITask: canned message sent -> %s", text);
    if (_display != NULL) {
      snprintf(_alert, sizeof(_alert), "Sent: %s", text);
      _need_refresh = true;
    }
  } else {
    playCannedTone(kToneCannedError);
    MESH_DEBUG_PRINTLN("UITask: failed to send canned message");
  }
  return ok;
}

bool UITask::sendQuickAdvert() {
  bool ok = the_mesh.advert();
  if (ok) {
    playTone(kToneAdvertSuccess);
    MESH_DEBUG_PRINTLN("UITask: advert sent");
  } else {
    playCannedTone(kToneCannedError);
    MESH_DEBUG_PRINTLN("UITask: advert failed");
  }
  return ok;
}

void UITask::updateCannedMode() {
  // Only process if in message mode
  if (!_cannedSelecting) {
    return;
  }
  
  // Check for timeout (applies to both modes)
  if ((millis() - _cannedLastInteraction) > kCannedSelectionTimeoutMs) {
    if (_morseInputMode) {
      finalizeMorseChar();  // Finalize any pending character
      Serial.println();  // Newline after morse output
    }
    MESH_DEBUG_PRINTLN("UITask: message mode timeout");
    exitCannedMode();
    return;
  }

  // Only process Morse input in Morse mode
  if (!_morseInputMode) {
    return;
  }

  // Use Button Input instead of Light Sensor
  // "按键输入morse code"
  
  bool raw_pressed = isButtonPressed();

  // LED Control during Morse Input: ON when pressed, OFF when released
#ifdef PIN_STATUS_LED
  // Skip if we are in feedback playback, otherwise follow button state
  if (!_morse_feedback_busy) {
      digitalWrite(PIN_STATUS_LED, raw_pressed ? HIGH : LOW);
  }
#endif

  // Skip logic during audible feedback period to avoid self-triggering if using mic? 
  // (Not relevant for button, but kept for feedback pacing)
  if (_morse_feedback_busy) {
    if (millis() >= _morse_feedback_end) {
      _morse_feedback_busy = false;
      // Restore LED state based on current button state immediately
#ifdef PIN_STATUS_LED      
       digitalWrite(PIN_STATUS_LED, raw_pressed ? HIGH : LOW);
#endif
    } else {
      return;  // Still in feedback period
    }
  }

  if (raw_pressed != _morse_last_reading_state) {
    _morse_last_debounce_time = millis();
  }

  // Check for character gap
  char decoded = 0;
  if (!_morse_is_pressed && _morseInput.checkGap(decoded)) {
    if (decoded != 0 && _morse_message_len < kMorseMessageMaxLen) {
      _morse_message[_morse_message_len++] = decoded;
      _morse_message[_morse_message_len] = '\0';
      Serial.printf(" [%c] -> \"%s\"\n", decoded, _morse_message);
    }
  }
  
  // Check for word gap
  if (_morse_message_len > 0 && 
      !_morse_is_pressed &&
      *_morseInput.getCurrentPattern() == 0 &&
      (millis() - _morseInput.getLastChangeTime()) > MorseCodeInput::WORD_SEPARATOR_TIME &&
      _morse_message[_morse_message_len - 1] != ' ') {
    if (_morse_message_len < kMorseMessageMaxLen) {
      _morse_message[_morse_message_len++] = ' ';
      _morse_message[_morse_message_len] = '\0';
      Serial.printf(" [SPACE] -> \"%s\"\n", _morse_message);
    }
  }

  if ((millis() - _morse_last_debounce_time) > 30) { // 30ms debounce
    if (raw_pressed != _morse_is_pressed) {
      _morse_is_pressed = raw_pressed;

      if (_morse_is_pressed) {
        // Pressed
        _morse_press_duration_start = millis();
        _cannedLastInteraction = millis(); // Keep mode alive
        
      } else {
        // Released
        unsigned long duration = millis() - _morse_press_duration_start;
        // Ignore very short glitches (<50ms)
        if (duration > 50) {
            // Logic:
            // < 2000ms: Dot/Dash input
            // >= 2000ms: Message Sending (Handled on Release effectively if not caught by hold check)
            
            if (duration < 2000) {
              bool isDash = (duration >= MorseCodeInput::DASH_THRESHOLD);  // 400ms threshold
              Serial.print(isDash ? "-" : ".");
              if (isDash) _morseInput.addDash(); else _morseInput.addDot();
              playMorseFeedback(isDash); // Play sound immediately
              _cannedLastInteraction = millis(); // Keep mode alive
            } else {
               // Held for >= 2000ms and released.
               // Verify if we already sent it during hold?
               // If this block runs, it means it wasn't caught by the hold check below 
               // (unlikely given loop speed, but possible if blocked)
               // OR the hold check fired, reset state, and we are seeing the release now.
               // We need to ensure we don't double send.
               // The hold check calls exitCannedMode(true) which kills this loop. 
               // So if we are here, we are still in mode.
               
               // Trigger Send on Release if not already done
               MESH_DEBUG_PRINTLN("UITask: Morse send triggered by long press release");
               finalizeMorseChar();
               if (_morse_message_len > 0) {
                  sendMorseMessage();
                  exitCannedMode(false); // Suppress exit tone, let Send tone play
                  return; // Stop processing
               } else {
                  playCannedTone(kToneCannedError); 
               }
            }
        }
      }
    } else {
      // Button state stable
      if (_morse_is_pressed) {
         // Check for Long Hold (Send) while still pressed
         if (millis() - _morse_press_duration_start > 2000) {
            // Held for > 2s -> Trigger Send Immediately
            MESH_DEBUG_PRINTLN("UITask: Morse send triggered by 2s hold monitoring");
            
            finalizeMorseChar(); // If any partial char
            
            if (_morse_message_len > 0) {
                // Send and Exit
                sendMorseMessage();
                exitCannedMode(false); // Suppress exit tone, let Send tone play
            } else {
                playCannedTone(kToneCannedError);
                // Prevent re-trigger or accidental dash on release
                // We fake a 'release' or just wait until user actually releases?
                // If we don't exit mode, user is still holding button.
                // We should probably wait for release before accepting new input.
                // But error tone indicates failure.
                // Reset start time to avoid repeated triggering?
                _morse_press_duration_start = millis(); 
            }
         }
      }
    }
  }
  _morse_last_reading_state = raw_pressed;
}

void UITask::playMorseFeedback(bool isDash) {
  // Feedback duration: dot=100ms, dash=400ms (matching CW timing at b=480)
  unsigned long feedbackDuration = isDash ? 400 : 100;
  _morse_feedback_busy = true;
  _morse_feedback_end = millis() + feedbackDuration;

#ifdef PIN_BUZZER
  // Play audio feedback if notify_mode is CW or RTTTL (not OFF)
  if (_node_prefs && _node_prefs->notify_mode != NOTIFY_MODE_OFF) {
    // Match CW mode: d=8,o=5,b=480, note=c# (dot=8th note, dash=2nd note ~4x longer)
    const char* dotTone = "MorseDot:d=8,o=5,b=480:8c#";
    const char* dashTone = "MorseDash:d=8,o=5,b=480:2c#";
    buzzer.play(isDash ? dashTone : dotTone);
  }
#endif

#ifdef PIN_STATUS_LED
  // LED feedback: turn OFF during feedback period
  led_state = 0;
  digitalWrite(PIN_STATUS_LED, 0);  // Turn off LED during feedback
#endif
}

// Morse Code Decoding Table
void UITask::finalizeMorseChar() {
  const char* pattern = _morseInput.getCurrentPattern();
  if (pattern && *pattern) {
    char decoded = MorseCodeInput::decodePattern(pattern);
    if (decoded == 0) decoded = '?';
    
    if (_morse_message_len < kMorseMessageMaxLen) {
      _morse_message[_morse_message_len++] = decoded;
      _morse_message[_morse_message_len] = '\0';
      Serial.printf(" [%c] -> \"%s\"\n", decoded, _morse_message);
    }
    _morseInput.reset();
  }
}

void UITask::sendMorseMessage() {
  // Finalize any pending character
  finalizeMorseChar();

  if (_morse_message_len == 0) {
    Serial.println("Morse: No message to send");
    playCannedTone(kToneCannedError);
    return;
  }
  
  Serial.printf("Morse: Sending \"%s\"\n", _morse_message);
  
  MyMesh::TapTargetState target;
  if (!the_mesh.resolveTapTarget(target)) {
    MESH_DEBUG_PRINTLN("UITask: no tap target for morse message");
    playCannedTone(kToneCannedError);
    return;
  }
  
  uint32_t timestamp = the_mesh.getRTCClock()->getCurrentTimeUnique();
  const char *sender = (_node_prefs != NULL) ? _node_prefs->node_name : "";
  
  bool ok = false;
  if (target.type == MyMesh::TapTargetType::Channel) {
    ok = the_mesh.sendGroupMessage(timestamp, target.channel.channel, sender, 
                                   _morse_message, _morse_message_len);
    if (ok) {
      the_mesh.logLocalChannelMessage(target.channel_idx, _morse_message);
    }
  } else {
    uint32_t expected_ack = 0;
    uint32_t est_timeout = 0;
    int result = the_mesh.sendMessage(target.contact, timestamp, 0, _morse_message, 
                                      expected_ack, est_timeout);
    ok = result != MSG_SEND_FAILED;
    if (ok) {
      the_mesh.logLocalChannelMessage(0, _morse_message);
    }
  }
  
  if (ok) {
    playCannedTone(kToneCannedSend);
    Serial.printf("Morse: Message sent -> %s\n", _morse_message);
  } else {
    playCannedTone(kToneCannedError);
    Serial.println("Morse: Failed to send message");
  }
  
  // Clear message buffer after sending
  _morse_message_len = 0;
  _morse_message[0] = '\0';
  _morseInput.reset();
}

void UITask::speakMorseMessage() {
  // Finalize any pending character first
  finalizeMorseChar();
  
  if (_morse_message_len == 0) {
    Serial.println("Morse: No message to speak");
    playCannedTone(kToneCannedError);
    return;
  }
  
  Serial.printf("Morse: Speaking \"%s\"\n", _morse_message);
  
#ifdef PIN_BUZZER
  // Build RTTTL melody matching CW mode settings (d=8,o=5,b=480, note=c#)
  static char melody[512];
  int pos = snprintf(melody, sizeof(melody), "MorseSpeak:d=8,o=5,b=480:");
  for (uint8_t i = 0; i < _morse_message_len && pos < (int)sizeof(melody) - 20; i++) {
    char ch = toupper(_morse_message[i]);
    const char* morseCode = nullptr;
    
    if (ch == ' ') {
      // Word gap ~7 units (matching CW mode)
      pos += snprintf(&melody[pos], sizeof(melody) - pos, "7p,");
      continue;
    }
    
    morseCode = MorseCodeInput::getPattern(ch);
    
    if (morseCode) {
      for (const char* p = morseCode; *p && pos < (int)sizeof(melody) - 10; p++) {
        if (*p == '.') {
          // Dot: 8th note c# + 8th rest (matching CW mode)
          pos += snprintf(&melody[pos], sizeof(melody) - pos, "8c#,8p,");
        } else if (*p == '-') {
          // Dash: 2nd note c# + 8th rest (matching CW mode)
          pos += snprintf(&melody[pos], sizeof(melody) - pos, "2c#,8p,");
        }
      }
      // Inter-letter gap: additional 4th rest (matching CW mode)
      pos += snprintf(&melody[pos], sizeof(melody) - pos, "4p,");
    }
  }
  
  // Remove trailing comma if present
  if (pos > 0 && melody[pos-1] == ',') {
    melody[pos-1] = '\0';
  }
  
  buzzer.play(melody);
#endif

  if (_display != NULL) {
    snprintf(_alert, sizeof(_alert), "Msg: %s", _morse_message);
    _need_refresh = true;
  }
  
  _cannedLastInteraction = millis();  // Reset timeout
}

void UITask::playCannedTone(const char *melody) {
  playTone(melody);
}

void UITask::playBinarySelectionTone(uint8_t index) {
#if defined(PIN_BUZZER)
  static char melody[128];
  int pos = snprintf(melody, sizeof(melody), "TapSel:d=32,o=5,b=220:");
  for (int bit = 2; bit >= 0 && pos < (int)sizeof(melody) - 1; --bit) {
    const char* note = (index & (1 << bit)) ? "16a6" : "16f5";
    pos += snprintf(&melody[pos], sizeof(melody) - pos, "%s", note);
    if (bit > 0 && pos < (int)sizeof(melody) - 1) {
      pos += snprintf(&melody[pos], sizeof(melody) - pos, ",16p,");
    }
  }
  buzzer.play(melody);
#else
  (void)index;
#endif
}
#endif  // HEADLESS_CANNED_MESSAGES

void UITask::handleButtonAnyPress() {
  MESH_DEBUG_PRINTLN("UITask: any press triggered");
  // called on any button press before other events, to wake up the display quickly
  // do not refresh the display here, as it may block the button handler
  if (_display != NULL) {
    _displayWasOn = _display->isOn();  // Track display state before any action
    if (!_displayWasOn) {
      _display->turnOn();
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
  }
  
  // Trigger LED blink in message selection mode
#ifdef PIN_STATUS_LED
  if (_led_in_msg_select_mode) {
    _msg_select_blink_expiry = millis() + 100;  // Blink off for 100ms
  }
#endif
}

void UITask::handleButtonShortPress() {
  MESH_DEBUG_PRINTLN("UITask: short press triggered");
  
#ifdef HEADLESS_CANNED_MESSAGES
  // In Morse Input Mode, we strictly use raw timing (in updateCannedMode)
  // Ignore event-based callbacks to prevent "smart" button logic from interfering
  if (_cannedSelecting && _morseInputMode) {
    _cannedLastInteraction = millis(); // Refresh timeout
    return;
  }
#endif

  // Stop any playing RTTTL or CW when button is pressed
#ifdef PIN_BUZZER
  if (buzzer.isPlaying()) {
    rtttl::stop();
    Serial.println("[Button] Stopped ringtone/CW playback");
    return;
  }
#endif

#ifdef HEADLESS_CANNED_MESSAGES
  if (_cannedSelecting) {
    // If in Morse Input Mode, ignore high-level events (handled in updateCannedMode loop)
    if (_morseInputMode) {
      _cannedLastInteraction = millis();
      return;
    }

    _cannedLastInteraction = millis();  // Reset timeout
    
    // In canned message mode: single click cycles through messages
    advanceCannedMessage();
    if (_display != NULL) {
      const char* msg = the_mesh.getHeadlessCannedMessage(_cannedIndex);
      snprintf(_alert, sizeof(_alert), "[%d] %s", _cannedIndex + 1, msg);
      _need_refresh = true;
    }
    return;
  }

  bool advertSent = sendQuickAdvert();
#ifdef PIN_STATUS_LED
  startLedTimeAnnouncement();
#endif
  if (_display != NULL) {
    snprintf(_alert, sizeof(_alert), advertSent ? "Advert sent" : "Advert failed");
    _need_refresh = true;
  }
  return;
#endif
  if (_display != NULL) {
    // Only clear message preview if display was already on before button press
    if (_displayWasOn) {
      // If display was on and showing message preview, clear it
      if (_origin[0] && _msg[0]) {
        clearMsgPreview();
      } else {
        // Otherwise, refresh the display
        _need_refresh = true;
      }
    } else {
      _need_refresh = true; // display just turned on, so we need to refresh
    }
    // Note: Display turn-on and auto-off timer extension are handled by handleButtonAnyPress
  }
}

void UITask::handleButtonDoublePress() {
  MESH_DEBUG_PRINTLN("UITask: double press triggered");
#ifdef HEADLESS_CANNED_MESSAGES
  if (_cannedSelecting) {
    // In Morse Input Mode, ignore Double Press events.
    // We treat them as two separate raw inputs in updateCannedMode.
    if (_morseInputMode) {
      _cannedLastInteraction = millis();
      return;
    }
    
    // In canned message mode: double click switches to Morse input mode
    _morseInputMode = true;
    _cannedLastInteraction = millis();
    
    // Clear Morse buffers
    _morseInput.reset();
    _morse_message_len = 0;
    _morse_message[0] = '\0';
    _morse_is_pressed = false;
    
    MESH_DEBUG_PRINTLN("UITask: Morse input mode entered");
    playCannedTone(kToneMorseEnter);
    if (_display != NULL) {
      snprintf(_alert, sizeof(_alert), "Morse Input");
      _need_refresh = true;
    }
  } else {
    enterCannedMode();
  }
  return;
#endif
  // Non-headless: send advert (legacy behavior)
  #ifdef PIN_BUZZER
    notify(UIEventType::ack);
  #endif
  bool ok = the_mesh.advert();
  if (_display != NULL) {
    snprintf(_alert, sizeof(_alert), ok ? "Advert sent" : "Advert failed");
    _need_refresh = true;
  }
}

void UITask::handleButtonTriplePress() {
  MESH_DEBUG_PRINTLN("UITask: triple press triggered");
#ifdef HEADLESS_CANNED_MESSAGES
  if (_cannedSelecting && _morseInputMode) {
      _cannedLastInteraction = millis();
      return; // Ignore in Morse Input Mode
  }
  
  // In Morse input mode: triple press resets the current message
  // (Removed as requested: only timeout exits/resets)
  
  // Headless: triple press toggles GPS
  bool gpsEnabled;
  if (toggleGPSSetting(gpsEnabled)) {
  #if defined(PIN_BUZZER)
    playTone(gpsEnabled ? kToneGpsOn : kToneGpsOff);
  #endif
    if (_display != NULL) {
      snprintf(_alert, sizeof(_alert), "GPS: %s", gpsEnabled ? "Enabled" : "Disabled");
      _need_refresh = true;
    }
  }
#else
  // Non-headless: triple press toggles buzzer Flip to mute
#ifdef PIN_BUZZER
  if (buzzer.isQuiet()) {
    buzzer.quiet(false);
    notify(UIEventType::ack);
    snprintf(_alert, sizeof(_alert), "Buzzer: ON");
  } else {
    buzzer.quiet(true);
    snprintf(_alert, sizeof(_alert), "Buzzer: OFF");
  }
  if (_display != NULL) _need_refresh = true;
#endif
#endif
}

void UITask::handleButtonQuadruplePress() {
  MESH_DEBUG_PRINTLN("UITask: quadruple press triggered");
  // Easter Egg removed as requested
}

void UITask::handleButtonLongPress() {
  MESH_DEBUG_PRINTLN("UITask: long press triggered");
#ifdef HEADLESS_CANNED_MESSAGES
  if (_cannedSelecting) {
    if (_morseInputMode) {
      // In Morse Input Mode, ignore standard Button Long Press (1.2s)
      // We handle our own long-hold logic (2s) in updateCannedMode
      _cannedLastInteraction = millis();
      return; 
    }
    // In canned message mode: long press sends selected canned message
    if (sendCurrentCannedMessage()) {
      exitCannedMode(false);
    }
    return;
  }
#endif
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
  }
}

void UITask::handleButtonLongHold() {
  MESH_DEBUG_PRINTLN("UITask: long hold triggered");
#ifdef HEADLESS_CANNED_MESSAGES
  if (_cannedSelecting && _morseInputMode) {
      // In Morse mode: Long hold 2s triggers sending the message
      // Note: This matches the user requirement "2s to send"
      MESH_DEBUG_PRINTLN("UITask: Morse send triggered by long hold");
      
      finalizeMorseChar(); // Ensure last character is captured
      
      if (_morse_message_len > 0) {
        sendMorseMessage();
        exitCannedMode(false); // Suppress exit tone, let Send tone play
      } else {
        playCannedTone(kToneCannedError);
      }
      return; 
  }
#endif
  playTone(kTonePowerDown);
  shutdown();
}

void UITask::playTone(const char *melody) {
#if defined(PIN_BUZZER)
  if (melody != nullptr) {
    buzzer.play(melody);
  }
#else
  (void)melody;
#endif
}

void UITask::playRingtone(const char* melody) {
  Serial.printf("[Ringtone] UITask::playRingtone called - melody='%s'\n", melody ? melody : "NULL");
#ifdef ENABLE_FLIP_MUTE
  Serial.printf("[Ringtone] _node_prefs=%p, flipmute_enabled=%d, notify_mode=%d\n", 
                _node_prefs, (_node_prefs ? _node_prefs->flipmute_enabled : -1), 
                (_node_prefs ? _node_prefs->notify_mode : -1));
#else
  Serial.printf("[Ringtone] _node_prefs=%p, notify_mode=%d\n", _node_prefs, (_node_prefs ? _node_prefs->notify_mode : -1));
#endif
  
  // Also respect /buz off for ringtones/RTTTL/CW.
  if (_node_prefs && _node_prefs->notify_mode == NOTIFY_MODE_OFF) {
    Serial.println("[Ringtone] notify_mode is OFF, returning");
    return;
  }
  
  // Check FlipMute: if enabled and device is face-down in dark, don't play sound
  #ifdef T1000_E
  #ifdef ENABLE_FLIP_MUTE
    extern bool t1000e_is_face_down_in_dark(uint32_t light_threshold_lux);
    if (_node_prefs && _node_prefs->flipmute_enabled) {
      Serial.println("[Ringtone] FlipMute enabled in UITask, checking orientation");
      if (t1000e_is_face_down_in_dark(3)) {  // threshold: 3 lux
        Serial.println("[FlipMute] Device face-down and dark in UITask::playRingtone, suppressing sound");
        return;
      }
    } else {
      if (_node_prefs) {
        Serial.printf("[Ringtone] FlipMute disabled in UITask (flipmute_enabled=%d)\n", _node_prefs->flipmute_enabled);
      }
    }
  #endif
  #endif
  
#if defined(PIN_BUZZER)
  // In CW mode, always enqueue so playback path is unified and not interrupted mid-stream.
  if (_node_prefs && _node_prefs->notify_mode == NOTIFY_MODE_CW && melody) {
    if (_cwCount < kCwQueueCapacity) {
      StrHelper::strncpy(_cwQueue[_cwTail], melody, kCwMaxLen);
      _cwTail = (_cwTail + 1) % kCwQueueCapacity;
      ++_cwCount;
      // If buzzer idle, start immediately from head to avoid delay.
      if (!buzzer.isPlaying()) {
        buzzer.play(_cwQueue[_cwHead]);
        _cwHead = (_cwHead + 1) % kCwQueueCapacity;
        --_cwCount;
      }
    }
    return;
  }
#endif
  playTone(melody);
}

void UITask::onNotifyModeChanged(uint8_t mode) {
#ifdef PIN_BUZZER
  if (mode == NOTIFY_MODE_OFF) {
    buzzer.quiet(true);
  } else if (buzzer.isQuiet()) {
    buzzer.quiet(false);
  }
#else
  (void)mode;
#endif
}

bool UITask::toggleGPSSetting(bool &enabledOut) {
  if (_sensors == NULL) {
    return false;
  }
  int num = _sensors->getNumSettings();
  for (int i = 0; i < num; i++) {
    if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
      bool currentlyOn = strcmp(_sensors->getSettingValue(i), "1") == 0;
      _sensors->setSettingValue("gps", currentlyOn ? "0" : "1");
      enabledOut = !currentlyOn;
      return true;
    }
  }
  return false;
}

bool UITask::isBuzzerPlaying() {
#ifdef PIN_BUZZER
  return buzzer.isPlaying();
#else
  return false;
#endif
}

void UITask::pollBuzzer() {
#ifdef PIN_BUZZER
  buzzer.loop();
#endif
}

void UITask::pollInput() {
#ifdef PIN_USER_BTN
  if (_userButton) _userButton->update();
#endif
#ifdef PIN_USER_BTN_ANA
  if (_userButtonAnalog) _userButtonAnalog->update();
#endif
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  if (_userButton) return _userButton->isPressed();
#endif
#ifdef PIN_USER_BTN_ANA
  if (_userButtonAnalog) return _userButtonAnalog->isPressed();
#endif
  return false;
}

void UITask::stopBuzzer() {
#ifdef PIN_BUZZER
  // Play empty string to stop current playback
  buzzer.play(""); 
#endif
}

#ifdef PIN_STATUS_LED
void UITask::startLedTimeAnnouncement() {
  if (_led_in_msg_select_mode) {
    MESH_DEBUG_PRINTLN("UITask: skip LED time playback (LED busy)");
    return;
  }

  mesh::RTCClock* rtc = nullptr;
#ifdef HEADLESS_CANNED_MESSAGES
  rtc = the_mesh.getRTCClock();
#endif
  if (rtc == nullptr) {
    MESH_DEBUG_PRINTLN("UITask: RTC unavailable, skip LED time playback");
    return;
  }

  resetLedTimeAnnouncement();

  uint32_t now = rtc->getCurrentTime();
  uint32_t daySeconds = now % 86400UL;
  uint8_t hour = (daySeconds / 3600UL) % 24U;
  uint8_t minute = (daySeconds / 60UL) % 60U;

  buildTimeLedPattern(hour, minute);
  if (_led_time_step_count == 0) {
    MESH_DEBUG_PRINTLN("UITask: LED time playback pattern empty");
    return;
  }

  _led_time_playback_active = true;
  _led_time_step_index = 0;
  _led_time_next_transition = millis();

  MESH_DEBUG_PRINTLN("UITask: LED time playback -> %02u:%02u", hour, minute);
}

void UITask::resetLedTimeAnnouncement() {
  _led_time_playback_active = false;
  _led_time_step_index = 0;
  _led_time_step_count = 0;
  _led_time_next_transition = 0;
}

void UITask::buildTimeLedPattern(uint8_t hour, uint8_t minute) {
  _led_time_step_count = 0;
  char timeDigits[5];
  snprintf(timeDigits, sizeof(timeDigits), "%02u%02u", hour, minute);

  for (int i = 0; i < 4 && _led_time_step_count < kLedTimeMaxSteps; ++i) {
    const char* pattern = MorseCodeInput::getPattern(timeDigits[i]);
    if (!pattern) {
      MESH_DEBUG_PRINTLN("UITask: missing Morse pattern for digit %c", timeDigits[i]);
      continue;
    }
    uint16_t gap = (i == 1) ? kLedTimeWordGapMs : kLedTimeCharGapMs;
    if (i == 3) {
      gap = kLedTimeWordGapMs;
    }
    appendLedSymbolSequence(pattern, gap);
  }
}

void UITask::appendLedSymbolSequence(const char* pattern, uint16_t gapAfterMs) {
  if (!pattern) {
    return;
  }

  for (const char* symbol = pattern; *symbol && _led_time_step_count < kLedTimeMaxSteps; ++symbol) {
    uint16_t onDuration = (*symbol == '.') ? kLedTimeDotMs : kLedTimeDashMs;
    _led_time_steps[_led_time_step_count++] = {onDuration, 1};
    if (_led_time_step_count >= kLedTimeMaxSteps) {
      break;
    }

    uint16_t gap = (*(symbol + 1) == '\0') ? gapAfterMs : kLedTimeSymbolGapMs;
    _led_time_steps[_led_time_step_count++] = {gap, 0};
  }
}
#endif