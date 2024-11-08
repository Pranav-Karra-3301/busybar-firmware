#pragma once

#include <stdint.h>
#include <stddef.h>

uint16_t intercom_calculate_cksum(const void* data, size_t data_size);
