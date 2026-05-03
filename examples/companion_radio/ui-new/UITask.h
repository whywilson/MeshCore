#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <Arduino.h>
#include <helpers/sensors/LPPDataHelpers.h>

#ifndef LED_STATE_ON
  #define LED_STATE_ON 1
#endif

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif
#ifdef PIN_VIBRATION
  #include <helpers/ui/GenericVibration.h>
#endif

#ifdef ENABLE_MORSE_CODE_INPUT
  #include <helpers/ui/MorseCodeInput.h>
#endif

#include "../AbstractUITask.h"
#include "../NodePrefs.h"

class UITask : public AbstractUITask {
  DisplayDriver* _display;
  SensorManager* _sensors;
#ifdef PIN_BUZZER
  genericBuzzer buzzer;
#endif
#ifdef PIN_VIBRATION
  GenericVibration vibration;
#endif
  unsigned long _next_refresh, _auto_off;
  NodePrefs* _node_prefs;
  char _alert[80];
  unsigned long _alert_expiry;
  int _msgcount;
  unsigned long ui_started_at, next_batt_chck;
  int next_backlight_btn_check = 0;
#ifdef PIN_STATUS_LED
  int led_state = 0;
  int next_led_change = 0;
  int last_led_increment = 0;
  bool _led_in_msg_select_mode = false;  // Track if in message selection mode
  bool _led_in_morse_mode = false;       // Track if in morse code mode
  unsigned long _msg_select_blink_expiry = 0;  // When to turn LED back on after blink
#endif

#ifdef PIN_USER_BTN_ANA
  unsigned long _analogue_pin_read_millis = millis();
#endif

#ifdef ENABLE_MORSE_CODE_INPUT
  MorseCodeInput morse_input;
  char _morse_input_buffer[50];  // Buffer for accumulated morse input
  uint8_t _morse_input_length;
  uint32_t _morse_last_input_time;
  bool _in_morse_mode;
  bool _in_msg_select_mode;

  // LED Replay (Time Display)
  bool _led_replay_active = false;
  char _led_replay_buffer[10];
  uint8_t _led_replay_char_idx;
  const char* _led_replay_pattern;
  uint8_t _led_replay_symbol_idx;
  uint32_t _led_replay_next_time;
  enum LedReplayState { LED_REPLAY_IDLE, LED_REPLAY_START_CHAR, LED_REPLAY_ON, LED_REPLAY_OFF, LED_REPLAY_CHAR_GAP } _led_replay_state;
  
  void startLedTimeDisplay();
  void pollLedReplay();
#endif

  UIScreen* splash;
  UIScreen* home;
  UIScreen* msg_preview;
  UIScreen* curr;

  void userLedHandler();
  void triggerMsgSelectBlink();  // Blink LED briefly when key pressed in message mode
  
  // Button action handlers
  char checkDisplayOn(char c);
  char handleLongPress(char c);
  char handleDoubleClick(char c);
  char handleTripleClick(char c);

#ifdef ENABLE_MORSE_CODE_INPUT
  void handleMorseButtonInput();
  void sendMorseMessage();
#endif

  void setCurrScreen(UIScreen* c);

public:

  UITask(mesh::MainBoard* board, BaseSerialInterface* serial) : AbstractUITask(board, serial), _display(NULL), _sensors(NULL) {
    next_batt_chck = _next_refresh = 0;
    ui_started_at = 0;
    curr = NULL;
  }
  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);

  void gotoHomeScreen() { setCurrScreen(home); }
  void showAlert(const char* text, int duration_millis);
  int  getMsgCount() const { return _msgcount; }
  bool hasDisplay() const { return _display != NULL; }
  bool isButtonPressed() const;

  bool isBuzzerQuiet() { 
#ifdef PIN_BUZZER
    return buzzer.isQuiet();
#else
    return true;
#endif
  }

  void toggleBuzzer();
  bool getGPSState();
  void toggleGPS();


  // from AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void loop() override;
  void playRingtone(const char* melody) override;
  void onNotifyModeChanged(uint8_t mode) override;
  bool isPlayingRingtone() override;
  bool isBuzzerPlaying() override;
  void pollBuzzer() override;
  void pollInput() override;
  bool isButtonPressed() const override;
  void stopBuzzer() override;

  void shutdown(bool restart = false);
};
