#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/SensorManager.h>
#include <stddef.h>

#ifdef HEADLESS_CANNED_MESSAGES
#include <helpers/ChannelDetails.h>
#include <helpers/ui/MorseCodeInput.h>
#endif

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "../AbstractUITask.h"
#include "../NodePrefs.h"

#include "Button.h"

class UITask : public AbstractUITask {
  DisplayDriver* _display;
  SensorManager* _sensors;
#ifdef PIN_BUZZER
  genericBuzzer buzzer;
  static constexpr uint8_t kCwQueueCapacity = 4;
  static constexpr uint16_t kCwMaxLen = 1536;
  char _cwQueue[kCwQueueCapacity][kCwMaxLen];
  uint8_t _cwHead = 0;
  uint8_t _cwTail = 0;
  uint8_t _cwCount = 0;
#endif
  unsigned long _next_refresh, _auto_off;
  NodePrefs* _node_prefs;
  char _version_info[32];
  char _origin[62];
  char _msg[80];
  char _alert[80];
  int _msgcount;
  bool _need_refresh = true;
  bool _displayWasOn = false;  // Track display state before button press
  unsigned long ui_started_at;

#ifdef PIN_STATUS_LED
  int led_state = 0;
  int next_led_change = 0;
  int last_led_increment = 0;
  bool _led_in_msg_select_mode = false;  // Track if in message selection mode
  unsigned long _msg_select_blink_expiry = 0;  // When to turn LED back on after blink
  struct LedPulseStep {
    uint16_t duration_ms;
    uint8_t state;
  };
  static constexpr uint8_t kLedTimeMaxSteps = 96;
  LedPulseStep _led_time_steps[kLedTimeMaxSteps];
  uint8_t _led_time_step_count = 0;
  uint8_t _led_time_step_index = 0;
  unsigned long _led_time_next_transition = 0;
  bool _led_time_playback_active = false;
  void startLedTimeAnnouncement();
  void resetLedTimeAnnouncement();
  void buildTimeLedPattern(uint8_t hour, uint8_t minute);
  void appendLedSymbolSequence(const char* pattern, uint16_t gapAfterMs);
#endif

  // Button handlers
#ifdef PIN_USER_BTN
  Button* _userButton = nullptr;
#endif
#ifdef PIN_USER_BTN_ANA
  Button* _userButtonAnalog = nullptr;
#endif

  void renderCurrScreen();
  void userLedHandler();
  void renderBatteryIndicator(uint16_t batteryMilliVolts);
  
  // Button action handlers
  void handleButtonAnyPress();
  void handleButtonShortPress();
  void handleButtonDoublePress();
  void handleButtonTriplePress();
  void handleButtonQuadruplePress();
  void handleButtonLongPress();
  void handleButtonLongHold();
  void playTone(const char *melody);
  bool toggleGPSSetting(bool &enabledOut);

#ifdef HEADLESS_CANNED_MESSAGES
  static constexpr uint32_t kCannedSelectionTimeoutMs = 10000;
  bool _cannedSelecting = false;  // In message mode (canned or morse)
  bool _morseInputMode = false;   // false = canned message mode, true = morse input mode
  int8_t _cannedIndex = -1;
  uint32_t _cannedLastInteraction = 0;
  
  // Morse Code Input variables
  MorseCodeInput _morseInput;
  bool _morse_is_pressed = false;
  uint32_t _morse_light_threshold = 0;
  unsigned long _morse_last_debounce_time = 0;
  bool _morse_last_reading_state = false; // true = dark (pressed)
  unsigned long _morse_press_duration_start = 0;
  bool _morse_feedback_busy = false;  // True when playing feedback (skip light detection)
  unsigned long _morse_feedback_end = 0;  // When feedback period ends
  
  // Morse Code Decoding
  static constexpr size_t kMorseMessageMaxLen = 64;  // Max decoded message length
  char _morse_message[kMorseMessageMaxLen + 1];  // Decoded message buffer
  uint8_t _morse_message_len = 0;  // Length of decoded message
  
  void playMorseFeedback(bool isDash);  // Play morse dot/dash feedback
  void finalizeMorseChar();  // Decode and append character to message
  void sendMorseMessage();  // Send the decoded message
  void speakMorseMessage();  // Play current morse message as audio
  void enterCannedMode();
  void exitCannedMode(bool playTone = true);
  void advanceCannedMessage();
  bool sendCurrentCannedMessage();
  bool sendQuickAdvert();
  void updateCannedMode();
  void playCannedTone(const char *melody);
  void playBinarySelectionTone(uint8_t index);
#endif

 
public:

  UITask(mesh::MainBoard* board, BaseSerialInterface* serial) : AbstractUITask(board, serial), _display(NULL), _sensors(NULL) {
      _next_refresh = 0;
      ui_started_at = 0;
  }
  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);

  bool hasDisplay() const { return _display != NULL; }
  void clearMsgPreview();

  // from AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void loop() override;
  void playRingtone(const char* melody) override;
  void onNotifyModeChanged(uint8_t mode) override;
  bool isBuzzerPlaying() override;
  void pollBuzzer() override;
  void pollInput() override;
  bool isButtonPressed() const override;
  void stopBuzzer() override;

  void shutdown(bool restart = false);
};
