#include "sn76489an.h"
#include <math.h>
#include <soc/io_mux_reg.h>
#include <driver/ledc.h>
#include <rom/ets_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <../../include/config.h>

uint16_t Sn76489an::get_freq(float tone_fq)
{
  tone_fq = std::max(MIN_FREQ, std::min(MAX_FREQ, tone_fq));
  uint16_t n = std::round(Config::MASTER_CLOCK_FREQ_HZ / (32.0f * tone_fq));
  return n & 0b1111111111;
}

Sn76489an::Sn76489an(config_t cfg)
    // : _clock_hz(cfg.clock_hz),
    // _clk_pin(cfg.clk_pin),
    : _we_pin(cfg.we_pin),
      _ce_pin(cfg.ce_pin),
      // _ready_pin(cfg.ready_pin),
      _d0_pin(cfg.d0_pin),
      _d1_pin(cfg.d1_pin),
      _d2_pin(cfg.d2_pin),
      _d3_pin(cfg.d3_pin),
      _d4_pin(cfg.d4_pin),
      _d5_pin(cfg.d5_pin),
      _d6_pin(cfg.d6_pin),
      _d7_pin(cfg.d7_pin)
{
  printf("Chip init...\n");
  // for convenience
  _data_pins[0] = &_d0_pin;
  _data_pins[1] = &_d1_pin;
  _data_pins[2] = &_d2_pin;
  _data_pins[3] = &_d3_pin;
  _data_pins[4] = &_d4_pin;
  _data_pins[5] = &_d5_pin;
  _data_pins[6] = &_d6_pin;
  _data_pins[7] = &_d7_pin;

  assert(GPIO_IS_VALID_OUTPUT_GPIO(Config::MASTER_CLOCK_PIN));

  setupOutputPin(_we_pin);
  disableWrite();

  setupOutputPin(_ce_pin);
  deselectChip();

  // ESP_ERROR_CHECK(gpio_reset_pin(_ready_pin));
  // ESP_ERROR_CHECK(gpio_set_direction(_ready_pin, GPIO_MODE_INPUT));
  // ESP_ERROR_CHECK(gpio_set_pull_mode(_ready_pin, GPIO_PULLUP_ONLY));

  for (size_t i = 0; i < DATA_PIN_COUNT; i++)
  {
    setupOutputPin(*_data_pins[i]);
  }
  zeroData();
  printf("Chip init done!\n");
}

Sn76489an::~Sn76489an()
{
  ESP_ERROR_CHECK(gpio_reset_pin(_we_pin));
  ESP_ERROR_CHECK(gpio_reset_pin(_ce_pin));
  // ESP_ERROR_CHECK(gpio_reset_pin(_ready_pin));
  for (size_t i = 0; i < DATA_PIN_COUNT; i++)
  {
    gpio_num_t pin = *_data_pins[i];
    ESP_ERROR_CHECK(gpio_reset_pin(pin));
  }
}

void Sn76489an::begin()
{
  printf("Chip begin...\n");
  // Start frequency source
  ESP_ERROR_CHECK(ledc_timer_config(&Config::MASTER_CLOCK_TIMER_CFG));
  ESP_ERROR_CHECK(ledc_channel_config(&Config::MASTER_CLOCK_TIMER_CHAN_CFG));

  muteTone(Tone1);
  muteTone(Tone2);
  muteTone(Tone3);
  muteNoise();

  printf("Chip begin done!\n");
}

void Sn76489an::end()
{
  ESP_ERROR_CHECK(ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0));
}

void Sn76489an::adjustLastTone(float fq)
{
  // printf("Adjusting most significant part of frequency (%f).\n", fq);
  writeFreqHigh(fq);
  send();
}

void Sn76489an::muteTone(Tone tone)
{
  setToneAttenuation(tone, ATTENUATION_SILENCE);
}

void Sn76489an::setTone(Tone tone, float fq)
{
  // printf("Setting tone (%u) least signifcant part of frequency (%f).\n", tone, fq);
  adjustToneFine(tone, fq);
  adjustLastTone(fq);
}

void Sn76489an::setToneAttenuation(Tone tone, uint8_t att)
{
  // printf("Setting tone (%u) attenuation (%u).\n", tone, att);
  writeReg(tone | ATTENTUATION_REG);
  writeAttenutation(att);
  send();
}

void Sn76489an::adjustToneFine(Tone tone, float fq)
{
  writeReg(tone);
  writeFreqLow(fq);
  send();
}

void Sn76489an::setNoise(NoiseType type, NoiseShift shift)
{
  // printf("Setting noise type (%u) and shift (%u).\n", type, shift);
  writeReg(NOISE_REG);
  writeNoiseType(type);
  writeNoiseShift(shift);
  send();
}

void Sn76489an::setNoiseAttenuation(uint8_t att)
{
  // printf("Setting noise attenuation (%u).\n", att);
  writeReg(NOISE_REG | ATTENTUATION_REG);
  writeAttenutation(att);
  send();
}

void Sn76489an::muteNoise()
{
  setNoiseAttenuation(ATTENUATION_SILENCE);
}

void Sn76489an::send()
{
  // printf("Writing following data to chip:\n");
  // for (size_t i = 0; i < DATA_PIN_COUNT; i++)
  // {
  //   int val = gpio_get_level(*_data_pins[i]);
  //   printf("  D%u: %u\n", i, val);
  // }
  // printf("\n");

  disableWrite();
  enableWrite();
  ets_delay_us(WRITE_TIME_US);
  disableWrite();
  zeroData();
}

void Sn76489an::initialize()
{
  ESP_ERROR_CHECK(gpio_set_level(_ce_pin, ChipDisable));
  ESP_ERROR_CHECK(gpio_set_level(_we_pin, WriteDisable));
}

void Sn76489an::waitForReady()
{
  // do
  // {
  //   ets_delay_us(1);
  // } while (gpio_get_level(_ready_pin) == ChipBusy);
}

void Sn76489an::writeReg(uint8_t reg)
{
  writeDataValue(0, true);
  for (size_t i = 0; i < 3; i++)
  {
    writeDataValue(3 - i, reg & 1);
    reg >>= 1;
  }
}

void Sn76489an::writeDataValue(size_t d, bool val)
{
  ESP_ERROR_CHECK(gpio_set_level(*_data_pins[d], val ? 1 : 0));
}

void Sn76489an::writeFreqLow(float fq)
{
  int16_t freq = get_freq(fq);
  writeFreqLsb(freq);
}

void Sn76489an::writeFreqLsb(int16_t freq)
{
  for (size_t i = 0; i < 4; i++)
  {
    writeDataValue(7 - i, freq & 1);
    freq >>= 1;
  }
}

void Sn76489an::writeFreqHigh(float fq)
{
  int16_t freq = get_freq(fq);
  writeFreqMsb(freq);
}

void Sn76489an::writeFreqMsb(int16_t freq)
{
  freq >>= 4;

  writeDataValue(0, false);
  for (size_t i = 0; i < 6; i++)
  {
    writeDataValue(7 - i, freq & 1);
    freq >>= 1;
  }
}

void Sn76489an::setupOutputPin(gpio_num_t pin)
{
  assert(GPIO_IS_VALID_OUTPUT_GPIO(pin));
  ESP_ERROR_CHECK(gpio_reset_pin(pin));
  ESP_ERROR_CHECK(gpio_pullup_dis(pin));
  // ESP_ERROR_CHECK(gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY));
  ESP_ERROR_CHECK(gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT));
  ESP_ERROR_CHECK(gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_3));
}

void Sn76489an::writeAttenutation(uint8_t att)
{
  for (size_t i = 0; i < 4; i++)
  {
    writeDataValue(7 - i, att & 1);
    att >>= 1;
  }
}

void Sn76489an::writeNoiseType(uint8_t nt)
{
  writeDataValue(NOISE_TYPE_PIN, nt & 1);
}

void Sn76489an::writeNoiseShift(uint8_t shift)
{
  for (size_t i = 0; i < 3; i++)
  {
    writeDataValue(7 - i, shift & 1);
    shift >>= 1;
  }
}

void Sn76489an::selectChip()
{
  ESP_ERROR_CHECK(gpio_set_level(_ce_pin, ChipEnable));
}

void Sn76489an::deselectChip()
{
  ESP_ERROR_CHECK(gpio_set_level(_ce_pin, ChipDisable));
}

void Sn76489an::enableWrite()
{
  ESP_ERROR_CHECK(gpio_set_level(_we_pin, WriteEnable));
}

void Sn76489an::disableWrite()
{
  ESP_ERROR_CHECK(gpio_set_level(_we_pin, WriteDisable));
}

void Sn76489an::zeroData()
{
  for (size_t i = 0; i < DATA_PIN_COUNT; i++)
  {
    writeDataValue(i, false);
  }
}
