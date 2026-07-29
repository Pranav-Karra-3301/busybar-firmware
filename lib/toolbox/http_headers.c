#include "http_headers.h"

void http_header_free(HttpHeader header) {
    furi_string_free(header.key);
    furi_string_free(header.value);
}

typedef enum ParseState {
    ParseStateKey,
    ParseStateColon,
    ParseStateValue,
    ParseStateValueSpace,
    ParseStateNewline,
} ParseState;

static bool parse_headers_list(HttpHeaders* headers, const char* data, size_t data_size) {
    ParseState state = ParseStateKey;
    size_t key_begin_i = 0;
    size_t key_end_i = 0;
    size_t value_begin_i = 0;
    size_t value_end_i = 0;
    for(size_t i = 0; i != data_size; ++i) {
        char c = data[i];
        switch(state) {
        case ParseStateKey: {
            if(c == ':') {
                if(key_begin_i == i) {
                    // Line starts with a colon
                    return false;
                } else {
                    key_end_i = i;
                    state = ParseStateColon;
                }
            } else if(c == '\r' && key_begin_i == i) {
                // End of headers
                return true;
            } else if(isspace((int)c)) {
                return false;
            }
            break;
        }
        case ParseStateColon: {
            if(c != ' ' && c != '\t') {
                if(isspace((int)c)) {
                    return false;
                }
                state = ParseStateValue;
                value_begin_i = i;
            }
            break;
        }
        case ParseStateValue: {
            if(c == '\r') {
                value_end_i = i;
                state = ParseStateNewline;
            } else if(c == ' ' || c == '\t') {
                value_end_i = i;
                state = ParseStateValueSpace;
            }
            break;
        }
        case ParseStateValueSpace: {
            if(c == '\r') {
                state = ParseStateNewline;
            } else if(c != ' ' && c != '\t') {
                state = ParseStateValue;
            }
            break;
        }
        case ParseStateNewline: {
            if(c == '\n') {
                HttpHeader header = {
                    .key = furi_string_alloc_printf(
                        "%.*s", key_end_i - key_begin_i, data + key_begin_i),
                    .value = furi_string_alloc_printf(
                        "%.*s", value_end_i - value_begin_i, data + value_begin_i),
                };
                HttpHeaderArray_push_back(headers->headers, header);

                key_begin_i = i + 1;
                state = ParseStateKey;
            } else {
                return false;
            }
            break;
        }
        }
    }
    return true;
}

static ssize_t parse_status_line(HttpHeaders* headers, const char* data, size_t size) {
    ssize_t i = 0;
    if(size < strlen("HTTP/1.0 XXX \r\n")) {
        return -1;
    }
    if(strncmp(data, "HTTP/", 5) != 0) {
        return -1;
    }
    i += 5;
    if(!isdigit((int)data[i + 0]) || data[i + 1] != '.' || !isdigit((int)data[i + 2]) ||
       data[i + 3] != ' ') {
        return -1;
    }
    i += 4;
    char status_code[4];
    for(ssize_t j = 0; j != 3; ++j) {
        if(!isdigit((int)data[i + j])) {
            return -1;
        }
        status_code[j] = data[i + j];
    }
    if(data[i + 3] != ' ') {
        return -1;
    }
    status_code[3] = 0;
    headers->status = atoi(status_code);
    i += 4;
    const char* cr = strchr(data + i, '\r');
    if(!cr || cr[1] != '\n') {
        return -1;
    }
    size_t reason_phrase_len = cr - data - i;
    headers->status_text = furi_string_alloc_printf("%.*s", reason_phrase_len, data + i);
    return cr - data + 2;
}

HttpHeaders* http_headers_parse(const char* data, size_t size) {
    HttpHeaders* headers = malloc(sizeof(HttpHeaders));
    HttpHeaderArray_init(headers->headers);

    bool success = false;

    do {
        ssize_t headers_offset = parse_status_line(headers, data, size);
        if(headers_offset < 0) {
            break;
        }

        success = parse_headers_list(headers, data + headers_offset, size - headers_offset);
    } while(false);

    if(!success) {
        http_headers_free(headers);
        headers = NULL;
    }
    return headers;
}

void http_headers_free(HttpHeaders* headers) {
    HttpHeaderArray_clear(headers->headers);
    if(headers->status_text) {
        furi_string_free(headers->status_text);
    }
    free(headers);
}
