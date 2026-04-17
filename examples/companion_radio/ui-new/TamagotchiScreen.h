#pragma once

/**
 * TamagotchiScreen v2 - Mesh Tamagotchi
 *
 * Controls (tama mode):
 *   Joystick L/R      - walk the pet
 *   Encoder click     - short jump (cannot reach top icons)
 *   Encoder long-hold - high jump (reaches top icons)
 *   Side btn click    - toggle display
 *   Side btn long     - toggle tama / MeshCore mode
 */

#include <helpers/ui/UIScreen.h>
#include <helpers/ui/DisplayDriver.h>
#include <Arduino.h>
#include "../MyMesh.h"

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
    PET_IDLE, PET_WALK, PET_JUMP,
    PET_WAVE, PET_SWEAT, PET_DIZZY, PET_SLEEP,
    PET_MAIL_INCOMING,
    PET_MAILRUN, PET_MAILOPEN,
    PET_MAIL_MENU,
    PET_SMS_RECIPIENT, PET_SMS_SELECT,
    PET_SMS_SEND_ANIM, PET_SMS_SEND_DROP
  };

  int    _pet_x;
  int    _target_x;
  int8_t _walk_dir;

private:
  UITask*       _task;
  PetState      _state;
  uint8_t       _anim;
  unsigned long _next_anim;
  unsigned long _wave_until;
  bool          _blink;
  unsigned long _next_blink;
  int           _stress;
  unsigned long _last_decay;
  char          _mail_from[32];
  char          _mail_text[80];
  static const int _mail_history_size = 4;
  char          _mail_history_from[4][32];
  char          _mail_history_text[4][80];
  int           _mail_history_count;
  int           _mail_view_idx;
  unsigned long _mail_phase_start;
  int           _mail_run_from_x;
  unsigned long _mail_dismiss_at;
  int           _mail_action_idx;
  bool          _pending_nav_beep;
  // Recipient selection (channels + contacts)
  int           _recip_idx;
  int           _recip_count;
  bool          _reply_to_channel = false;
  int           _reply_channel_idx = -1;
  char          _sms_nav_dir;
  unsigned long _sms_nav_anim_until;

  // SMS quick-send
  const char*  _quick_msgs[6] = {"Hi","On my way","OK","Need help","Thank you","Call me"};
  static const int _quick_msg_count = 6;
  int           _sms_msg_idx  = 0;
  int           _sms_rcpt_idx = 0;
  int           _sms_rcpt_count = 0;
  char          _sms_rcpt_names[6][20];
  int           _sms_rcpt_contact_idx[6];
  bool          _sms_direct_reply = false;
  unsigned long _sms_send_anim_start = 0;
  unsigned long _sms_send_drop_start = 0;
  int           _sms_env_start_x = 0;
  int           _sms_env_start_y = 0;
  int           _sms_env_x = 0;
  int           _sms_env_y = 0;

  // Deferred icon trigger (executed on landing)
  char          _pending_icon[8];

  uint8_t       _bars;
  unsigned long _next_bar_decay;
  unsigned long _last_walk_input;
  char          _last_nav_key;
  int           _jump_y;
  int           _jump_vy;
  unsigned long _jump_last_physics;
  bool          _landed;
  bool          _high_jump;   // true = long-press jump (cap at MAX_JUMP_HI)
  unsigned long _last_keypress_ms;

  static const int GROUND_Y  = 63;
  static const int FEET_Y    = GROUND_Y - 1;
  static const int PET_X_MIN = 22;
  static const int PET_X_MAX = 118;
  static const int MAILBOX_X = 8;

  // Icon X centres (evenly spaced, x=30/56/82/108)
  static const int ICON_ADV = 33;   // broadcast (speaker)
  static const int ICON_SMS = 60;   // messages
  static const int ICON_BT  = 87;   // bluetooth
  static const int ICON_BZ  = 114;  // bell / ringtone

  // Jump caps:
  //   head_top = FEET_Y - jump_y - 30 = 31 - jump_y
  //   icon zone: head_top <= 14  =>  jump_y >= 17
  //   MAX_JUMP_HI = 25  =>  head_top = 6   (touches icons)
  //   MAX_JUMP_LO = 12  =>  head_top = 19  (below icons)
  static const int MAX_JUMP_HI = 25;
  static const int MAX_JUMP_LO = 12;

  bool hasMail() const {
    return _mail_history_count > 0;
  }

  void loadViewedMail() {
    if (_mail_history_count <= 0) {
      _mail_from[0] = 0;
      _mail_text[0] = 0;
      return;
    }

    if (_mail_view_idx < 0) _mail_view_idx = 0;
    if (_mail_view_idx >= _mail_history_count) _mail_view_idx = _mail_history_count - 1;

    strncpy(_mail_from, _mail_history_from[_mail_view_idx], sizeof(_mail_from) - 1);
    _mail_from[sizeof(_mail_from) - 1] = 0;
    strncpy(_mail_text, _mail_history_text[_mail_view_idx], sizeof(_mail_text) - 1);
    _mail_text[sizeof(_mail_text) - 1] = 0;
  }

  void pushMail(const char* from_name, const char* text) {
    for (int i = _mail_history_size - 1; i > 0; --i) {
      strncpy(_mail_history_from[i], _mail_history_from[i - 1], sizeof(_mail_history_from[i]) - 1);
      _mail_history_from[i][sizeof(_mail_history_from[i]) - 1] = 0;
      strncpy(_mail_history_text[i], _mail_history_text[i - 1], sizeof(_mail_history_text[i]) - 1);
      _mail_history_text[i][sizeof(_mail_history_text[i]) - 1] = 0;
    }

    strncpy(_mail_history_from[0], from_name ? from_name : "???", sizeof(_mail_history_from[0]) - 1);
    _mail_history_from[0][sizeof(_mail_history_from[0]) - 1] = 0;
    {
      // Normalize double-space after ':' (e.g. "Name:  msg" -> "Name: msg")
      char tmp_text[sizeof(_mail_history_text[0])];
      strncpy(tmp_text, text ? text : "", sizeof(tmp_text) - 1);
      tmp_text[sizeof(tmp_text) - 1] = 0;
      char* p = tmp_text;
      while ((p = strstr(p, ":  ")) != NULL) { memmove(p + 2, p + 3, strlen(p + 3) + 1); }
      strncpy(_mail_history_text[0], tmp_text, sizeof(_mail_history_text[0]) - 1);
    }
    _mail_history_text[0][sizeof(_mail_history_text[0]) - 1] = 0;

    if (_mail_history_count < _mail_history_size) _mail_history_count++;
    _mail_view_idx = 0;
    loadViewedMail();
  }

  void closeMailView() {
    _state = PET_IDLE;
    _pet_x = PET_X_MIN;
    _mail_action_idx = 0;
  }

  bool loadSmsRecipients() {
    _sms_rcpt_count = 0;
    int total = the_mesh.getNumContacts();
    for (int i = 0; i < total && _sms_rcpt_count < 6; i++) {
      ContactInfo ci;
      if (the_mesh.getContactByIdx(i, ci) && ci.name[0]) {
        strncpy(_sms_rcpt_names[_sms_rcpt_count], ci.name, 19);
        _sms_rcpt_names[_sms_rcpt_count][19] = 0;
        _sms_rcpt_contact_idx[_sms_rcpt_count] = i;
        _sms_rcpt_count++;
      }
    }
    return _sms_rcpt_count > 0;
  }

  int getChannelCount() {
    int ch_count = 0;
#ifdef MAX_GROUP_CHANNELS
    ChannelDetails cd;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      if (the_mesh.getChannel(i, cd) && cd.name[0]) ch_count++;
    }
#endif
    return ch_count;
  }

  int findChannelIndexByName(const char* name) {
#ifdef MAX_GROUP_CHANNELS
    ChannelDetails cd;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      if (the_mesh.getChannel(i, cd) && cd.name[0] && strcmp(cd.name, name) == 0) return i;
    }
#endif
    return -1;
  }

  bool openSmsRecipientChooser() {
    loadSmsRecipients();
    int ch_count = getChannelCount();
    int contact_count = _sms_rcpt_count;
    _recip_count = ch_count + contact_count;
    if (_recip_count <= 0) {
      if (_task) _task->showAlert("No targets", 1000);
      return false;
    }

    _recip_idx = 0;
    _sms_msg_idx = 0;
    _sms_direct_reply = false;
    _reply_to_channel = false;
    _reply_channel_idx = -1;
    _state = PET_SMS_RECIPIENT;
    return true;
  }

  void openMailView() {
    _state = PET_MAILOPEN;
    _mail_phase_start = millis();
    _mail_action_idx = 0;
    loadViewedMail();
  }

  bool openReplyComposer() {
    _sms_msg_idx = 0;
    _sms_direct_reply = true;

    int channel_idx = findChannelIndexByName(_mail_from);
    if (channel_idx >= 0) {
      _reply_to_channel = true;
      _reply_channel_idx = channel_idx;
      _state = PET_SMS_SELECT;
      playMenuBeep();
      return true;
    }

    if (!loadSmsRecipients()) {
      if (_task) _task->showAlert("No contacts", 1000);
      return false;
    }

    for (int i = 0; i < _sms_rcpt_count; i++) {
      if (strcmp(_sms_rcpt_names[i], _mail_from) == 0) {
        _reply_to_channel = false;
        _reply_channel_idx = -1;
        _sms_rcpt_idx = _sms_rcpt_contact_idx[i];
        _state = PET_SMS_SELECT;
        playMenuBeep();
        return true;
      }
    }

    if (_task) _task->showAlert("Target missing", 1000);
    _sms_direct_reply = false;
    _reply_to_channel = false;
    _reply_channel_idx = -1;
    playMenuBeep();
    return false;
  }

  void startSmsSendAnimation() {
    _sms_send_anim_start = millis();
    _sms_send_drop_start = 0;
    if (abs(_pet_x - (MAILBOX_X + 14)) <= 4) {
      dispatchSelectedQuickMessage();
      _sms_direct_reply = false;
      _state = PET_IDLE;
      return;
    }
    _sms_env_start_x = _pet_x - 18;
    _sms_env_start_y = FEET_Y - 20;
    _sms_env_x = _sms_env_start_x;
    _sms_env_y = _sms_env_start_y;
    _walk_dir = 0;
    _target_x = _pet_x;
    _pending_nav_beep = false;
    _state = PET_SMS_SEND_ANIM;
  }

  bool dispatchSelectedQuickMessage() {
    if (!_task) return false;

    if (_reply_to_channel) {
      ChannelDetails chd;
      if (the_mesh.getChannel(_reply_channel_idx, chd)) {
        bool ok = the_mesh.sendGroupMessage(rtc_clock.getCurrentTime(), chd.channel,
                                            the_mesh.getNodeName(), _quick_msgs[_sms_msg_idx],
                                            strlen(_quick_msgs[_sms_msg_idx]));
        _task->showAlert(ok ? "Sent!" : "Failed", 1000);
        return ok;
      }
    } else {
      ContactInfo ci;
      if (the_mesh.getContactByIdx(_sms_rcpt_idx, ci)) {
        uint32_t ack, tout;
        int r = the_mesh.sendMessage(ci, rtc_clock.getCurrentTime(), 0,
                                     _quick_msgs[_sms_msg_idx], ack, tout);
        _task->showAlert(r >= 0 ? "Sent!" : "Failed", 1000);
        return (r >= 0);
      }
    }

    _task->showAlert("Failed", 1000);
    return false;
  }

  void drawWingFeathers(DisplayDriver& d, int anchor_x, int anchor_y, int scale, bool right) {
    auto px = [&](int ox, int oy, int w, int h) {
      int draw_x = right ? (anchor_x + ox) : (anchor_x - ox - w + 1);
      d.fillRect(draw_x, anchor_y + oy, w, h);
    };

    if (scale == 2) {
      px(0, 1, 1, 3);
      px(1, 0, 1, 3);
      px(2, 1, 1, 2);
      px(3, 2, 1, 1);
      px(1, 4, 1, 1);
      return;
    }

    if (scale == 3) {
      px(0, 1, 1, 4);
      px(1, 0, 1, 5);
      px(2, 1, 1, 4);
      px(3, 2, 1, 3);
      px(4, 3, 1, 2);
      px(2, 5, 1, 1);
      return;
    }

    px(0, 1, 1, 5);
    px(1, 0, 1, 6);
    px(2, 1, 1, 6);
    px(3, 2, 1, 5);
    px(4, 3, 1, 4);
    px(5, 4, 1, 3);
    px(6, 5, 1, 2);
    px(2, 7, 1, 1);
    px(4, 7, 1, 1);
  }

  void drawWingedEnvelope(DisplayDriver& d, int x, int y, int scale) {
    d.setColor(DisplayDriver::LIGHT);
    if (scale <= 1) {
      d.fillRect(x, y, 1, 1);
      return;
    }
    if (scale == 2) {
      drawWingFeathers(d, x + 2, y + 1, scale, false);
      drawWingFeathers(d, x + 6, y + 1, scale, true);
      d.drawRect(x + 2, y + 2, 5, 3);
      d.fillRect(x + 3, y + 3, 1, 1);
      d.fillRect(x + 5, y + 3, 1, 1);
      return;
    }

    if (scale == 3) {
      drawWingFeathers(d, x + 4, y + 1, scale, false);
      drawWingFeathers(d, x + 11, y + 1, scale, true);
      d.drawRect(x + 4, y + 2, 8, 5);
      d.fillRect(x + 5, y + 3, 1, 1);
      d.fillRect(x + 10, y + 3, 1, 1);
      d.fillRect(x + 6, y + 4, 1, 1);
      d.fillRect(x + 9, y + 4, 1, 1);
      d.fillRect(x + 7, y + 5, 2, 1);
      return;
    }

    drawWingFeathers(d, x + 5, y + 2, scale, false);
    drawWingFeathers(d, x + 14, y + 2, scale, true);
    d.drawRect(x + 5, y + 3, 10, 6);
    d.fillRect(x + 6, y + 4, 1, 1);
    d.fillRect(x + 13, y + 4, 1, 1);
    d.fillRect(x + 7, y + 5, 1, 1);
    d.fillRect(x + 12, y + 5, 1, 1);
    d.fillRect(x + 8, y + 6, 1, 1);
    d.fillRect(x + 11, y + 6, 1, 1);
    d.fillRect(x + 9, y + 7, 2, 1);
  }

  // ---- character renderer -----------------------------------------------
  void drawBot(DisplayDriver& d, int cx, int fy,
               bool blink, bool dizzy, bool asleep,
               bool wave_arm, bool happy, int bob) {
    int hy = fy - 30;
    int head_w = 16, head_h = 12;
    int head_x = cx - head_w/2;
    int neck_h = 2;
    int by = hy + head_h + neck_h;
    int body_w = 12, body_h = 10;
    int body_x = cx - body_w/2;
    int ly = by + body_h;
    int arm_w = 3, arm_h = 8, arm_gap = 2;
    int left_arm_x  = body_x - arm_w - arm_gap;
    int right_arm_x = body_x + body_w + arm_gap;
    int arm_y = by + 1;
    int leg_w = 4, leg_h = 7;
    int left_leg_x  = cx - body_w/2 + 1;
    int right_leg_x = cx + 1;

    // Paint a dark 1px silhouette first so the robot stays readable against the mailbox.
    d.setColor(DisplayDriver::DARK);
    d.fillRect(head_x - 1, hy - 1, head_w + 2, head_h + 2);
    d.fillRect(cx - 2, hy + head_h - 1, 4, neck_h + 2);
    d.fillRect(body_x - 1, by - 1, body_w + 2, body_h + 2);
    if (_anim == 0) d.fillRect(left_arm_x - 1, arm_y,     arm_w + 2, arm_h + 2);
    else            d.fillRect(left_arm_x - 1, arm_y + 2, arm_w + 2, arm_h + 2);
    if (wave_arm) {
      d.fillRect(right_arm_x - 1, arm_y - 7, arm_w + 2, arm_h + 2);
      d.fillRect(right_arm_x - 2, arm_y - 9, arm_w + 4, 4);
    } else {
      if (_anim == 0) d.fillRect(right_arm_x - 1, arm_y,     arm_w + 2, arm_h + 2);
      else            d.fillRect(right_arm_x - 1, arm_y + 2, arm_w + 2, arm_h + 2);
    }
    d.fillRect(left_leg_x - 1,  ly - 1 + (bob ? 1 : 0), leg_w + 2, leg_h + 2);
    d.fillRect(right_leg_x - 1, ly - 1 + (bob ? 0 : 1), leg_w + 2, leg_h + 2);

    d.setColor(DisplayDriver::LIGHT);
    d.fillRect(head_x, hy, head_w, head_h);
    d.setColor(DisplayDriver::DARK);
    d.fillRect(head_x + 2, hy + 2, head_w - 4, head_h - 4);
    d.setColor(DisplayDriver::LIGHT);
    d.drawRect(head_x, hy, head_w, head_h);

    if (asleep) {
      d.fillRect(cx - 3, hy + 5, 2, 1);
      d.fillRect(cx + 1, hy + 5, 2, 1);
    } else {
      d.fillRect(cx - 4, hy + 4, 2, 2);
      d.fillRect(cx + 2, hy + 4, 2, 2);
      if (blink) {
        d.setColor(DisplayDriver::DARK);
        d.fillRect(cx - 4, hy + 4, 2, 1);
        d.fillRect(cx + 2, hy + 4, 2, 1);
        d.setColor(DisplayDriver::LIGHT);
      }
    }

    if (wave_arm || _state == PET_MAILOPEN) {
      d.fillRect(cx - 3, hy + 8, 6, 2);
    } else if (happy) {
      // Smile: lift mouth corners slightly to avoid overlapping chin
      d.fillRect(cx - 3, hy + 7, 1, 1);
      d.fillRect(cx + 2, hy + 7, 1, 1);
      d.fillRect(cx - 2, hy + 8, 4, 1);
    } else if (dizzy) {
      d.fillRect(cx - 2, hy + 8, 2, 1);
      d.fillRect(cx + 1, hy + 8, 2, 1);
    } else {
      d.fillRect(cx - 2, hy + 8, 4, 1);
    }

    d.fillRect(cx - 1, hy + head_h, 2, neck_h);

    d.setColor(DisplayDriver::LIGHT);
    d.fillRect(body_x, by, body_w, body_h);
    d.setColor(DisplayDriver::DARK);
    d.fillRect(body_x + 2, by + 2, body_w - 4, body_h - 4);
    d.setColor(DisplayDriver::LIGHT);
    d.drawRect(body_x, by, body_w, body_h);

    if (_anim == 0) d.fillRect(left_arm_x, arm_y + 1, arm_w, arm_h);
    else            d.fillRect(left_arm_x, arm_y + 3, arm_w, arm_h);

    if (wave_arm) {
      d.fillRect(right_arm_x,     arm_y - 6, arm_w, arm_h);
      d.fillRect(right_arm_x - 1, arm_y - 8, arm_w + 2, 2);
    } else {
      if (_anim == 0) d.fillRect(right_arm_x, arm_y + 1, arm_w, arm_h);
      else            d.fillRect(right_arm_x, arm_y + 3, arm_w, arm_h);
    }

    d.fillRect(left_leg_x,  ly + (bob ? 1 : 0), leg_w, leg_h);
    d.fillRect(right_leg_x, ly + (bob ? 0 : 1), leg_w, leg_h);
  }

  // ---- mailbox -----------------------------------------------------------
  void drawMailbox(DisplayDriver& d, bool mail_open) {
    int bw = 14, bh = 10;
    int gy = GROUND_Y;
    int ph = 14;
    int lx = MAILBOX_X - bw/2;
    int by = gy - ph - bh;
    int px = MAILBOX_X;

    d.setColor(DisplayDriver::LIGHT);
    d.fillRect(px - 1, gy - ph, 2, ph);
    d.fillRect(lx, by, bw, bh);
    d.setColor(DisplayDriver::DARK);
    d.fillRect(lx + 1, by + 1, bw - 2, bh - 2);
    d.setColor(DisplayDriver::LIGHT);
    if (mail_open) {
      d.fillRect(lx, by - 3, bw, 3);
    } else {
      d.fillRect(lx + 1, by + 1, bw - 2, 2);
      d.fillRect(lx + bw,     by + 2, 2, 6);
      d.fillRect(lx + bw + 2, by + 2, 4, 4);
    }
    d.fillRect(lx + 2, by + bh/2, bw - 4, 1);
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
    d.fillRect(zx,     zy,     4, 1);
    d.fillRect(zx + 1, zy + 2, 2, 1);
    d.fillRect(zx,     zy + 3, 4, 1);
  }

  // ---- status bar --------------------------------------------------------
  void drawStatusBar(DisplayDriver& d, uint16_t batt_mv) {
    int pct = ((int)batt_mv - BATT_MIN_MILLIVOLTS) * 100 / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
    pct = _tc(pct, 0, 100);

    d.setColor(DisplayDriver::LIGHT);

    // 1. Dynamic horizontal battery
    int bx = 3, by = 3, bw = 14, bh = 8;
    d.drawRect(bx, by, bw, bh);
    d.fillRect(bx+bw, by+2, 2, 4); // positive terminal
    int fill_w = (pct * (bw - 4)) / 100;
    if (fill_w > 0) d.fillRect(bx+2, by+2, fill_w, bh-4);

    bool bt_on = _task && _task->isSerialEnabled();
    bool quiet = _task && _task->isBuzzerQuiet();

    // 2. Bluetooth icon (clean redraw)
    int btx = ICON_BT - 6, bty = 2;
    // spine
    d.fillRect(btx+4, bty,   2, 10);
    // upper right diamond
    d.fillRect(btx+6, bty+1, 2, 2);
    d.fillRect(btx+8, bty+2, 2, 2);
    d.fillRect(btx+6, bty+3, 2, 2);
    // upper left branch
    d.fillRect(btx+2, bty+2, 2, 2);
    // lower right diamond
    d.fillRect(btx+6, bty+5, 2, 2);
    d.fillRect(btx+8, bty+6, 2, 2);
    d.fillRect(btx+6, bty+7, 2, 2);
    // lower left branch
    d.fillRect(btx+2, bty+5, 2, 2);

    if (!bt_on) {
      d.setColor(DisplayDriver::DARK); // erase with slash background
      for(int i=0; i<3; i++) {
        for(int j=0; j<12; j++) d.fillRect(btx+j, bty+11-j+i-1, 1, 1);
      }
      d.setColor(DisplayDriver::LIGHT); // draw slash
      for(int j=0; j<12; j++) d.fillRect(btx+j, bty+11-j, 1, 1);
    }

    // 3. SMS Envelope
    int sx = ICON_SMS - 7, sy = 3;
    d.drawRect(sx, sy, 14, 8);
    d.fillRect(sx+1, sy+1, 1, 1); d.fillRect(sx+12, sy+1, 1, 1);
    d.fillRect(sx+2, sy+2, 1, 1); d.fillRect(sx+11, sy+2, 1, 1);
    d.fillRect(sx+3, sy+3, 1, 1); d.fillRect(sx+10, sy+3, 1, 1);
    d.fillRect(sx+4, sy+4, 1, 1); d.fillRect(sx+9,  sy+4, 1, 1);
    d.fillRect(sx+5, sy+5, 4, 1); // fold tip
    // inner lower flaps
    d.fillRect(sx+1, sy+6, 1, 1); d.fillRect(sx+12, sy+6, 1, 1);
    d.fillRect(sx+2, sy+5, 1, 1); d.fillRect(sx+11, sy+5, 1, 1);

    // 4. Buzzer/Bell (Ring or Mute)
    int zx = ICON_BZ - 5, zy = 2;
    d.fillRect(zx+4, zy,   2, 2); // stem knob
    d.fillRect(zx+3, zy+1, 4, 3);
    d.fillRect(zx+2, zy+3, 6, 2);
    d.fillRect(zx+1, zy+5, 8, 3); // bell body
    d.fillRect(zx+4, zy+8, 2, 2); // clapper
    
    if (quiet) {
      d.setColor(DisplayDriver::DARK);
      for(int i=0; i<3; i++) {
        for(int j=0; j<12; j++) d.fillRect(zx-1+j, bty+10-j+i-1, 1, 1);
      }
      d.setColor(DisplayDriver::LIGHT);
      for(int j=0; j<12; j++) d.fillRect(zx-1+j, bty+10-j, 1, 1);
    } else {
      // right ringing arc
      d.fillRect(zx+10, zy+3, 1, 4); d.fillRect(zx+11, zy+4, 1, 2);
      // left ringing arc
      d.fillRect(zx-1, zy+3, 1, 4); d.fillRect(zx-2, zy+4, 1, 2);
    }

    // 5. Advert / Speaker
    int ax = ICON_ADV - 6, ay = 3;
    d.fillRect(ax+3, ay+3, 3, 2); // speaker neck
    d.fillRect(ax+2, ay+2, 2, 4); // body base
    d.fillRect(ax+1, ay+3, 2, 2);
    d.fillRect(ax+5, ay+1, 2, 6); // speaker cone
    d.fillRect(ax+6, ay,   2, 8); // outer flare
    
    // sound waves
    d.fillRect(ax+10, ay+2, 1, 4); d.fillRect(ax+11, ay+3, 1, 2);
    d.fillRect(ax+13, ay+1, 1, 6); d.fillRect(ax+14, ay+2, 1, 4);

    d.setColor(DisplayDriver::LIGHT);
    const int th = 14;
    d.fillRect(0, th, 128, 1);  // divider
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

    unsigned long anim_ms = (_state == PET_MAILRUN) ? 100 :
                            (_state == PET_WALK)    ? 150 : 350;
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

    if (_state == PET_MAIL_INCOMING) {
      const int duration = 1000;
      unsigned long elapsed = now - _mail_phase_start;
      if (elapsed >= (unsigned long)duration) {
        if (_task) _task->playBuzzer("mail:d=32,o=7,b=180:g,c");
        _mail_run_from_x = _pet_x;
        _mail_phase_start = now;
        _state = PET_MAILRUN;
      }
      return;
    }
    if (_state == PET_MAILRUN) {
      const int target = MAILBOX_X + 14;
      const int duration = 1000;
      unsigned long elapsed = now - _mail_phase_start;
      if (elapsed >= (unsigned long)duration) {
        _pet_x = target;
        openMailView();
      } else {
        _pet_x = _mail_run_from_x + ((target - _mail_run_from_x) * (int)elapsed) / duration;
      }
      return;
    }
    if (_state == PET_MAILOPEN) {
      return;
    }
    if (_state == PET_MAIL_MENU) {
      return;
    }
    if (_state == PET_SMS_SEND_ANIM) {
      const int duration = 1600;
      const int target_x = MAILBOX_X - 4;
      const int target_y = 34;
      unsigned long elapsed = now - _sms_send_anim_start;
      if (elapsed >= (unsigned long)duration) {
        _sms_env_x = target_x;
        _sms_env_y = target_y;
        dispatchSelectedQuickMessage();
        _sms_direct_reply = false;
        _state = PET_IDLE;
        return;
      }

      int t = (int)((elapsed * 128) / duration);
      _sms_env_x = _sms_env_start_x + ((target_x - _sms_env_start_x) * t) / 128;
      _sms_env_y = _sms_env_start_y + ((target_y - _sms_env_start_y) * t) / 128 - ((64 - abs(64 - t)) / 5);
      return;
    }
    if (_state == PET_SMS_SEND_DROP) {
      const int drop_ms = 260;
      const int ground_y = GROUND_Y - 7;
      unsigned long elapsed = now - _sms_send_drop_start;
      if (elapsed >= (unsigned long)drop_ms) {
        _sms_env_y = ground_y;
        if (elapsed >= (unsigned long)(drop_ms + 500)) {
          _sms_direct_reply = false;
          if (_task) _task->showAlert("Canceled", 800);
          _state = PET_IDLE;
        }
        return;
      }

      _sms_env_y = _sms_env_start_y + ((ground_y - _sms_env_start_y) * (int)elapsed) / drop_ms;
      return;
    }

    if (_walk_dir != 0) {
      int old_walk_dir = _walk_dir;
      _pet_x += _walk_dir * 9;
      if (_walk_dir > 0 && _pet_x >= _target_x) { _pet_x = _target_x; _walk_dir = 0; }
      else if (_walk_dir < 0 && _pet_x <= _target_x) { _pet_x = _target_x; _walk_dir = 0; }
      _pet_x = _tc(_pet_x, PET_X_MIN, PET_X_MAX);
      if (old_walk_dir != 0 && _walk_dir == 0 && _pending_nav_beep) {
        playMenuBeep();
        _pending_nav_beep = false;
      }
    }

    // Jump physics: every 50 ms
    if ((_jump_y > 0 || _jump_vy > 0) && now - _jump_last_physics >= 50) {
      _jump_y  += _jump_vy;
      _jump_vy -= 2;    // gravity
      // Apply ceiling cap
      int cap = _high_jump ? MAX_JUMP_HI : MAX_JUMP_LO;
      if (_jump_y > cap) { _jump_y = cap; if (_jump_vy > 0) _jump_vy = 0; }
      if (_jump_y <= 0)  { _jump_y = 0; _jump_vy = 0; _landed = true; _high_jump = false; }
      _jump_last_physics = now;
    }

    int bpct = ((int)batt_mv - BATT_MIN_MILLIVOLTS) * 100
               / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
    bpct = _tc(bpct, 0, 100);

    if (_state == PET_SMS_RECIPIENT || _state == PET_SMS_SELECT ||
        _state == PET_SMS_SEND_ANIM || _state == PET_SMS_SEND_DROP ||
        _state == PET_MAIL_INCOMING || _state == PET_MAIL_MENU ||
        _state == PET_MAILRUN || _state == PET_MAILOPEN) {
      return; // Do not override active interaction states
    }

    if      (bpct <= 20)        _state = PET_SLEEP;
    else if (_stress >= 75)     _state = PET_DIZZY;
    else if (_stress >= 40)     _state = PET_SWEAT;
    else if (now < _wave_until) _state = PET_WAVE;
    else if (_jump_y > 0)       _state = PET_JUMP;
    else if (_walk_dir != 0)    _state = PET_WALK;
    else                        _state = PET_IDLE;
  }

  void playMenuBeep() {
    if (_task) _task->playBuzzer("nav:d=32,o=6,b=240:e");
  }

  void renderPixelFrame(DisplayDriver& d, int x, int y, int w, int h) {
    // Dark filled rounded rect (r≈3 staircase) — serves as 2px dark outer ring + BG clear
    d.setColor(DisplayDriver::DARK);
    d.fillRect(x+3, y,     w-6, 1);
    d.fillRect(x+1, y+1,   w-2, 1);
    d.fillRect(x,   y+2,   w,   h-4);
    d.fillRect(x+1, y+h-2, w-2, 1);
    d.fillRect(x+3, y+h-1, w-6, 1);
    // 1px LIGHT rounded border ring at 2px inset (r≈1)
    d.setColor(DisplayDriver::LIGHT);
    d.fillRect(x+3,   y+2,   w-6, 1);   // top
    d.fillRect(x+2,   y+3,   1,   h-6); // left
    d.fillRect(x+w-3, y+3,   1,   h-6); // right
    d.fillRect(x+3,   y+h-3, w-6, 1);  // bottom
  }

  void renderMailActions(DisplayDriver& d) {
    int bx = 20, by = 39, bw = 88, bh = 18;
    renderPixelFrame(d, bx, by, bw, bh);
    d.setTextSize(1);
    if (_mail_action_idx == 0) {
      d.setColor(DisplayDriver::LIGHT);
      d.fillRect(bx + 6, by + 4, 32, 10);
      d.setColor(DisplayDriver::DARK);
      d.drawTextCentered(bx + 22, by + 6, "Reply");
      d.setColor(DisplayDriver::LIGHT);
      d.drawTextCentered(bx + 64, by + 6, "Back");
    } else {
      d.setColor(DisplayDriver::LIGHT);
      d.fillRect(bx + 50, by + 4, 28, 10);
      d.setColor(DisplayDriver::LIGHT);
      d.drawTextCentered(bx + 22, by + 6, "Reply");
      d.setColor(DisplayDriver::DARK);
      d.drawTextCentered(bx + 64, by + 6, "Back");
      d.setColor(DisplayDriver::LIGHT);
    }
  }

  // ---- mail full-screen renderer -----------------------------------------
  void renderMailOpen(DisplayDriver& d, unsigned long now) {
    unsigned long elapsed = now - _mail_phase_start;
    int env_h = (int)(elapsed / 16);
    if (env_h > 46) env_h = 46;

    int ew = 114;
    int ex = (128 - ew) / 2;
    int ey = (64 - env_h) / 2;

    renderPixelFrame(d, ex, ey, ew, env_h);

    if (env_h < 18) return;

    d.setColor(DisplayDriver::DARK);
    for (int i = 0; i < ew / 2; i++) {
      int depth = i / 8; if (depth > 7) depth = 7;
      d.fillRect(ex + 2 + i,      ey + 3, 1, depth);
      d.fillRect(ex + ew - 3 - i, ey + 3, 1, depth);
    }

    if (env_h < 36) return;

    d.setColor(DisplayDriver::LIGHT);
    d.setTextSize(1);
    char hdr[40];
    snprintf(hdr, sizeof(hdr), "< %s >", _mail_from);
    d.drawTextCentered(64, ey + 5, hdr);
    if (_mail_history_count > 1) {
      char idx[12];
      snprintf(idx, sizeof(idx), "%d/%d", _mail_view_idx + 1, _mail_history_count);
      d.drawTextRightAlign(ex + ew - 5, ey + 5, idx);
    }

    for (int x = ex + 4; x < ex + ew - 4; x += 4)
      d.fillRect(x, ey + 14, 2, 1);

    d.setColor(DisplayDriver::LIGHT);
    d.setCursor(ex + 7, ey + 18);
    d.printWordWrap(_mail_text, ew - 16);
  }

public:
  explicit TamagotchiScreen(UITask* task)
    : _task(task), _state(PET_IDLE), _anim(0),
      _next_anim(0), _wave_until(0),
      _blink(false), _next_blink(0),
      _stress(0), _last_decay(0),
      _mail_history_count(0), _mail_view_idx(0),
      _mail_phase_start(0), _mail_dismiss_at(0), _mail_action_idx(0),
      _sms_msg_idx(0), _sms_rcpt_idx(0), _sms_rcpt_count(0),
        _bars(0), _next_bar_decay(0), _last_walk_input(0), _last_nav_key(0),
      _pending_nav_beep(false),
      _sms_nav_dir(0), _sms_nav_anim_until(0),
      _jump_y(0), _jump_vy(0), _jump_last_physics(0),
      _landed(false), _high_jump(false), _last_keypress_ms(0)
  {
    _pet_x    = 60;
    _target_x = 60;
    _walk_dir = 0;
    memset(_mail_from, 0, sizeof(_mail_from));
    memset(_mail_text, 0, sizeof(_mail_text));
    memset(_mail_history_from, 0, sizeof(_mail_history_from));
    memset(_mail_history_text, 0, sizeof(_mail_history_text));
    memset(_sms_rcpt_names, 0, sizeof(_sms_rcpt_names));
    memset(_sms_rcpt_contact_idx, 0, sizeof(_sms_rcpt_contact_idx));
    memset(_pending_icon, 0, sizeof(_pending_icon));
    _recip_idx = 0;
    _recip_count = 0;
    _sms_nav_dir = 0;
    _sms_nav_anim_until = 0;
  }

  // ---- event hooks -------------------------------------------------------
  void onHandshake() { _wave_until = millis() + 3000; onMeshActivity(15); }

  void onMeshActivity(int delta = 8) {
    _stress = _tc(_stress + delta, 0, 100);
    if (_bars < 5) _bars++;
    _next_bar_decay = millis() + 5000;
  }

  void onNewMessage(const char* from_name, const char* text) {
    if (_state == PET_MAIL_INCOMING || _state == PET_MAILRUN || _state == PET_MAILOPEN) return;
    pushMail(from_name, text);
    _jump_y = 0; _jump_vy = 0;
    _state = PET_MAIL_INCOMING;
    _mail_phase_start = millis();
    onMeshActivity(10);
  }

  PetState getState() const { return _state; }

  // ---- UIScreen ----------------------------------------------------------
  bool handleInput(char c) override {
    unsigned long now = millis();

    auto ignoreRepeatedNav = [&](char key, unsigned long guard_ms) -> bool {
      if (_last_nav_key == key && now - _last_walk_input < guard_ms) return true;
      _last_nav_key = key;
      _last_walk_input = now;
      return false;
    };

    auto markNonNav = [&]() {
      _last_nav_key = 0;
    };

    if (_state == PET_MAILOPEN) {
      if (c == KEY_LEFT || c == KEY_RIGHT) {
        if (ignoreRepeatedNav(c, 80)) return true;
        if (_mail_history_count > 1) {
          if (c == KEY_LEFT) _mail_view_idx = (_mail_view_idx + _mail_history_count - 1) % _mail_history_count;
          else               _mail_view_idx = (_mail_view_idx + 1) % _mail_history_count;
          loadViewedMail();
        }
        playMenuBeep();
        return true;
      }
      if (c == KEY_PREV || c == KEY_NEXT) {
        markNonNav();
        closeMailView();
        return true;
      }
      if (c == KEY_ENTER) {
        if (now - _last_keypress_ms < 120) return true;
        _last_keypress_ms = now;
        markNonNav();
        _mail_action_idx = 0;
        _state = PET_MAIL_MENU;
        playMenuBeep();
        return true;
      }
      return true;
    }

    if (_state == PET_MAIL_MENU) {
      if (c == KEY_LEFT || c == KEY_RIGHT) {
        if (ignoreRepeatedNav(c, 80)) return true;
        _mail_action_idx ^= 1;
        playMenuBeep();
        return true;
      }
      if (c == KEY_PREV || c == KEY_NEXT) {
        markNonNav();
        _state = PET_MAILOPEN;
        return true;
      }
      if (c == KEY_ENTER) {
        if (now - _last_keypress_ms < 120) return true;
        _last_keypress_ms = now;
        markNonNav();
        if (_mail_action_idx == 0) {
          openReplyComposer();
        } else {
          closeMailView();
        }
        return true;
      }
      return true;
    }

    // SMS recipient selection (phase 1)
    if (_state == PET_SMS_RECIPIENT) {
      if (_recip_count <= 0) {
        if (c == KEY_PREV || c == KEY_NEXT) { markNonNav(); _state = PET_IDLE; return true; }
        return false;
      }
      if (c == KEY_LEFT) {
        if (ignoreRepeatedNav(KEY_LEFT, 80)) return true;
        _recip_idx = (_recip_idx + _recip_count - 1) % _recip_count;
        _sms_nav_dir = KEY_LEFT;
        _sms_nav_anim_until = now + 120;
        playMenuBeep();
        return true;
      }
      if (c == KEY_RIGHT) {
        if (ignoreRepeatedNav(KEY_RIGHT, 80)) return true;
        _recip_idx = (_recip_idx + 1) % _recip_count;
        _sms_nav_dir = KEY_RIGHT;
        _sms_nav_anim_until = now + 120;
        playMenuBeep();
        return true;
      }
      if (c == KEY_ENTER) {
        if (now - _last_keypress_ms < 120) return true;
        _last_keypress_ms = now;
        markNonNav();
        // Determine whether selected item is a channel or a contact
        int ch_count = 0;
#ifdef MAX_GROUP_CHANNELS
        ChannelDetails cdtmp;
        for (int i = 0; i < MAX_GROUP_CHANNELS; i++) if (the_mesh.getChannel(i, cdtmp) && cdtmp.name[0]) ch_count++;
#endif
        if (_recip_idx < ch_count) {
          _reply_to_channel = true;
          _reply_channel_idx = _recip_idx;
        } else {
          _reply_to_channel = false;
          _sms_rcpt_idx = _sms_rcpt_contact_idx[_recip_idx - ch_count];
        }
        _sms_direct_reply = false;
        _sms_msg_idx = 0;
        _state = PET_SMS_SELECT;
        playMenuBeep();
        return true;
      }
      if (c == KEY_PREV || c == KEY_NEXT) { markNonNav(); _state = PET_IDLE; return true; }
      return false;
    }

    // SMS message selection (phase 2)
    if (_state == PET_SMS_SELECT) {
      if (c == KEY_LEFT) {
        if (ignoreRepeatedNav(KEY_LEFT, 80)) return true;
        _sms_msg_idx = (_sms_msg_idx + _quick_msg_count - 1) % _quick_msg_count;
        _sms_nav_dir = KEY_LEFT;
        _sms_nav_anim_until = now + 120;
        playMenuBeep();
        return true;
      }
      if (c == KEY_RIGHT) {
        if (ignoreRepeatedNav(KEY_RIGHT, 80)) return true;
        _sms_msg_idx = (_sms_msg_idx + 1) % _quick_msg_count;
        _sms_nav_dir = KEY_RIGHT;
        _sms_nav_anim_until = now + 120;
        playMenuBeep();
        return true;
      }
      if (c == KEY_ENTER) {
        if (now - _last_keypress_ms < 120) return true;
        _last_keypress_ms = now;
        markNonNav();
        startSmsSendAnimation();
        return true;
      }
      if (c == KEY_PREV) {
        markNonNav();
        _state = _sms_direct_reply ? PET_MAIL_MENU : PET_SMS_RECIPIENT;
        return true;
      }
      if (c == KEY_NEXT)  { markNonNav(); _sms_direct_reply = false; _state = PET_IDLE; return true; }
      return false;
    }

    if (_state == PET_SMS_SEND_ANIM) {
      if (c == KEY_PREV) {
        markNonNav();
        _sms_send_drop_start = now;
        _sms_env_start_y = _sms_env_y;
        _state = PET_SMS_SEND_DROP;
        return true;
      }
      return true;
    }

    if (_state == PET_SMS_SEND_DROP) {
      if (c == KEY_PREV) return true;
      return true;
    }

    // Jump: short press = low arc, long press (>=350ms) = high arc
    if (c == KEY_ENTER && _jump_y == 0) {
      if (hasMail() && abs(_pet_x - PET_X_MIN) <= 4) {
        if (now - _last_keypress_ms < 120) return true;
        _last_keypress_ms = now;
        markNonNav();
        openMailView();
        return true;
      }
      if (now - _last_keypress_ms < 100) return true;
      _last_keypress_ms = now;
      markNonNav();
      unsigned long dur = _task ? _task->getEncoderPressDuration() : 0;
      _high_jump = (dur >= 220);
      _jump_vy   = _high_jump ? 18 : 8;
      _jump_last_physics = millis();
      if (_task) {
        _task->playBuzzer(_high_jump ? "jumpL:d=32,o=6,b=150:c,e,g" : "jumpS:d=32,o=6,b=200:e");
      }
      return true;
    }

    const int stops[] = {22, 33, 60, 87, 114};
    int num_stops = 5;

    if (c == KEY_LEFT) {
      if (ignoreRepeatedNav(KEY_LEFT, 70)) return true;
      for (int i = num_stops - 1; i >= 0; i--) {
        if (stops[i] < _target_x - 4) {
          _target_x = stops[i];
          _walk_dir = -1;
          _pending_nav_beep = true;
          break;
        }
      }
      return true;
    }
    if (c == KEY_RIGHT) {
      if (ignoreRepeatedNav(KEY_RIGHT, 70)) return true;
      for (int i = 0; i < num_stops; i++) {
        if (stops[i] > _target_x + 4) {
          _target_x = stops[i];
          _walk_dir = 1;
          _pending_nav_beep = true;
          break;
        }
      }
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
    display.fillRect(0, GROUND_Y, 128, 1);

    int cx = _pet_x;
    int fy = FEET_Y - _jump_y;
    int head_top = fy - 30;

    // While in high jump and head inside icon zone: record pending icon
    if (_high_jump && _jump_y > 0 && head_top <= 14 && _pending_icon[0] == 0) {
      int nearest = -1, best_d = 15;
      int centres[] = {ICON_ADV, ICON_SMS, ICON_BT, ICON_BZ};
      for (int i = 0; i < 4; i++) {
        int dist = abs(cx - centres[i]);
        if (dist < best_d) { best_d = dist; nearest = i; }
      }
      static const char* names[] = {"ADV","SMS","BT","BZ"};
      if (nearest >= 0) strncpy(_pending_icon, names[nearest], sizeof(_pending_icon)-1);
    }

    // Execute deferred icon action on landing
    if (_landed && _pending_icon[0]) {
      if (strcmp(_pending_icon, "BT") == 0 && _task) {
        if (_task->isSerialEnabled()) _task->disableSerial();
        else _task->enableSerial();
        _task->showAlert(_task->isSerialEnabled() ? "BT On" : "BT Off", 800);
      } else if (strcmp(_pending_icon, "SMS") == 0) {
        openSmsRecipientChooser();
      } else if (strcmp(_pending_icon, "BZ") == 0 && _task) {
        _task->toggleBuzzer();
      } else if (strcmp(_pending_icon, "ADV") == 0 && _task) {
        if (the_mesh.advert()) _task->showAlert("Advert sent", 1000);
        else                   _task->showAlert("Advert fail", 1000);
      }
      _pending_icon[0] = 0;
    }

    // ---- SMS panels ----
    if (_state == PET_SMS_RECIPIENT || _state == PET_SMS_SELECT) {
      bool is_rcpt = (_state == PET_SMS_RECIPIENT);
      int ew = 114;
      int env_h = is_rcpt ? 28 : 44;
      int ex = 7, ey = is_rcpt ? 18 : 10;
      int hdr_y = is_rcpt ? ey + 5 : 16;
      int content_y = is_rcpt ? ey + 14 : 33;
      int arrow_y = is_rcpt ? ey + 12 : 31;
      renderPixelFrame(display, ex, ey, ew, env_h);

      // compute channel vs contact counts
      int ch_count = getChannelCount();

      // Header (no background shading)
      display.setColor(DisplayDriver::LIGHT);
      display.setTextSize(1);
      char hdr[40];
      if (is_rcpt) snprintf(hdr, sizeof(hdr), "TO: %d/%d", _recip_idx + 1, _recip_count);
      else {
        if (_reply_to_channel) {
          ChannelDetails chd; if (the_mesh.getChannel(_reply_channel_idx, chd)) snprintf(hdr, sizeof(hdr), "MSG to %s", chd.name); else snprintf(hdr, sizeof(hdr), "MSG to CH#%d", _reply_channel_idx);
        } else {
          ContactInfo ci; if (the_mesh.getContactByIdx(_sms_rcpt_idx, ci)) snprintf(hdr, sizeof(hdr), "MSG to %s", ci.name); else snprintf(hdr, sizeof(hdr), "MSG");
        }
      }
      display.drawTextCentered(64, hdr_y, hdr);

      display.setColor(DisplayDriver::LIGHT);

      // Content Row (no background shading)
      display.setColor(DisplayDriver::DARK);
      display.setTextSize(1);
      if (is_rcpt && _recip_count == 0) {
        display.setColor(DisplayDriver::LIGHT);
        display.drawTextCentered(64, content_y, "Empty");
      } else {
        char cur[40];
        if (is_rcpt) {
          if (_recip_idx < ch_count) {
            ChannelDetails chd; if (the_mesh.getChannel(_recip_idx, chd)) strncpy(cur, chd.name, sizeof(cur)-1);
            else strncpy(cur, "Channel", sizeof(cur)-1);
          } else {
            int contact_slot = _recip_idx - ch_count;
            ContactInfo ci; if (contact_slot >= 0 && contact_slot < _sms_rcpt_count && the_mesh.getContactByIdx(_sms_rcpt_contact_idx[contact_slot], ci)) strncpy(cur, ci.name, sizeof(cur)-1);
            else strncpy(cur, "Contact", sizeof(cur)-1);
          }
          cur[sizeof(cur)-1] = 0;
          display.setColor(DisplayDriver::LIGHT);
          display.drawTextCentered(64, content_y, cur);
        } else {
          display.setColor(DisplayDriver::LIGHT);
          display.drawTextCentered(64, content_y, _quick_msgs[_sms_msg_idx]);
        }
      }
      
      display.setColor(DisplayDriver::LIGHT);
      
      // Nokia style scroll arrows with pressed-state
      bool leftPressed  = (_sms_nav_dir == KEY_LEFT && now < _sms_nav_anim_until);
      bool rightPressed = (_sms_nav_dir == KEY_RIGHT && now < _sms_nav_anim_until);
      display.setColor(leftPressed ? DisplayDriver::DARK : DisplayDriver::LIGHT);
      // Left Arrow
      display.fillRect(12, arrow_y + 4, 2, 2);
      display.fillRect(13, arrow_y + 2, 1, 6);
      display.fillRect(14, arrow_y, 1, 10);
      display.setColor(rightPressed ? DisplayDriver::DARK : DisplayDriver::LIGHT);
      // Right Arrow
      display.fillRect(114, arrow_y + 4, 2, 2);
      display.fillRect(114, arrow_y + 2, 1, 6);
      display.fillRect(113, arrow_y, 1, 10);
      display.setColor(DisplayDriver::LIGHT);
      
      return 100;
    }

    if (_state == PET_SMS_SEND_ANIM || _state == PET_SMS_SEND_DROP) {
      drawMailbox(display, true);
      drawBot(display, cx, FEET_Y, false, false, false, false, false, 0);
      drawWingedEnvelope(display, _sms_env_x, _sms_env_y, 4);
      return 60;
    }

    if (_state == PET_MAIL_INCOMING) {
      unsigned long elapsed = now - _mail_phase_start;
      const int duration = 1000;
      int t = (elapsed >= (unsigned long)duration) ? 128 : (int)((elapsed * 128) / duration);
      int env_x = 1 + (t / 24);
      int env_y = (t < 72) ? (8 + (t / 8)) : (17 + ((34 - 17) * (t - 72)) / 56);
      int scale = (t < 18) ? 1 : ((t < 48) ? 2 : ((t < 82) ? 3 : 4));
      drawMailbox(display, false);
      drawBot(display, cx, fy, _blink, false, false, false, false, 0);
      drawWingedEnvelope(display, env_x, env_y, scale);
      return 60;
    }

    // ---- MAILOPEN ----
    if (_state == PET_MAILOPEN || _state == PET_MAIL_MENU) {
      drawMailbox(display, true);
      drawBot(display, cx, FEET_Y, false, false, false, true, false, 0);
      renderMailOpen(display, now);
      if (_state == PET_MAIL_MENU) renderMailActions(display);
      return 100;
    }

    // ---- SLEEP ----
    if (_state == PET_SLEEP) {
      int sy = GROUND_Y - 10;
      display.setColor(DisplayDriver::LIGHT);
      display.fillRect(cx - 14, sy,     20, 6);
      display.fillRect(cx + 6,  sy - 5,  8, 6);
      display.setColor(DisplayDriver::DARK);
      display.fillRect(cx + 7,  sy - 4,  6, 4);
      display.setColor(DisplayDriver::LIGHT);
      display.drawRect(cx + 6,  sy - 5,  8, 6);
      display.fillRect(cx + 8,  sy - 3,  4, 1);
      display.fillRect(cx - 18, sy + 1,  5, 3);
      display.fillRect(cx - 18, sy + 5,  5, 3);
      drawZzz(display, cx, sy - 5);
    } else {
      display.setColor(DisplayDriver::LIGHT);
      drawMailbox(display, abs(cx - MAILBOX_X) <= 18);

      bool wave  = (_state == PET_WAVE);
      bool sweat = (_state == PET_SWEAT);
      bool dizzy = (_state == PET_DIZZY);
      bool happy = (_state == PET_MAILRUN);
      int  bob = _landed ? 2 :
                 ((_state == PET_IDLE || _state == PET_WALK ||
                   _state == PET_MAILRUN) && _anim == 1) ? 1 : 0;
      if (_landed) _landed = false;

      drawBot(display, cx, fy, _blink, dizzy, false, wave, happy, bob);
      if (dizzy) { display.setColor(DisplayDriver::LIGHT); drawDizzy(display, cx, fy - 27); }
      if (sweat) { display.setColor(DisplayDriver::LIGHT); drawSweat(display, cx, fy - 27); }
    }

    return 150;
  }
};
