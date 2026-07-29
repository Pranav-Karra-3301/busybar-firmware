/// @file http_headers.h Utility functions for dealing with HTTP headers
#pragma once

#include <furi/core/string.h>
#include <m-array.h>

typedef struct HttpHeader {
    FuriString* key;
    FuriString* value;
} HttpHeader;

void http_header_free(HttpHeader header);

M_ARRAY_DEF(HttpHeaderArray, HttpHeader, M_OPEXTEND(M_POD_OPLIST, CLEAR(http_header_free)));

typedef struct HttpHeaders {
    uint32_t status;
    FuriString* status_text;

    HttpHeaderArray_t headers;
} HttpHeaders;

HttpHeaders* http_headers_parse(const char* data, size_t size);

void http_headers_free(HttpHeaders* headers);
