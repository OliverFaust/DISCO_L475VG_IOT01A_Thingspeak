// uplink_status.hpp -- the messages the Uplink process sends to the Console.
#pragma once

#include <cstdint>
#include <type_traits>

enum class UplinkEvent : uint8_t {
    ModuleReady,     // Wi-Fi driver initialised
    ModuleFailed,    // driver/module initialisation failed; retry after retryInMs
    Joined,          // joined the access point; ip valid; durationMs = join time
    JoinFailed,      // join failed (code); retry after retryInMs
    Uploaded,        // ThingSpeak accepted the update: entryId > 0
    Rejected,        // ThingSpeak answered, but entry id 0 (or HTTP status not 200)
    UploadFailed,    // network error at `stage` (code); retry after retryInMs
    Rejoining,       // too many failed uploads in a row: leaving and joining again
    SampleInvalid,   // the latest sample was invalid: nothing sent this time
};

enum class UplinkStage : uint8_t { None, Dns, Socket, Connect, Send, Receive, Http };

struct UplinkStatus {
    uint32_t    time_ms;      // kernel tick when the event happened
    UplinkEvent event;
    UplinkStage stage;        // UploadFailed: where
    uint8_t     ip[4];        // Joined: DHCP address
    int32_t     code;         // driver/socket return code, or HTTP status for Rejected
    int32_t     entryId;      // Uploaded: ThingSpeak entry id
    uint32_t    sampleSeq;    // Uploaded/Rejected/UploadFailed: the sample's seq
    uint32_t    durationMs;   // Joined: join time; Uploaded/Rejected: request time
    uint32_t    retryInMs;    // failures: time until the next attempt
    uint32_t    uploadsOk;    // counters since start
    uint32_t    uploadsRejected;
    uint32_t    uploadsFailed;
    uint32_t    joins;
};

static_assert(std::is_trivially_copyable_v<UplinkStatus>, "UplinkStatus must be trivially copyable");
