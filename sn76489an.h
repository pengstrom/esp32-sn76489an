#if !defined(_SN76489AN_H)
#define _SN76489AN_H

#include <driver/gpio.h>
#include <driver/sdm.h>
#include <../../include/config.h>

class Sn76489an
{
public:
  static const uint8_t TONE_1_REG = 0b000;
  static const uint8_t TONE_2_REG = 0b010;
  static const uint8_t TONE_3_REG = 0b100;
  static const uint8_t ATTENUATION_SILENCE = 0b1111;
  static const uint8_t ATTENUATION_FULL_VOLUME = 0b0000;
  static constexpr float MIN_FREQ = Config::MASTER_CLOCK_FREQ_HZ / (32.0f * 0b1111111111);
  static constexpr float MAX_FREQ = Config::MASTER_CLOCK_FREQ_HZ / (32.0f * 0b1);

  struct config_t
  {
    // control pins
    gpio_num_t we_pin;
    gpio_num_t ce_pin;

    // output pin
    // gpio_num_t ready_pin;

    // input pins
    gpio_num_t d0_pin;
    gpio_num_t d1_pin;
    gpio_num_t d2_pin;
    gpio_num_t d3_pin;
    gpio_num_t d4_pin;
    gpio_num_t d5_pin;
    gpio_num_t d6_pin;
    gpio_num_t d7_pin;
  };

  enum Tone
  {
    Tone1 = TONE_1_REG,
    Tone2 = TONE_2_REG,
    Tone3 = TONE_3_REG
  };

  enum NoiseType
  {
    NoisePeriodic = 0,
    NoiseWhite = 1,
  };

  enum NoiseShift
  {
    NoiseDiv512 = 0,
    NoiseDiv1024,
    NoiseDiv2048,
    NoiseTone3
  };

  Sn76489an(config_t cfg);
  ~Sn76489an();

  void begin();
  void end();

  void setTone(Tone tone, float fq);
  void setToneAttenuation(Tone tone, uint8_t att);
  void adjustToneFine(Tone tone, float fq);
  void adjustLastTone(float fq);
  void muteTone(Tone tone);

  void setNoise(NoiseType type, NoiseShift fq);
  void setNoiseAttenuation(uint8_t att);
  void muteNoise();

protected:
  static const uint8_t NOISE_REG = 0b110;
  static const uint8_t ATTENTUATION_REG = 0b001;
  static const uint8_t NOISE_TYPE_PIN = 5;

  enum ChipSignal
  {
    ChipEnable = 0,
    ChipDisable = 1
  };

  enum WriteSignal
  {
    WriteEnable = 0,
    WriteDisable = 1
  };

  enum ReadySignal
  {
    ChipReady = 1,
    ChipBusy = 0
  };

  static const uint8_t ATT_MASK = 0b1111;
  static const int MAX_CLOCK_FREQ = 4 * 1000 * 1000; // 4 kHz
  static const int WRITE_TIME_CYCLES = 32;           // cycles
  static const int WRITE_TIME_US = (WRITE_TIME_CYCLES * 1000 * 1000) / Config::MASTER_CLOCK_FREQ_HZ;
  static const size_t DATA_PIN_COUNT = 8;

  static uint16_t get_freq(float tone_fq);

  // control pins
  gpio_num_t _clk_pin;
  gpio_num_t _we_pin;
  gpio_num_t _ce_pin;

  // output pin
  // gpio_num_t _ready_pin;

  // input pins
  gpio_num_t *_data_pins[DATA_PIN_COUNT];
  gpio_num_t _d0_pin;
  gpio_num_t _d1_pin;
  gpio_num_t _d2_pin;
  gpio_num_t _d3_pin;
  gpio_num_t _d4_pin;
  gpio_num_t _d5_pin;
  gpio_num_t _d6_pin;
  gpio_num_t _d7_pin;

  void setupOutputPin(gpio_num_t pin);

  void initialize();

  void send();

  void waitForReady();

  // Prepare data pins
  void writeReg(uint8_t reg);
  void writeDataValue(size_t d, bool val);
  void writeFreqLow(float fq);
  void writeFreqLsb(int16_t freq);
  void writeFreqHigh(float fq);
  void writeFreqMsb(int16_t freq);
  void writeAttenutation(uint8_t att);
  void writeNoiseType(uint8_t nt);
  void writeNoiseShift(uint8_t shift);

  void selectChip();
  void deselectChip();
  void enableWrite();
  void disableWrite();

  void zeroData();
};

#endif // _SN76489AN_H
