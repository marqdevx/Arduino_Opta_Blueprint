/* -------------------------------------------------------------------------- */
/* FILENAME:    OptaAnalog.h
   AUTHOR:      Daniele Aimo (d.aimo@arduino.cc)
   DATE:        20230801
   REVISION:    0.0.1
   DESCRIPTION: Header file for OptaAnalog class
   LICENSE:     Copyright (c) 2024 Arduino SA
                This Source Code Form is subject to the terms fo the Mozilla
                Public License (MPL), v 2.0. You can obtain a copy of the MPL
                at http://mozilla.org/MPL/2.0/.
   NOTES:                                                                     */
/* -------------------------------------------------------------------------- */

#ifndef OPTA_ANALOG_BLUE_MODULE
#define OPTA_ANALOG_BLUE_MODULE

#if defined ARDUINO_OPTA_ANALOG || defined ARDUINO_UNO_TESTALOG_SHIELD
#include "Arduino.h"
#include "CommonCfg.h"
#include "MsgCommon.h"
#include "OptaAnalogTypes.h"
#include "OptaBlueModule.h"
#include "OptaCrc.h"
#include "Protocol.h"
#include "SPI.h"
#include "boot.h"
#include "sys/_stdint.h"
#include <cstdint>

// #define DEBUG_ENABLE_SPI
// #define DEBUG_SERIAL
// #define DEBUG_ANALOG_PARSE_MESSAGE
//
#define BUFF_DIM 4

/* definition of "fake" channel for those register that have a SINGLE REGISTER
 * for each Analog Device (instead other kind of register have 4 register all
 * equal one for each channel)*/
#define OA_DUMMY_CHANNEL_DEVICE_0 101
#define OA_DUMMY_CHANNEL_DEVICE_1 102

class OptaAnalog : public Module {
private:
  /* ---------------------------------------------------------------------
   * Data structures used to hold information about Analog Device AD74412R
   * --------------------------------------------------------------------- */
  CfgFun_t fun[OA_AN_CHANNELS_NUM]; // function configuration x channel
  CfgPwm pwm[OA_PWM_CHANNELS_NUM];  // pwm configuration x channel
  CfgAdc adc[OA_AN_CHANNELS_NUM];   // adc configuration x channel
  CfgDi din[OA_AN_CHANNELS_NUM];    // d[igital]i[nput] configuration x channel
  CfgGpo gpo[OA_AN_CHANNELS_NUM];   // gpo configuration x channel
  CfgDac dac[OA_AN_CHANNELS_NUM];   // dac configuration x channel
  CfgRtd rtd[OA_AN_CHANNELS_NUM];   // rtd configuration x channel

  uint16_t alert[OA_AN_DEVICES_NUM];
  uint16_t aMask[OA_AN_DEVICES_NUM]; // a[lert]Mask
  uint16_t state[OA_AN_DEVICES_NUM];

  bool en_adc_diag_rej[OA_AN_DEVICES_NUM];
  bool di_scaled[OA_AN_DEVICES_NUM];
  uint8_t di_th[OA_AN_DEVICES_NUM];
  /* the status of the digital inputs (bitmask) */
  uint8_t digital_ins;
  /* the status of the digital outputs (bitmask) */
  uint8_t gpo_digital_out;
  /* the status of the leds (bitmask) */
  uint8_t led_status;
  uint16_t rtd_update_time;
  /* ---------------------------------------------------------------------
   * SPI communication buffer and functions
   * --------------------------------------------------------------------- */
  uint8_t com_buffer[BUFF_DIM];

  uint8_t adc_ch_mask_0 = 0;
  uint8_t adc_ch_mask_1 = 0;
  uint8_t adc_ch_mask_0_last = 0;
  uint8_t adc_ch_mask_1_last = 0;

  bool update_dac_using_LDAC = false;

  /* used to avoid change of function while "adding" adc to
   * a certain channel */
  bool write_function_configuration[OA_AN_CHANNELS_NUM] = {
      true, true, true, true, true, true, true, true};

  uint8_t channel_setup = 0;

  void set_channel_setup(uint8_t ch);
  void setup_channels();

  /* ABOUT register and reading writing register with the function below
   * --------------------------------------------------------------------
   * Typically in the Analog Device AD74412R there are 2 "kind" of registers
     - channels register --> 1 register for each channel (4 register per device)
       in this case the address of the 4 registers are consecutive
     - single register --> 1 register that handle all channels

     Opta Analog uses 2 Analog Devices AD74412R at the same time. So Opta Analog
     channels are 8 from 0 to 7.

     The write_reg and read_reg function have a channel parameter that uses the
     Opta Analog channel numeration (so from 0 to 7). But they can be used to
     read and write "single" register by passing the ch parameter the special
     values:
     - OA_DUMMY_CHANNEL_DEVICE_0 -> for register on AD74412R device 0
     - OA_DUMMY_CHANNEL_DEVICE_1 -> for register on AD74412R device 1
     Moreover these functions in case of "channel" registers use as first
     parameter
     ('addr') the "base" address of the set of 4 consecutive addresses used for
     the 4 channels and the displacement is automatically calculated by the
     function.

     The read_direct_reg and write_direct_reg instead read and write directly a
     certain register from a certain device (0 or 1) at a certain address (no
     automatic displacement calculation is performed)

     Pay attention to the fact that the mapping of the Opta Analog channels
     (from 0 to 7) is the following:
     - ch 0 -> device 0 register channel (the displacement) 1 of AD74412R
     - ch 1 -> device 0 register channel (the displacement) 0 of AD74412R
     - ch 2 -> device 1 register channel (the displacement) 0 of AD74412R
     - ch 3 -> device 1 register channel (the displacement) 1 of AD74412R
     - ch 4 -> device 1 register channel (the displacement) 2 of AD74412R
     - ch 5 -> device 1 register channel (the displacement) 3 of AD74412R
     - ch 6 -> device 0 register channel (the displacement) 2 of AD74412R
     - ch 7 -> device 0 register channel (the displacement) 3 of AD74412R

      Other function defined here below are helpers to correctly map the address
      - get_add_offset takes the Opta Analog channel and returns the
     displacement
      - get_device takes the Opta Analog channel and returns the device index (0
     or 1)
      - get_dummy_channel takes the Opta Analog channel and return the "dummy"
     channel code to be passed to write_reg and read_reg (i.e. it returns the
     device dummy code the Opta channel belongs to)
     */

  void write_reg(uint8_t addr, uint16_t value, uint8_t ch);
  bool read_reg(uint8_t add, uint16_t &value, uint8_t ch);
  uint8_t get_add_offset(uint8_t ch);
  uint8_t get_device(uint8_t ch);
  uint8_t get_dummy_channel(uint8_t ch);
  bool read_direct_reg(uint8_t device, uint8_t addr, uint16_t &value);
  void write_direct_reg(uint8_t device, uint8_t addr, uint16_t value);

  /* contains all the digital input status */
  void update_alert_mask(int8_t ch);
  void update_alert_status();
  void update_live_status(uint8_t ch);
  void update_live_status();
  void update_adc_value(uint8_t ch);
  void update_adc_diagnostic(uint8_t ch);

  bool is_adc_started(uint8_t device);
  void stop_adc(uint8_t device, bool power_down);
  void start_adc(uint8_t device, bool single_acquisition);
  bool is_adc_updatable(uint8_t device, bool wait_for_conversion);
  bool adc_enable_channel(uint8_t ch, uint16_t &reg);

  bool is_adc_busy(uint8_t ch);
  bool is_adc_conversion_finished(uint8_t ch);

  /* SPI Sending and receiving messages functions */

  /* init pwm */
  void begin_pwms();
  /* tell if the ADC conversion is in progress */
  bool stAdcIsBusy(uint8_t device);
  /* tell if a channel is assigned to DAC function */
  bool is_dac_used(uint8_t ch);
  bool parse_setup_rtd_channel();
  bool parse_setup_adc_channel();
  bool parse_setup_dac_channel();
  bool parse_setup_di_channel();
  bool parse_setup_high_imp_channel();
  bool parse_get_adc_value();
  bool parse_get_all_adc_value();
  bool parse_set_dac_value();
  bool parse_set_all_dac_value();
  bool parse_get_di_value();
  bool parse_set_pwm_value();
  bool parse_get_rtd_value();
  bool parse_set_rtd_update_rate();
  bool parse_set_led();

  void toggle_ldac();

public:
  OptaAnalog();
  void begin() override;
  void update() override;
  int parse_rx() override;

  uint8_t getMajorFw();
  uint8_t getMinorFw();
  uint8_t getReleaseFw();
  std::vector<uint8_t> getProduct();
  void goInBootloaderMode();
  /* NAMING CONVENTION:
   * ------------------
   *  All functions start with the following prefix (each prefix has certain
   *  meaning):
   *  'configure': only set a variable to a value appropriate for that
   *  'send': take all the settings (the variables set with 'configure'
   * function) and send the configuration to the Analog chip. From this point
   * the configuration is actually used. 'update': function to be called
   * periodically (usually to get information form the device but possibly
   * also to set something to the Analog device), the update function read
   * information and put it into a variable 'get' get the value of the
   * variable set with the 'update' function 'set' is used to make "complex"
   * pre-configured configuration and compose some 'configure' functions and
   * 'send' */

  /* ################################################################### */
  /* CONFIGURE CHANNEL FUNCTIONs                                         */
  /* ################################################################### */
  void configureFunction(uint8_t ch, CfgFun_t f);
  void sendFunction(uint8_t ch);
  CfgFun_t getFunction(uint8_t ch);

  /* ################################################################### */
  /* ADC FUNCTIONs                                                       */
  /* ################################################################### */

  /* configure the ADC function */
  void configureAdcMux(uint8_t ch, CfgAdcMux_t m);
  void configureAdcRange(uint8_t ch, CfgAdcRange_t r);
  void configureAdcPullDown(uint8_t ch, bool en);
  void configureAdcRejection(uint8_t ch, bool en);
  void configureAdcDiagnostic(uint8_t ch, bool en);
  void configureAdcDiagRejection(uint8_t ch, bool en);
  void configureAdcMovingAverage(uint8_t ch, uint8_t ma);
  void configureAdcEnable(uint8_t ch, bool en);
  /* send ADC configuration to the device */
  void sendAdcConfiguration(uint8_t ch);
  /* start and stop the ADC conversion */
  void startAdc(bool single = false);
  void stopAdc(bool power_down = false);
  /* read adc values for all the enabled channels */
  void updateAdc(bool wait_for_conversion = false);
  void updateAdcDiagnostics();
  /* get the ADC values */
  uint16_t getAdcValue(uint8_t ch);
  uint16_t getAdcDiagValue(uint8_t ch);

  bool stConvAdcFinished(uint8_t device);
  /* performs a SW reset of the 2 analog devices present on Opta Analog */
  void swAnalogDevReset();
  void sychLDAC();

  /* ##################################################################### */
  /*                         RTD FUNCTIONs                                 */
  /* ##################################################################### */
  /* assign the RTD function to the channel ch, the use_3_w is meant to indicate
   * the use of 3 wires RTD. 3 wires RTD is available only for channels 0 and 1.
   * If this parameter is true for channels different from 0 and 1 it is not
   * used. The configuration is not actually used until the 'send' function is
   * called */
  void configureRtd(uint8_t ch, bool use_3_w, float current);
  /* calculate and update the Rdt values */
  void updateRtd();
  /* get the RTD value */
  float getRtdValue(uint8_t ch);
  /* ##################################################################### */
  /*                         DIN FUNCTIONs                                 */
  /* ##################################################################### */

  void configureDinFilterCompIn(uint8_t ch, bool en);
  void configureDinInvertCompOut(uint8_t ch, bool en);
  void configureDinEnableComp(uint8_t ch, bool en);
  void configureDinDebounceSimple(uint8_t ch, bool en);
  void configureDinScaleComp(uint8_t ch, bool en);
  void configureDinCompTh(uint8_t ch, uint8_t v);
  void configureDinCurrentSink(uint8_t ch, uint8_t v);
  void configureDinDebounceTime(uint8_t ch, uint8_t v);

  void sendDinConfiguration(uint8_t ch);
  void updateDinReadings();
  bool getDinValue(uint8_t ch);

  /* ##################################################################### */
  /*                         GPO FUNCTIONs                                 */
  /* ##################################################################### */

  void configureGpo(uint8_t ch, CfgGpo_t f, GpoState_t state);
  void updateGpo(uint8_t ch);
  void digitalWriteAnalog(uint8_t ch, GpoState_t s);
  void digitalParallelWrite(GpoState_t a, GpoState_t b, GpoState_t c,
                            GpoState_t d);

  /* ##################################################################### */
  /*                         DAC FUNCTIONs                                 */
  /* ##################################################################### */

  /* set the DAC configuration */
  void configureDacCurrLimit(uint8_t ch, CfgOutCurrLim_t cl);
  void configureDacUseSlew(uint8_t ch, CfgOutSlewRate_t sr, CfgOutSlewStep_t r);
  void configureDacDisableSlew(uint8_t ch);
  void configureDacUseReset(uint8_t ch, uint16_t value);
  void configureDacDisableReset(uint8_t ch);
  void configureDacResetValue(uint8_t ch, uint16_t value);
  /* reset the DAC value to the reset DAC value - at the
   * present this function reset all channel configured in a
   * single analog device TODO: verify this behaviour */
  /* write DAC configuration to the device */
  void sendDacConfiguration(uint8_t ch);
  /* used to set the DAC value \*/
  void configureDacValue(uint8_t ch, uint16_t value);
  /* write the DAC value to the device */
  void updateDacValue(uint8_t ch, bool toggle = true);
  /* read the current DAC value used (this can be different
   * from Dac Value due to slew rate settings */
  void updateDacPresentValue(uint8_t ch);
  /* get the current Dac value */
  uint16_t getDacCurrentValue(uint8_t ch);

  /* write the reset value to the device */
  void resetDacValue(uint8_t ch);
  /* update DAC channel if allocated to this function */
  void updateDac(uint8_t ch);

  /* ##################################################################### */
  /*                     ALERT AND DIAGNOSTIC                              */
  /* ##################################################################### */
  void configureAlertMaskRegister(uint8_t device, uint16_t alert);
  void updateAlertStatusRegister(uint8_t device);

  void enableThermalReset(bool en);

  uint8_t getSiliconRevision(uint8_t device);

  void writeReg(uint8_t addr, uint16_t value, int8_t ch) {
    return write_reg(addr, value, ch);
  }
  uint16_t readReg(uint8_t device, uint8_t addr) {
    uint16_t value = 0;
    read_direct_reg(device, addr, value);
    return value;
  }

  /* ##################################################################### */
  /*                                PWMS                                   */
  /* ##################################################################### */
  /* configure the period of the PWM related to channel ch */
  void configurePwmPeriod(uint8_t ch, uint32_t period_us);
  /* configure the pulse of the PWM related to channel ch */
  void configurePwmPulse(uint8_t ch, uint32_t pulse_us);
  /* update PWM, if is not active the Pwm is automatically
   * started */
  void updatePwm(uint8_t ch);
  /* suspend the PWM */
  void suspendPwm(uint8_t ch);

  /* ##################################################################### */
  /*                                LEDS                                   */
  /* ##################################################################### */

  void updateLedStatus();
  void setLedStatus(uint8_t i);
#ifdef DEBUG_SERIAL
  void displayOaDebugInformation();
  int debugChannelFunction(uint8_t ch);
  void debugAdcConfiguration(uint8_t ch);
  void debugDiConfiguration(uint8_t ch);
  void debugDacFunction(uint8_t ch);
#endif
#ifdef DEBUG_UPDATE_FW
  void set_led_on(uint8_t l) { led_status |= (1 << l); }
  void set_led_off(uint8_t l) { led_status &= ~(1 << l); }
  void debug_with_leds();
#endif
};
#endif
#endif
