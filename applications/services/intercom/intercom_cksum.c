#include "intercom_cksum.h"

#include <furi.h>

uint16_t intercom_calculate_cksum(const void* data, size_t data_size) {
    UNUSED(data);
    UNUSED(data_size);

    // TODO: Determine checksum algo
    return 0xdada;
}
