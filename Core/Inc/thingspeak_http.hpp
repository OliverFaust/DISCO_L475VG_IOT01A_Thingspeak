// thingspeak_http.hpp -- the HTTP request and response of a ThingSpeak channel update.
//
// Pure text processing, no I/O: the Wi-Fi code sends what tsBuildUpdate() writes and passes what
// it receives to tsParseResponse(). This keeps the protocol testable on a PC.
//
// Request (HTTP/1.1; the connection is closed by the server after the response):
//   GET /update?api_key=<write key>&field1=<T>&field2=<RH>&field3=<p> HTTP/1.1
//   Host: api.thingspeak.com
//   Connection: close
// Response: status line, headers, and a plain-text body with the new entry id, "0" if the update
// was rejected (wrong key, or less than 15 s after the previous update of the channel).
#pragma once

#include <cstddef>

inline constexpr const char* kThingSpeakHost = "api.thingspeak.com";
inline constexpr unsigned short kThingSpeakPort = 80;   // plain HTTP: the module has no TLS

// Writes the update request for three field values (2 decimal places, negative values correct).
// Returns its length, or 0 if buf is too small. Pass a masked key (tsMaskKey) to get a printable copy.
std::size_t tsBuildUpdate(char* buf, std::size_t size, const char* key, float field1, float field2, float field3);

// Copies `key` with all but its first 4 characters replaced by '*' (same length), for printing.
void tsMaskKey(char* out, std::size_t size, const char* key);

struct TsResponse {
    int  status = 0;          // HTTP status code, 0 if no status line yet
    bool headersComplete = false;
    bool bodyComplete = false; // whole body received (Content-Length, or final chunk)
    long entryId = -1;        // body as a number: >0 new entry id, 0 rejected, -1 not (yet) parsed
};

// Parses what has been received so far (need not be NUL-terminated; len bytes are used).
TsResponse tsParseResponse(const char* buf, std::size_t len);
