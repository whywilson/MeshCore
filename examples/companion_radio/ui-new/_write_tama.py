#!/usr/bin/env python3
import os

content = r"""#pragma once

/**
 * TamagotchiScreen v2 - Mesh Tamagotchi
 *
 * Pet states driven by mesh health:
 *   IDLE     - 2-frame walk cycle, random blink
 *   WALK     - user moving pet left/right via encoder CW/CCW
 *   WAVE     - new node handshake (3 s)
 *   SWEAT    - stress 40-74
 *   DIZZY    - stress >= 75
 *   SLEEP    - battery <= 20%
 *   MAILRUN  - running toward mailbox (new message)
 *   MAILOPEN - opening mailbox + showing message full-screen
 *
 * Controls (in tama mode):
 *   Encoder CCW  (KEY_LEFT)  - move pet left
 *   Encoder CW   (KEY_RIGHT) - move pet right
 *   Encoder press (KEY_ENTER) - stop
 *   Side btn click - switch to legacy MeshCore screen (UITask)
 */

#include <helpers/ui/UIScreen.h>
#include <helpers/ui/DisplayDriver.h>
#include <Arduino.h>

#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif

class UITask;

static inline int _tc(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

class TamagotchiScreen : public UIScreen {
public:
  enum PetState {
    PET_IDLE, PET_WALK,
    PET_WAVE, PET_SWEAT, PET_DIZZY, PET_SLEEP,
    PET_MAILRUN, PET_MAILOPEN
  };

  int    _pet_x;      // horizontal center (pixels 0-127)
  int8_t _walk_dir;   // -1=left, +1=right, 0=stopped

private:
  UITask*       _task;
  PetState      _state;
  uint8_t       _anim;          // frame 0/1
  unsigned long _next_anim;
  unsigned long _wave_until;
  bool          _blink;
  unsigned long _next_blink;
  int           _stress;
  unsigned long _last_decay;
  char          _mail_from[32];
  char          _mail_text[80];
  unsigned long _mail_phase_start;
  unsigned long _mail_dismiss_at;
  uint8_t       _bars;
  unsigned long _next_bar_decay;
  unsigned long _last_walk_input;

  static const int GROUND_Y  = 52;
  static const int FEET_Y    = GROUND_Y - 1;
  static const int PET_X_MIN = 14;
  static const int PET_X_MAX = 118;
  static const int MAILBOX_X = 8;

  // ---- character renderer -----------------------------------------------
  // cx/fy = center-x / feet-y
  void drawBot(DisplayDriver& d, int cx, int fy,
               bool blink, bool dizzy, bool asleep,
               bool wave_arm, int bob) {
    int hy = fy - 27;  // head top
    int by = fy - 16;  // body top
    int ly = fy - 8;   // legs top

    d.setColor(DisplayDriver::LIGHT);

    // Antennas
    d.fillRect(cx - 4, hy - 4, 2, 4);
    d.fillRect(cx + 2, hy - 4, 2, 4);
    d.fillRect(cx - 5, hy - 5, 4, 2);
    d.fillRect(cx + 1, hy - 5, 4, 2);

    // Head (12x8)
    d.fillRect(cx - 6, hy, 12, 8);
    d.setColor(DisplayDriver::DARK);
    d.fillRect(cx - 5, hy + 1, 10, 6);
    d.setColor(DisplayDriver::LIGHT);
    d.drawRect(cx - 6, hy, 12, 8);

    // Eyes
    if (asleep) {
      d.fillRect(cx - 4, hy + 3, 3, 1);
      d.fillRect(cx + 1, hy + 3, 3, 1);
    } else if (dizzy) {
      d.fillRect(cx - 4, hy + 2, 2, 2);
      d.fillRect(cx - 4, hy + 4, 2, 2);
      d.fillRect(cx + 2, hy + 2, 2, 2);
      d.fillRect(cx + 2, hy + 4, 2, 2);
      d.setColor(DisplayDriver::DARK);
      d.fillRect(cx - 3, hy + 3, 2, 2);
      d.fillRect(cx + 3, hy + 3, 2, 2);
      d.setColor(DisplayDriver::LIGHT);
    } else if (!blink) {
      d.fillRect(cx - 4, hy + 2, 3, 2);
      d.fillRect(cx + 1, hy + 2, 3, 2);
    }

    // Mouth
    if (wave_arm || _state == PET_MAILOPEN) {
      d.fillRect(cx - 3, hy + 5, 6, 2);
      d.setColor(DisplayDriver::DARK);
      d.fillRect(cx - 2, hy + 5, 4, 1);
      d.setColor(DisplayDriver::LIGHT);
    } else if (asleep) {
      d.fillRect(cx - 1, hy + 6, 2, 1);
    } else if (dizzy) {
      d.fillRect(cx - 3, hy + 6, 2, 1);
      d.fillRect(cx - 1, hy + 5, 2, 1);
      d.fillRect(cx + 1, hy + 6, 2, 1);
    } else if (_state == PET_SWEAT) {
      d.fillRect(cx - 2, hy + 6, 4, 1);
    } else {
      d.fillRect(cx - 2, hy + 6, 4, 2);
      d.setColor(DisplayDriver::DARK);
      d.fillRect(cx - 1, hy + 6, 2, 1);
      d.setColor(DisplayDriver::LIGHT);
    }

    // Neck
    d.fillRect(cx - 2, hy + 8, 4, 2);

    // Body (12x8)
    d.fillRect(cx - 6, by, 12, 8);
    d.setColor(DisplayDriver::DARK);
    d.fillRect(cx - 3, by + 2, 2, 2);
    d.fillRect(cx + 1, by + 2, 2, 2);
    d.setColor(DisplayDriver::LIGHT);
    d.fillRect(cx - 2, by + 3, 1, 1);
    d.fillRect(cx + 2, by + 3, 1, 1);

    // Arms
    if (wave_arm) {
      // right arm up
      if (_anim == 0) {
        d.fillRect(cx + 6, by - 5, 3, 7);
        d.fillRect(cx + 5, by - 7, 4, 2);
        d.fillRect(cx + 9, by - 7, 3, 2);
      } else {
        d.fillRect(cx + 6, by - 3, 3, 7);
        d.fillRect(cx + 6, by - 5, 4, 2);
        d.fillRect(cx + 10, by - 5, 3, 2);
      }
      d.fillRect(cx - 9, by, 3, 6);
      d.fillRect(cx - 9, by + 6, 4, 2);
    } else {
      bool af = (_anim == 0);
      d.fillRect(cx + 6, by + (af ? 0 : 2), 3, 6);
      d.fillRect(cx + 5, by + (af ? 6 : 8), 4, 2);
      d.fillRect(cx - 9, by + (af ? 2 : 0), 3, 6);
      d.fillRect(cx - 9, by + (af ? 8 : 6), 4, 2);
    }

    // Legs
    bool lf = (_anim == 0);
    d.fillRect(cx - 5, ly + (lf ? 0 : bob), 3, 7);
    d.fillRect(cx - 6, ly + 7 + (lf ? 0 : bob), 5, 2);
    d.fillRect(cx + 2, ly + (lf ? bob : 0), 3, 7);
    d.fillRect(cx + 1, ly + 7 + (lf ? bob : 0), 5, 2);
  }

  // ---- mailbox -----------------------------------------------------------
  void drawMailbox(DisplayDriver& d, bool mail_open) {
    int mx = MAILBOX_X;
    int gy = GROUND_Y;
    d.setColor(DisplayDriver::LIGHT);
    d.fillRect(mx - 1, gy - 12, 2, 12);      // post
    d.fillRect(mx - 5, gy - 20, 10, 8);      // box
    d.setColor(DisplayDriver::DARK);
    d.fillRect(mx - 4, gy - 19, 8, 6);       // inner
    d.setColor(DisplayDriver::LIGHT);
    if (mail_open) {
      d.fillRect(mx - 5, gy - 22, 10, 3);    // lid up
    } else {
      d.fillRect(mx - 4, gy - 21, 8, 2);     // lid closed
      d.fillRect(mx + 5, gy - 17, 2, 5);     // flag post
      d.fillRect(mx + 5, gy - 17, 4, 3);     // flag
    }
    d.fillRect(mx - 3, gy - 17, 6, 1);       // slot
  }

  // ---- overlays ----------------------------------------------------------
  void drawSweat(DisplayDriver& d, int cx, int hy) {
    int off = (_anim == 0) ? 0 : 2;
    d.fillRect(cx - 9, hy + off,     2, 3);
    d.fillRect(cx - 8, hy + off + 3, 2, 1);
    d.fillRect(cx + 6, hy + off + 1, 2, 3);
  }

  void drawDizzy(DisplayDriver& d, int cx, int hy) {
    int o = (_anim == 0) ? 0 : 2;
    d.fillRect(cx - 1 + o, hy - 6, 2, 4);
    d.fillRect(cx - 3 + o, hy - 4, 6, 2);
    d.fillRect(cx - 10,    hy + o, 4, 2);
    d.fillRect(cx + 6, hy + 2 - o, 4, 2);
  }

  void drawZzz(DisplayDriver& d, int cx, int hy) {
    int zx = cx + 7 + (_anim ? 1 : 0);
    int zy = hy - 6 - (_anim ? 2 : 0);
    d.fillRect(zx,     zy,     4, 1);
    d.fillRect(zx + 1, zy + 2, 2, 1);
    d.fillRect(zx,     zy + 3, 4, 1);
    zx += 5; zy -= 2;
    d.fillRect(zx,     zy,     5, 1);
    d.fillRect(zx + 2, zy + 2, 2, 1);
    d.fillRect(zx,     zy + 4, 5, 1);
  }

  // ---- status bar --------------------------------------------------------
  void drawStatusBar(DisplayDriver& d, uint16_t batt_mv) {
    int pct = ((int)batt_mv - BATT_MIN_MILLIVOLTS) * 100
              / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
    pct = _tc(pct, 0, 100);

    d.setColor(DisplayDriver::LIGHT);
    d.drawRect(1, 1, 14, 7);
    d.fillRect(15, 3, 2, 3);
    int fill = (pct * 12) / 100;
    if (fill > 0) {
      d.setColor(pct > 30 ? DisplayDriver::GREEN : DisplayDriver::RED);
      d.fillRect(2, 2, fill, 5);
    }

    d.setColor(DisplayDriver::YELLOW);
    d.setTextSize(1);
    const char* lbl;
    switch (_state) {
      case PET_WAVE:
      case PET_MAILOPEN: lbl = "WAVE";  break;
      case PET_SWEAT:    lbl = "BUSY";  break;
      case PET_DIZZY:    lbl = "DIZZY"; break;
      case PET_SLEEP:    lbl = "SLEEP"; break;
      case PET_MAILRUN:  lbl = "MAIL";  break;
      default:           lbl = "IDLE";  break;
    }
    d.drawTextCentered(64, 1, lbl);

    for (int i = 0; i < 5; i++) {
      int bh = i + 2;
      int bx = 105 + i * 5;
      int by = 8 - bh;
      d.setColor(i < _bars ? DisplayDriver::GREEN : DisplayDriver::DARK);
      d.fillRect(bx, by, 4, bh);
      d.setColor(DisplayDriver::LIGHT);
      d.drawRect(bx, by, 4, bh);
    }

    d.setColor(DisplayDriver::LIGHT);
    d.fillRect(0, 9, 128, 1);
  }

  // ---- state machine -----------------------------------------------------
  void updateState(uint16_t batt_mv) {
    unsigned long now = millis();

    if (now - _last_decay > 2000) {
      if (_stress > 0) _stress--;
      _last_decay = now;
    }
    if (now > _next_bar_decay) {
      if (_bars > 0) _bars--;
      _next_bar_decay = now + 5000;
    }

    unsigned long anim_ms = (_state == PET_MAILRUN) ? 120 : 350;
    if (now >= _next_anim) {
      _anim ^= 1;
      _next_anim = now + anim_ms;
    }

    if (_state == PET_IDLE && now >= _next_blink) {
      _blink = !_blink;
      _next_blink = now + (_blink ? 150 : 2500 + random(2000));
    } else if (_state != PET_IDLE) {
      _blink = false;
    }

    // Mail run: slide pet toward mailbox
    if (_state == PET_MAILRUN) {
      int target = MAILBOX_X + 14;
      _pet_x -= 3;
      if (_pet_x <= target) {
        _pet_x = target;
        _state = PET_MAILOPEN;
        _mail_phase_start = now;
        _mail_dismiss_at  = now + 5500;
      }
      return;
    }
    if (_state == PET_MAILOPEN) {
      if (now > _mail_dismiss_at) {
        _state = PET_IDLE;
        _pet_x = 64;
      }
      return;
    }

    // Walk momentum
    if (_walk_dir != 0) {
      _pet_x = _tc(_pet_x + (int)_walk_dir * 2, PET_X_MIN, PET_X_MAX);
    }
    // Auto-stop after inertia
    if (now - _last_walk_input > 300) _walk_dir = 0;

    int bpct = ((int)batt_mv - BATT_MIN_MILLIVOLTS) * 100
               / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
    bpct = _tc(bpct, 0, 100);

    if      (bpct <= 20)         _state = PET_SLEEP;
    else if (_stress >= 75)      _state = PET_DIZZY;
    else if (_stress >= 40)      _state = PET_SWEAT;
    else if (now < _wave_until)  _state = PET_WAVE;
    else if (_walk_dir != 0)     _state = PET_WALK;
    else                         _state = PET_IDLE;
  }

  // ---- mail full-screen renderer -----------------------------------------
  void renderMailOpen(DisplayDriver& d, unsigned long now) {
    unsigned long elapsed = now - _mail_phase_start;
    int env_h = (int)(elapsed / 25);
    if (env_h > 50) env_h = 50;

    int ey = (64 - env_h) / 2;

    d.setColor(DisplayDriver::LIGHT);
    d.fillRect(4, ey, 120, env_h);

    if (env_h < 18) return;

    d.setColor(DisplayDriver::DARK);
    d.fillRect(5, ey + 1, 118, env_h - 2);
    d.setColor(DisplayDriver::LIGHT);

    // Envelope flap (V shape at top)
    int mid = 4 + 60;
    for (int i = 0; i < 10; i++) {
      d.fillRect(4 + i, ey + i / 2, 1, 1);
      d.fillRect(123 - i, ey + i / 2, 1, 1);
    }

    if (env_h < 36) return;

    // From header
    d.setTextSize(1);
    char hdr[36];
    snprintf(hdr, sizeof(hdr), "< %s >", _mail_from);
    d.drawTextCentered(64, ey + 4, hdr);

    d.fillRect(6, ey + 13, 116, 1);

    // Message text
    d.setCursor(8, ey + 16);
    d.printWordWrap(_mail_text, 112);

    // Countdown
    d.setColor(DisplayDriver::YELLOW);
    int remain = (int)(_mail_dismiss_at - now) / 1000;
    char hint[12];
    snprintf(hint, sizeof(hint), "  [%ds]", remain > 0 ? remain : 0);
    d.drawTextCentered(64, ey + env_h - 9, hint);
  }

public:
  explicit TamagotchiScreen(UITask* task)
    : _pet_x(64), _walk_dir(0),
      _task(task),
      _state(PET_IDLE), _anim(0), _next_anim(0),
      _wave_until(0),
      _blink(false), _next_blink(3000),
      _stress(0), _last_decay(0),
      _mail_phase_start(0), _mail_dismiss_at(0),
      _bars(0), _next_bar_decay(5000),
      _last_walk_input(0)
  {
    _mail_from[0] = 0;
    _mail_text[0] = 0;
  }

  // ---- event hooks -------------------------------------------------------

  void onHandshake() {
    _wave_until = millis() + 3000;
    onMeshActivity(15);
  }

  void onMeshActivity(int delta = 8) {
    _stress = _tc(_stress + delta, 0, 100);
    if (_bars < 5) _bars++;
    _next_bar_decay = millis() + 5000;
  }

  void onNewMessage(const char* from_name, const char* text) {
    if (_state == PET_MAILRUN || _state == PET_MAILOPEN) return;
    strncpy(_mail_from, from_name ? from_name : "???", sizeof(_mail_from) - 1);
    _mail_from[sizeof(_mail_from) - 1] = 0;
    strncpy(_mail_text, text ? text : "", sizeof(_mail_text) - 1);
    _mail_text[sizeof(_mail_text) - 1] = 0;
    _state = PET_MAILRUN;
    _mail_phase_start = millis();
    onMeshActivity(10);
  }

  PetState getState() const { return _state; }

  // ---- UIScreen ----------------------------------------------------------

  bool handleInput(char c) override {
    if (_state == PET_MAILOPEN) {
      _state = PET_IDLE;
      _pet_x = 64;
      return true;
    }
    if (c == KEY_LEFT) {
      _walk_dir = -1;
      _last_walk_input = millis();
      return true;
    }
    if (c == KEY_RIGHT) {
      _walk_dir = 1;
      _last_walk_input = millis();
      return true;
    }
    if (c == KEY_ENTER) {
      _walk_dir = 0;
      return true;
    }
    return false;
  }

  void poll() override {}

  int render(DisplayDriver& display) override {
    uint16_t batt_mv = _task ? _task->getBattMilliVolts() : 3700;
    unsigned long now = millis();
    updateState(batt_mv);

    display.setColor(DisplayDriver::LIGHT);
    drawStatusBar(display, batt_mv);

    // Ground
    display.setColor(DisplayDriver::LIGHT);
    display.fillRect(0, GROUND_Y, 128, 1);

    int cx = _pet_x;
    int fy = FEET_Y;

    if (_state == PET_MAILOPEN) {
      drawMailbox(display, true);
      drawBot(display, cx, fy, false, false, false, true, 0);
      renderMailOpen(display, now);
      return 100;
    }

    if (_state == PET_SLEEP) {
      // Character lying flat
      int sy = GROUND_Y - 10;
      display.setColor(DisplayDriver::LIGHT);
      display.fillRect(cx - 14, sy, 20, 6);
      display.fillRect(cx + 6,  sy - 5, 8, 6);
      display.setColor(DisplayDriver::DARK);
      display.fillRect(cx + 7, sy - 4, 6, 4);
      display.setColor(DisplayDriver::LIGHT);
      display.drawRect(cx + 6, sy - 5, 8, 6);
      display.fillRect(cx + 8, sy - 3, 4, 1);
      display.fillRect(cx - 18, sy + 1, 5, 3);
      display.fillRect(cx - 18, sy + 5, 5, 3);
      drawZzz(display, cx, sy - 5);
    } else {
      // Mailbox (always visible, closed)
      display.setColor(DisplayDriver::LIGHT);
      drawMailbox(display, false);

      bool wave  = (_state == PET_WAVE);
      bool sweat = (_state == PET_SWEAT);
      bool dizzy = (_state == PET_DIZZY);
      bool run   = (_state == PET_MAILRUN);
      int  bob   = ((_state == PET_IDLE || _state == PET_WALK || run) && _anim == 1) ? 1 : 0;

      drawBot(display, cx, fy, _blink, dizzy, false, wave, bob);

      if (dizzy) { display.setColor(DisplayDriver::LIGHT); drawDizzy(display, cx, fy - 27); }
      if (sweat) { display.setColor(DisplayDriver::LIGHT); drawSweat(display, cx, fy - 27); }
      if (_state == PET_SLEEP) { drawZzz(display, cx, fy - 27); }
    }

    // Bottom hint
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    display.fillRect(0, 55, 128, 1);
    display.drawTextCentered(64, 57, "dial:move  side:menu");

    return 150;
  }
};
"""

path = "/Users/wilson/Github/MeshCore/examples/companion_radio/ui-new/TamagotchiScreen.h"
with open(path, "w") as f:
    f.write(content)
print("Written", len(content), "bytes")
