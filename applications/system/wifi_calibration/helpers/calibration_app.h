#pragma once
#include <cli/cli.h>
#include "cli_worker.h"

//typedef struct CalibrationApp CalibrationApp;

void* calibration_app_start(CliWorker* worker);
void calibration_app_stop(void* app_handle);
void calibration_app_parse_msg(void* app_handle, uint8_t* data, size_t size);
