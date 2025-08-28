#include "l10n_table.h"
#include "l10n_table_i.h"
#include "l10n_generated.h"

#define READ_BUFFER_SZ 512

const L10nTable* l10n_table_alloc_builtin(const char* app_id, L10nLocale locale) {
    for(size_t i = 0; i < L10N_APP_LIST_COUNT; i++) {
        const L10nAppListEntry* entry = &L10N_APP_LIST[i];
        if((strcmp(entry->app_id, app_id) == 0) && (entry->locale == locale)) {
            return entry->table;
        }
    }
    return NULL;
}

static size_t l10n_table_get_entry_cnt_from_file(File* file) {
    size_t newline_cnt = 0;

    storage_file_seek(file, 0, true);
    char buffer[READ_BUFFER_SZ];
    size_t read_this_time = 0;
    while((read_this_time = storage_file_read(file, buffer, sizeof(buffer)))) {
        for(size_t i = 0; i < read_this_time; i++) {
            if(buffer[i] == '\n') newline_cnt++;
        }
    }

    return newline_cnt;
}

const L10nTable* l10n_table_alloc_from_storage(File* file) {
    size_t data_size = storage_file_size(file);
    size_t entry_cnt = l10n_table_get_entry_cnt_from_file(file);

    const size_t header_size = sizeof(L10nTable);
    const size_t entry_array_size = sizeof(const char* [entry_cnt]);
    uint8_t* allocation = malloc(header_size + entry_array_size + data_size);

    char** entries = (char**)(allocation + header_size);
    char* all_templates = (char*)(allocation + header_size + entry_array_size);

    storage_file_seek(file, 0, true);
    furi_check(storage_file_read(file, all_templates, data_size) == data_size);

    // replace all LFs with null terminators,
    // remember pointers to all resulting strings
    char* current_template_start = all_templates;
    size_t current_idx = 0;
    for(size_t i = 0; i < data_size; i++) {
        char* current_char = &all_templates[i];
        if(*current_char != '\n') continue;

        *current_char = '\0';
        entries[current_idx] = current_template_start;
        current_template_start = current_char + 1;
        current_idx++;
    }

    furi_check(current_idx == entry_cnt);

    L10nTable* table = (L10nTable*)allocation;
    table->entries = (const char* const*)entries;
    table->entry_cnt = entry_cnt;
    table->is_owned = true;

    return table;
}

void l10n_table_free(const L10nTable* table) {
    if(!table->is_owned) return;
    free((void*)table);
}

const char* l10n_table_get(const L10nTable* table, size_t index) {
    if(index >= table->entry_cnt) return NULL;
    const char* entry = table->entries[index];
    if(!entry) return entry;
    if(strcmp(entry, "") == 0) return NULL;
    return entry;
}
