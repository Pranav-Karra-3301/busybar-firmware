#include "ca_storage.h"

#include <storage/storage.h>

#define CA_STORAGE_BUNDLE_PATH   EXT_PATH("apps_assets/ca/cacert.pem")
#define CA_STORAGE_MAX_FILE_SIZE (350000LLU) // Limit max file size to 350KB

#define TAG "CaStorage"

struct CaStorage {
    char data[0];
};

static CaStorage* ca_storage_alloc(void) {
    CaStorage* instance = NULL;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    do {
        if(!storage_file_open(file, CA_STORAGE_BUNDLE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "Failed to open CA bundle file: %s", CA_STORAGE_BUNDLE_PATH);
            break;
        }

        const uint64_t file_size = storage_file_size(file);

        if(file_size == 0) {
            FURI_LOG_E(TAG, "CA bundle file is empty");
            break;
        }

        if(file_size > CA_STORAGE_MAX_FILE_SIZE) {
            FURI_LOG_E(
                TAG, "CA bundle file size exceeds %llu KB", CA_STORAGE_MAX_FILE_SIZE / 1000);
            break;
        }

        instance = malloc(file_size + 1);

        const uint64_t read_size = storage_file_read(file, instance, file_size);

        if(read_size != file_size) {
            FURI_LOG_E(
                TAG,
                "Failed to read CA bundle file: expected %llu, read %llu bytes",
                file_size,
                read_size);

            free(instance);
            instance = NULL;
            break;
        }

        instance->data[file_size] = '\0';

        FURI_LOG_I(TAG, "Load CA bundle file OK");

    } while(false);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(instance == NULL) {
        // Fallback empty certificate
        instance = calloc(1, sizeof(char));
    }

    return instance;
}

const char* ca_storage_get_pem_bundle(CaStorage* instance) {
    furi_check(instance);
    return instance->data;
}

void ca_storage_on_system_start(void) {
    CaStorage* instance = ca_storage_alloc();
    furi_record_create(RECORD_CA_STORAGE, instance);
}
