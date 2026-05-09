#include "ca_certs.h"

#include <storage/storage.h>

#define TAG                 "CaCerts"
#define CERT_FILE_CA_BUNDLE EXT_PATH("apps_assets/ca/cacert.pem")

void ca_certs_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    CaCerts* certs = NULL;
    bool success = false;

    do {
        if(!storage_file_open(file, CERT_FILE_CA_BUNDLE, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "Failed to open bundle file");
            break;
        }

        size_t size = storage_file_size(file);
        size_t size_w_null_termination = size + 1;
        certs = malloc(sizeof(CaCerts) + size_w_null_termination);
        certs->length = size;

        if(storage_file_read(file, certs->data, size) != size) {
            FURI_LOG_E(TAG, "Failed to read bundle file");
            break;
        }

        furi_record_create(RECORD_CA_CERTS, certs);
        success = true;
    } while(0);

    if(!success && certs) free(certs);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
