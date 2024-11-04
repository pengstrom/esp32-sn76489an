#include "snplayer.h"
#include <esp_timer.h>
#include <math.h>

const SnPlayer::note_config_t SnPlayer::NOTE_CONFIG_DEFAULT = {};
const Sn76489an::Tone SnPlayer::VOICE_TONE[3] = {Sn76489an::Tone1, Sn76489an::Tone2, Sn76489an::Tone3};

SnPlayer::SnPlayer(Sn76489an::config_t cfg)
    : Sn76489an(cfg)
{
}

SnPlayer::~SnPlayer()
{
}

void SnPlayer::begin()
{
  printf("SnPlayer begin...\n");
  Sn76489an::begin();

  _state.last_us = esp_timer_get_time();

  esp_timer_handle_t timer;
  esp_timer_create_args_t cfg{
      .callback = player_timer_intr,
      .arg = this,
      .name = "player_intr",
      .skip_unhandled_events = true,
  };
  ESP_ERROR_CHECK(esp_timer_create(&cfg, &timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000));

  // xTaskCreate(player_task, "player_task", 4096, this, 15, NULL);

  printf("SnPlayer begun!\n");
}

void SnPlayer::end()
{
  Sn76489an::end();
}

void SnPlayer::playNote(float fq, uint32_t dur, note_config_t cfg, int n)
{
  voice_state_t *available = nullptr;
  if (0 <= n && n < 3)
  {
    available = &_state.tones[n];
    available->osc = VOICE_TONE[n];
  }
  else
  {
    for (size_t i = 0; i < 3; i++)
    {
      if (!_state.tones[i].playing)
      {
        available = &_state.tones[i];
        available->osc = VOICE_TONE[i];
        break;
      }
    }
  }

  if (available == nullptr)
  {
    // sad...
    return;
  }

  available->cfg = cfg;
  if (cfg.envelope.attack >= dur)
  {
    available->env_state.attack_left = dur;
    dur = 0;
  }
  else
  {
    available->env_state.attack_left = cfg.envelope.attack;
    dur -= cfg.envelope.attack;
  }
  if (cfg.envelope.decay >= dur)
  {
    available->env_state.decay_left = dur;
    dur = 0;
  }
  else
  {
    available->env_state.decay_left = cfg.envelope.decay;
    dur -= cfg.envelope.decay;
  }
  available->env_state.sustain_left = dur;
  available->env_state.release_left = cfg.envelope.release;

  available->fq = fq;
  available->orig_dur = dur;

  available->playing = true;

  available->played = 0;
}

void SnPlayer::player_timer_intr(void *arg)
{
  // Update voice states
  SnPlayer *self = (SnPlayer *)arg;
  self->updatePlayerState();
  self->handlePlayer();
}

void SnPlayer::handlePlayer()
{
  for (size_t i = 0; i < 3; i++)
  {
    voice_state_t &tone = _state.tones[i];

    if (!tone.playing)
    {
      // Nothing to play
      return;
    }

    float f_vib = handleVibrato(tone);
    handleTremolo(tone, f_vib);

    handleEnvelope(tone);
  }
}

void SnPlayer::handleEnvelope(SnPlayer::voice_state_t tone)
{
  uint8_t att = ATTENUATION_SILENCE;
  if (tone.env_state.attack_left > 0)
  {
    // In attack
    uint32_t orig = tone.cfg.envelope.attack;
    uint32_t left = tone.env_state.attack_left;
    att = (ATTENUATION_SILENCE * left) / orig; // Everything is backwards
  }
  else if (tone.env_state.decay_left > 0)
  {
    // In decay
    uint32_t sus = tone.cfg.envelope.sustain;
    uint32_t orig = tone.cfg.envelope.decay;
    uint32_t left = tone.env_state.decay_left;
    att = (sus * (orig - left)) / orig;
  }
  else if (tone.env_state.sustain_left > 0)
  {
    // Sustain
    att = tone.cfg.envelope.sustain;
  }
  else if (tone.env_state.release_left > 0)
  {
    // In release
    uint32_t orig = tone.cfg.envelope.release;
    uint32_t left = tone.env_state.release_left;
    uint32_t sus = tone.cfg.envelope.sustain;
    att = sus + ((ATTENUATION_SILENCE - sus) * (orig - left)) / orig;
    if (att == ATTENUATION_SILENCE)
    {
      // Release resource if silent
      tone.playing = false;
    }
  }
  setToneAttenuation(tone.osc, att);
}

float SnPlayer::handleVibrato(SnPlayer::voice_state_t tone)
{
  float rise_amp = 0.0f;
  uint32_t played = tone.played;
  uint32_t delay = tone.cfg.vibr.vibr_delay;
  uint32_t rise = tone.cfg.vibr.vibr_rise;
  if (played > delay + rise)
  {
    // Full vibrato
    rise_amp = 1.0f;
  }
  else if (played > delay)
  {
    // Partial vibrato
    rise_amp = (played - delay) / float(rise);
  }

  uint32_t vib_period = tone.cfg.vibr.vibr_period;
  float vib = rise_amp * tone.cfg.vibr.vibr_amp * (played % vib_period > (vib_period / 2) ? 1 : -1);

  float fq = tone.fq + vib;
  setTone(tone.osc, fq);

  return fq;
}

void SnPlayer::updatePlayerState()
{
  uint64_t now_us = esp_timer_get_time();
  int32_t elapsed_us = now_us - _state.last_us;

  for (size_t i = 0; i < 3; i++)
  {
    int32_t to_consume = elapsed_us / 1000;
    voice_state_t &voice = _state.tones[i];

    updateVoice(voice, to_consume);
  }

  _state.last_us = now_us;
}

void SnPlayer::updateVoice(SnPlayer::voice_state_t &voice, int32_t &to_consume)
{
  voice.played += to_consume;

  uint32_t *counters[] = {&voice.env_state.attack_left, &voice.env_state.decay_left, &voice.env_state.sustain_left, &voice.env_state.release_left};
  for (size_t j = 0; j < sizeof(counters) / sizeof(uint32_t *); j++)
  {
    uint32_t &counter = *counters[j];
    if (counter >= to_consume)
    {
      counter -= to_consume;
      to_consume = 0;
    }
    else
    {
      to_consume -= counter;
      counter = 0;
    }
  }

  if (to_consume > 0)
  {
    voice.playing = false;
  }
}

void SnPlayer::handleTremolo(voice_state_t voice, float fq_vib)
{
  Tone osc = voice.osc;
  uint32_t played = voice.played;
  uint32_t period = voice.cfg.trem.period;
  uint16_t mask = voice.cfg.trem.bit_mask;
  uint16_t fq = get_freq(fq_vib);

  if (played % period > (period / 2))
  {
    fq |= mask;
  }
  else
  {
    fq &= ~mask;
  }

  writeReg(osc);
  writeFreqLsb(fq);
  send();

  writeFreqMsb(fq);
  send();
}
