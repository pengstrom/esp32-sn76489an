#if !defined(_SNPLAYER_H)
#define _SNPLAYER_H

#include <sn76489an.h>

class SnPlayer : public Sn76489an
{
public:
  struct note_envelope_t
  {
    uint32_t attack = 10;
    uint32_t decay = 10;
    uint8_t sustain = 255;
    uint32_t release = 10;
  };

  struct note_vibrato_t
  {
    uint32_t vibr_delay = 100;
    float vibr_amp = 0;
    uint32_t vibr_rise = 500;
    uint32_t vibr_period = 150;
  };

  struct note_tremolo_t
  {
    uint32_t period = 150;
    uint16_t bit_mask = 0;
  };

  struct note_config_t
  {
    note_envelope_t envelope;
    note_vibrato_t vibr;
    note_tremolo_t trem;
  };

  static const note_config_t NOTE_CONFIG_DEFAULT;

  SnPlayer(Sn76489an::config_t cfg);
  ~SnPlayer();

  void begin();
  void end();

  void playNote(float fq, uint32_t dur, note_config_t cfg = NOTE_CONFIG_DEFAULT, int n = -1);

private:
  static const TickType_t PLAYER_DELAY_US = 500;
  static const Tone VOICE_TONE[3];

  struct envelope_state_t
  {
    uint32_t attack_left = 0;
    uint32_t decay_left = 0;
    uint32_t sustain_left = 0;
    uint32_t release_left = 0;
  };

  struct voice_state_t
  {
    note_config_t cfg = {};
    envelope_state_t env_state = {};
    float fq = 0.0f;
    uint32_t played = 0;
    uint32_t orig_dur = 0;
    Tone osc = Tone1;
    bool playing = false;
  };

  struct noise_state_t
  {
    bool playing = false;
  };

  struct player_state_t
  {
    uint64_t last_us = 0;
    voice_state_t tones[3];
    noise_state_t noise = {};
  };

  static void player_timer_intr(void *arg);

  player_state_t _state = {};

  BaseType_t _player_was_delayed;
  TickType_t _last_player_wake;

  void handlePlayer();
  void updatePlayerState();
  void updateVoice(SnPlayer::voice_state_t &voice, int32_t &to_consume);

  void handleEnvelope(SnPlayer::voice_state_t tone);
  float handleVibrato(SnPlayer::voice_state_t tone);
  void handleTremolo(voice_state_t voice, float fq);
};

#endif // _SNPLAYER_H
