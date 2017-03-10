// Created by Diogo Justen (diogo.justen@reason.com.br) in Febuary 2016
// Copyright (C) 2016 Reason Tecnologia S.A. All rights reserved.
// Copyright (C) 2016 Alstom Grid Inc. All rights reserved.

// Includes ///////////////////////////////////////////////////////////////////

#include <boardinfo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <gainoff.h>
#include <time.h>
#include <math.h>
#include <acq.h>
#include <rms.h>
#include <measures.h>
#include <unistd.h>

// Definitions ////////////////////////////////////////////////////////////////

#define LOG_IDENTIFICATION      "[CALIBRATOR]"

#define MEASURES_SUBCYCLE_RATE 4

#define WAIT_TIME 20
#define PPC 256
#define NUM_ANALOG_PER_SLOT 8

#define BOARD_OFFSET(X) (X * 8)
#define CURRENT_OFFSET(X)(X * 8)
#define VOLTAGE_OFFSET(X)(X * 8 + 4)
#define VOLTAGE_SLOT 4
#define CURRENT_SLOT 0
#define NUM_CHANNEL_TYPE_PER_SLOT 4

#define SCALE_FACTOR 65536.0f
#define INFO_ADC_MAX 32768.0f
#define ERROR_X0 0.04f

#define DEFAULT_GAIN 0x8000
#define DEFAULT_OFFSET 0
#define MIN_GAIN (DEFAULT_GAIN * 1.00f)
#define MAX_GAIN (DEFAULT_GAIN * 1.05f)
#define MAX_OFFSET_A_B 60
#define MIN_OFFSET_A_B -60
#define MAX_OFFSET_C_D 600
#define MIN_OFFSET_C_D -600

#define PA8A_MAX_CURRENT 60.0f
#define PA8A_MAX_VOLTAGE 689.0f
#define PA8B_MAX_CURRENT 300.0f
#define PA8B_MAX_VOLTAGE 689.0f
#define PA8C_MAX_CURRENT 0.032f
#define PA8C_MAX_VOLTAGE 13.65f
#define PA8D_MAX_CURRENT 21.3f
#define PA8D_MAX_VOLTAGE 343.2f

#define PA8A_NOMINAL_CURRENT 1.0f
#define PA8A_NOMINAL_VOLTAGE 115.0f
#define PA8B_NOMINAL_CURRENT 5.0f
#define PA8B_NOMINAL_VOLTAGE 115.0f
#define PA8C_NOMINAL_CURRENT 0.02f
#define PA8C_NOMINAL_VOLTAGE 10.0f
#define PA8D_NOMINAL_CURRENT 5.0f
#define PA8D_NOMINAL_VOLTAGE 115.0f

// Macros /////////////////////////////////////////////////////////////////////

// Datatypes, Structures and Enumerations /////////////////////////////////////

typedef enum {
  SEARCHING1, WAITING1, CALIBRATING1,
  SEARCHING2, WAITING2, CALIBRATING2,
  FINISHED
} states_t;

typedef struct {
  float vnom;
  float inom;
  uint32_t frequency;
  uint32_t timeout_s;
  int board;
  dr60_pa8_info_t dr60_pa8_info;
  float coeficients[NUM_ANALOG_PER_SLOT];
  float inverse_coeficients[NUM_ANALOG_PER_SLOT];
} calibrator_t;

typedef enum {
  BOARD_PA8_A,
  BOARD_PA8_B,
  BOARD_PA8_C,
  BOARD_PA8_D,
  BOARD_PA8_MAX
} board_pa8_t;

// Private Functions Prototypes //////////////////////////////////////////////

// Public Variables ///////////////////////////////////////////////////////////

// Private Variables //////////////////////////////////////////////////////////

char *i2c_files[] = {
  "/sys/bus/i2c/devices/1-0052/eeprom",
  "/sys/bus/i2c/devices/1-0053/eeprom",
  "/sys/bus/i2c/devices/1-0054/eeprom",
  "/sys/bus/i2c/devices/1-0055/eeprom",
  ""
};

static float max_values[BOARD_PA8_MAX][2] = {
  { PA8A_MAX_CURRENT, PA8A_MAX_VOLTAGE },
  { PA8B_MAX_CURRENT, PA8B_MAX_VOLTAGE },
  { PA8C_MAX_CURRENT, PA8C_MAX_VOLTAGE },
  { PA8D_MAX_CURRENT, PA8D_MAX_VOLTAGE },
};

static float nominal_values[BOARD_PA8_MAX][2] = {
  { PA8A_NOMINAL_CURRENT, PA8A_NOMINAL_VOLTAGE },
  { PA8B_NOMINAL_CURRENT, PA8B_NOMINAL_VOLTAGE },
  { PA8C_NOMINAL_CURRENT, PA8C_NOMINAL_VOLTAGE },
  { PA8D_NOMINAL_CURRENT, PA8D_NOMINAL_VOLTAGE },
};

static float error_x0[BOARD_PA8_MAX][2] = {
  { PA8A_NOMINAL_CURRENT * ERROR_X0, PA8A_NOMINAL_VOLTAGE * ERROR_X0 },
  { PA8B_NOMINAL_CURRENT * ERROR_X0, PA8B_NOMINAL_VOLTAGE * ERROR_X0 },
  { PA8C_NOMINAL_CURRENT * ERROR_X0, PA8C_NOMINAL_VOLTAGE * ERROR_X0 },
  { PA8D_NOMINAL_CURRENT * ERROR_X0, PA8D_NOMINAL_VOLTAGE * ERROR_X0 },
};

static int min_max_offset[BOARD_PA8_MAX][2] = {
  { MIN_OFFSET_A_B, MAX_OFFSET_A_B },
  { MIN_OFFSET_A_B, MAX_OFFSET_A_B },
  { MIN_OFFSET_C_D, MAX_OFFSET_C_D },
  { MIN_OFFSET_C_D, MAX_OFFSET_C_D },
};

static float x_voltage[] = { 0.2f, 1.0f };
static float x_current[] = { 0.2f, 1.0f };

static float rms[NUM_ANALOG_PER_SLOT];
static int board_bom;

// XXX
static  dr60_pa8_info_t gainoff_old;

// Private Functions //////////////////////////////////////////////////////////

static void usage(char *program_name)
{
  printf("%s %s -b <board [0,1,2,3]> -t <timeout [s]> -f <frequency [Hz]>\n",
         LOG_IDENTIFICATION, program_name);
}

static int calibrator_get_args(calibrator_t *calibrator, int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; i += 2) {

    if (strlen(argv[i]) != 2 && argv[i][0] != 1) {
      printf("%s Invalid argument %s\n", LOG_IDENTIFICATION, argv[i]);
      usage(argv[0]);
      return -1;
    }

    if ((i + 1) == argc) {
      usage(argv[0]);
      printf("%s Invalid argument %s\n", LOG_IDENTIFICATION, argv[i]);
      return -1;
    }

    switch (argv[i][1]) {

      case 'b':
        if (sscanf(argv[i + 1], "%d", &calibrator->board) != 1) {
          printf("%s Invalid board argument: %s\n", LOG_IDENTIFICATION, argv[i + 1]);
          return -1;
        }

        if (calibrator->board < 0 || calibrator->board > 3) {
          printf("%s Invalid board argument: %d\n", LOG_IDENTIFICATION, calibrator->board);
          return -1;
        }
        break;

      case 't':
        if (sscanf(argv[i + 1], "%u", &calibrator->timeout_s) != 1) {
          printf("%s Invalid time: %s\n", LOG_IDENTIFICATION, argv[i]);
          return -1;
        }
        break;

      case 'f':
        if (sscanf(argv[i + 1], "%d", &calibrator->frequency) != 1) {
          printf("%s Invalid frequency: %s\n", LOG_IDENTIFICATION, argv[i]);
          return -1;
        }
        if (calibrator->frequency != 60 && calibrator->frequency != 50) {
          printf("%s Invalid frequency: %d\n", LOG_IDENTIFICATION, calibrator->frequency);
          return -1;
        }
        break;

      default:
        usage(argv[0]);
        return -1;
    }
  }

  return 0;
}

static int calibrator_get_multiplier(calibrator_t *calibrator, char *bom)
{
  int i;

  if ((strncmp(bom, "A", 1) != 0) &&
      (strncmp(bom, "B", 1) != 0) &&
      (strncmp(bom, "C", 1) != 0) &&
      (strncmp(bom, "D", 1) != 0)) {
    return -1;
  }

  board_bom = bom[0] - 'A';

  for (i = 0; i < NUM_CHANNEL_TYPE_PER_SLOT; i++) {
    calibrator->coeficients[CURRENT_SLOT + i] = max_values[bom[0] - 'A'][0] / INFO_ADC_MAX;
    calibrator->coeficients[VOLTAGE_SLOT + i] = max_values[bom[0] - 'A'][1] / INFO_ADC_MAX;
    calibrator->inverse_coeficients[CURRENT_SLOT + i] = INFO_ADC_MAX / max_values[bom[0] - 'A'][0];
    calibrator->inverse_coeficients[VOLTAGE_SLOT + i] = INFO_ADC_MAX / max_values[bom[0] - 'A'][1];
  }

  calibrator->inom = nominal_values[bom[0] - 'A'][0];
  calibrator->vnom = nominal_values[bom[0] - 'A'][1];

  return 0;
}

static int calibrator_check_board(calibrator_t *calibrator)
{
  generic_boardinfo_t board_info;

  if (boardinfo_read(i2c_files[calibrator->board], &board_info) != 0) {
    printf("%s Error reading device %s\n", LOG_IDENTIFICATION, i2c_files[calibrator->board]);
    return -1;
  }

  if (strncmp(board_info.model, DR60_PA8_MODEL, sizeof(DR60_PA8_MODEL)) != 0) {
    printf("%s Board model incompatible. This board %s is %s\n", LOG_IDENTIFICATION, i2c_files[calibrator->board], board_info.model);
    return -1;
  }

  if (calibrator_get_multiplier(calibrator, board_info.bom) < 0) {
    printf("%s Error get multiplier\n", LOG_IDENTIFICATION);
    return -1;
  }

  return 0;
}

static int calibrator_set_gain_offset(calibrator_t *calibrator)
{
  if (boardinfo_specific_write(i2c_files[calibrator->board], &calibrator->dr60_pa8_info, sizeof(dr60_pa8_info_t)) != 0) {
    printf("%s Error writing device %s\n", LOG_IDENTIFICATION, i2c_files[calibrator->board]);
    return -1;
  }

  return 0;
}

static int calibrator_finish(void)
{
  if (measures_finish() < 0) {
    return -1;
  }

  if (acq_finish() < 0) {
    return -1;
  }

  return 0;
}

static int calibrator_calc_gainoff(calibrator_t *calibrator, float y[2][NUM_ANALOG_PER_SLOT])
{
  uint32_t i;
  float gains[NUM_ANALOG_PER_SLOT] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
  float offsets[NUM_ANALOG_PER_SLOT] = {0, 0, 0, 0, 0, 0, 0, 0};

  for (i = 0; i < NUM_CHANNEL_TYPE_PER_SLOT; i++) {

    gains[VOLTAGE_SLOT + i] *= (x_voltage[1] - x_voltage[0]) / (y[1][VOLTAGE_SLOT + i] - y[0][VOLTAGE_SLOT + i]);
    gains[CURRENT_SLOT + i] *= (x_current[1] - x_current[0]) / (y[1][CURRENT_SLOT + i] - y[0][CURRENT_SLOT + i]);

    offsets[VOLTAGE_SLOT + i] = x_voltage[1] - y[1][VOLTAGE_SLOT + i] * gains[VOLTAGE_SLOT + i];
    offsets[CURRENT_SLOT + i] = x_current[1] - y[1][CURRENT_SLOT + i] * gains[CURRENT_SLOT + i];
  }

  // Technical Maneuver - Use previously calibrated values for current channels 
  for (i = 0; i < 4; i++) {
    calibrator->dr60_pa8_info.gain[i]   = gainoff_old.gain[i];
    calibrator->dr60_pa8_info.offset[i] = gainoff_old.offset[i];
  }

  for (i = 0; i < NUM_ANALOG_PER_SLOT; i++) {

    if (i >= 4 ) {
      calibrator->dr60_pa8_info.gain[i] = (uint16_t)(gains[i] * 0x8000);
      calibrator->dr60_pa8_info.offset[i] = (int16_t)round(offsets[i] * calibrator->inverse_coeficients[i]);
    }

    if ((calibrator->dr60_pa8_info.gain[i] > MAX_GAIN)
        || (calibrator->dr60_pa8_info.gain[i] < MIN_GAIN)
        || (calibrator->dr60_pa8_info.offset[i] > min_max_offset[board_bom][1])
        || (calibrator->dr60_pa8_info.offset[i] < min_max_offset[board_bom][0])) {

      printf("%s Gain or offset out of bounds on channel %d board %d."
             "Setting default values.\n", LOG_IDENTIFICATION, i + 1, calibrator->board);

      calibrator->dr60_pa8_info.gain[i] = DEFAULT_GAIN;
      calibrator->dr60_pa8_info.offset[i] = DEFAULT_OFFSET;
    }
  }

  return 0;
}

static int calibrator_set_default_gainoff(calibrator_t *calibrator)
{
  int i;
  int16_t gainoff_values[GAINOFF_MAX_GAIN + GAINOFF_MAX_OFFSET];

  for (i = 0; i < NUM_ANALOG_PER_SLOT; i++) {
    calibrator->dr60_pa8_info.gain[i] = DEFAULT_GAIN;
    calibrator->dr60_pa8_info.offset[i] = DEFAULT_OFFSET;
  }

  for (i = 0; i < GAINOFF_MAX_GAIN; i++) {
    gainoff_values[i] = DEFAULT_GAIN;
  }
  for (i = 0; i < GAINOFF_MAX_OFFSET; i++) {
    gainoff_values[i + GAINOFF_MAX_GAIN] = DEFAULT_OFFSET;
  }

  if (gainoff_set_values(gainoff_values, sizeof(gainoff_values)) < 0) {
    printf("%s Failed to set gainoff\n", LOG_IDENTIFICATION);
    return -1;
  }

  return 0;
}

// Public Functions ///////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
  states_t state = SEARCHING1;
  calibrator_t calibrator;
  int i;
  time_t begin, now;
  int wait_counter = 0;
  float y[2][NUM_ANALOG_PER_SLOT];
  int ch_freq[NUM_ANALOG_CHANNEL];
  signal_measure_read_t signal;
  static timestamper_t measures_timestamper;
  static timestamper_t phasor_timestamper;

  if (argc == 1) {
    usage(argv[0]);
    return -1;
  }

  calibrator.timeout_s = 300;
  calibrator.frequency = 60;
  signal = NULL;

  if (calibrator_get_args(&calibrator, argc, argv) < 0) {
    printf("%s Error getting args.\n", LOG_IDENTIFICATION);
    return -1;
  }

  if (calibrator_check_board(&calibrator) < 0) {
    printf("%s Error check board.\n", LOG_IDENTIFICATION);
    return -1;
  }

  if (boardinfo_specific_read(i2c_files[calibrator.board], &gainoff_old, sizeof(gainoff_old)) == RW_ERR) {
    printf("Error reading gain and offset: %s", i2c_files[calibrator.board]);
    return -1;
  }

  if (calibrator_set_default_gainoff(&calibrator) < 0) {
    printf("%s Error set default gain.\n", LOG_IDENTIFICATION);
    return -1;
  }

  if (acq_init(calibrator.frequency, PPC) < 0) {
    printf("%s Error acq init\n", LOG_IDENTIFICATION);
    return -1;
  }

  for (i = 0; i < NUM_ANALOG_CHANNEL; i++) {
    ch_freq[i] = calibrator.frequency;
  }

  if (timestamper_set_frequency(&measures_timestamper,
                                calibrator.frequency * MEASURES_SUBCYCLE_RATE) < 0) {
    printf("Unable to set aos frequency");
    return -1;
  }

  if (timestamper_set_frequency(&phasor_timestamper,
                                calibrator.frequency * PPC) < 0) {
    printf("Unable to set aos frequency to phasor timestamper");
    return -1;
  }

  if (measures_init(calibrator.frequency,
                    ch_freq,
                    PPC,
                    &measures_timestamper,
                    &phasor_timestamper,
                    signal) < 0) {
    printf("%s Error measures init. Probably instrument is running.\n", LOG_IDENTIFICATION);
    return -1;
  }

  x_voltage[0] *= calibrator.vnom;
  x_voltage[1] *= calibrator.vnom;
  x_current[0] *= calibrator.inom;
  x_current[1] *= calibrator.inom;

  time(&begin);

  while (state != FINISHED) {

    while(rms_has_data() == 0) {
      usleep(1000);
    }
    for (i = 0; i < NUM_ANALOG_PER_SLOT; i++) {
      rms[i] = (*rms_get_ptr(BOARD_OFFSET(calibrator.board) + i) * calibrator.coeficients[i]) / SCALE_FACTOR;
    }

    switch (state) {
      case SEARCHING1:

        printf("%s Apply %fV and %fA\n", LOG_IDENTIFICATION, x_voltage[0], x_current[0]);
        wait_counter = 0;
        state = WAITING1;
        break;

      case WAITING1:

        if (fabs(rms[VOLTAGE_OFFSET(0)] - x_voltage[0]) < error_x0[board_bom][1]) { 
          wait_counter++;
        }
        else {
          wait_counter = 0;
        }

        if (wait_counter >= WAIT_TIME) {
          state = CALIBRATING1;
        }
        break;

      case CALIBRATING1:

        for (i = 0; i < NUM_ANALOG_PER_SLOT; i++) {
          y[0][i] = rms[i];
printf("LOW - ch[%i] = %f\n", i, rms[i]);
        }
        state = SEARCHING2;
        break;

      case SEARCHING2:

        printf("%s Apply %fV and %fA\n", LOG_IDENTIFICATION, x_voltage[1], x_current[1]);
        wait_counter = 0;
        state = WAITING2;
        break;

      case WAITING2:

        if (fabs(rms[VOLTAGE_OFFSET(0)] - x_voltage[1]) < error_x0[board_bom][1]) {
          wait_counter++;
        }
        else {
          wait_counter = 0;
        }

        if (wait_counter >= WAIT_TIME) {
          state = CALIBRATING2;
        }
        break;

      case CALIBRATING2:

        for (i = 0; i < NUM_ANALOG_PER_SLOT; i++) {
          y[1][i] = rms[i];
printf("HIGH - ch[%i] = %f\n", i, rms[i]);
        }
        state = FINISHED;

      case FINISHED:
        break;
    }

    time(&now);
    if (calibrator.timeout_s < difftime(now, begin)) {
      printf("%s TIMEOUT!\n", LOG_IDENTIFICATION);
      calibrator_finish();
      return -1;
    }
  }

  if (calibrator_finish() < 0) {
    printf("%s Error calibrator finish\n", LOG_IDENTIFICATION);
    return -1;
  }

  if (calibrator_calc_gainoff(&calibrator, y) < 0) {
    printf("%s Error calculing gain and offset\n", LOG_IDENTIFICATION);
    return -1;
  }

  if (calibrator_set_gain_offset(&calibrator) < 0) {
    printf("%s Error setting gain and offset\n", LOG_IDENTIFICATION);
    return -1;
  }

  printf("%s Calibration successful\n", LOG_IDENTIFICATION);

  return 0;
}

// EOF
