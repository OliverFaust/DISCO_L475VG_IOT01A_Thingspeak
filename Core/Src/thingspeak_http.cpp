// thingspeak_http.cpp -- see thingspeak_http.hpp.
#include "thingspeak_http.hpp"

#include <cstdio>
#include <cstring>
#include "fixed_point.hpp"

std::size_t tsBuildUpdate(char* buf, std::size_t size, const char* key, float field1, float field2, float field3)
{
    char f1[16], f2[16], f3[16];
    format_fixed(f1, sizeof f1, field1, 2);
    format_fixed(f2, sizeof f2, field2, 2);
    format_fixed(f3, sizeof f3, field3, 2);
    const int n = std::snprintf(buf, size,
                                "GET /update?api_key=%s&field1=%s&field2=%s&field3=%s HTTP/1.1\r\n"
                                "Host: %s\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                                key, f1, f2, f3, kThingSpeakHost);
    if (n < 0 || static_cast<std::size_t>(n) >= size) {
        if (size > 0) buf[0] = '\0';
        return 0;
    }
    return static_cast<std::size_t>(n);
}

void tsMaskKey(char* out, std::size_t size, const char* key)
{
    if (size == 0) return;
    std::size_t i = 0;
    for (; key[i] != '\0' && i + 1 < size; ++i) {
        out[i] = (i < 4) ? key[i] : '*';
    }
    out[i] = '\0';
}

namespace {

// Case-insensitive search for a header name at the start of a line, within [p, end).
const char* findHeader(const char* p, const char* end, const char* name)
{
    const std::size_t n = std::strlen(name);
    for (const char* line = p; line < end; ) {
        if (static_cast<std::size_t>(end - line) > n) {
            std::size_t i = 0;
            while (i < n) {
                char a = line[i], b = name[i];
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                if (a != b) break;
                ++i;
            }
            if (i == n) return line + n;
        }
        const void* nl = std::memchr(line, '\n', static_cast<std::size_t>(end - line));
        if (nl == nullptr) break;
        line = static_cast<const char*>(nl) + 1;
    }
    return nullptr;
}

long parseNumber(const char* p, const char* end, int base, const char** after)
{
    while (p < end && (*p == ' ' || *p == '\t')) ++p;
    long v = 0;
    bool any = false;
    for (; p < end; ++p) {
        int d;
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if (base == 16 && *p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
        else if (base == 16 && *p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
        else break;
        v = v * base + d;
        any = true;
    }
    if (after) *after = p;
    return any ? v : -1;
}

} // namespace

TsResponse tsParseResponse(const char* buf, std::size_t len)
{
    TsResponse r;
    const char* end = buf + len;

    // Status line: "HTTP/1.x NNN ..."
    if (len >= 12 && std::strncmp(buf, "HTTP/1.", 7) == 0) {
        r.status = static_cast<int>(parseNumber(buf + 9, end, 10, nullptr));
    }

    // End of the headers: an empty line.
    const char* body = nullptr;
    for (const char* p = buf; p + 3 < end; ++p) {
        if (p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n') { body = p + 4; break; }
    }
    if (body == nullptr) return r;
    r.headersComplete = true;

    bool chunked = false;
    if (const char* te = findHeader(buf, body, "transfer-encoding:")) {
        while (te < body && (*te == ' ' || *te == '\t')) ++te;
        chunked = (body - te) >= 7 && (te[0] | 0x20) == 'c' && std::strncmp(te + 1, "hunked", 6) == 0;
    }
    if (chunked) {
        // First chunk: "<hex size>\r\n<data>\r\n"; the body of an update is one short chunk.
        const char* after = nullptr;
        const long n = parseNumber(body, end, 16, &after);
        if (n < 0 || after + 2 > end) return r;
        const char* data = after + 2;              // skip CRLF after the size
        if (data + n > end) return r;
        r.entryId = parseNumber(data, data + n, 10, nullptr);
        r.bodyComplete = true;
        return r;
    }

    const char* cl = findHeader(buf, body, "content-length:");
    if (cl != nullptr) {
        const long n = parseNumber(cl, body, 10, nullptr);
        if (n >= 0 && body + n <= end) {
            r.entryId = parseNumber(body, body + n, 10, nullptr);
            r.bodyComplete = true;
        }
        return r;
    }
    // Neither: the body ends when the server closes the connection; parse what is there.
    r.entryId = parseNumber(body, end, 10, nullptr);
    return r;
}
