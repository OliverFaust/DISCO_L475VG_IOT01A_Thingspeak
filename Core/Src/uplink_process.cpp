// uplink_process.cpp -- see uplink_process.hpp.
#include "uplink_process.hpp"

#include <cstring>
#include "cmsis_os2.h"
#include "Driver_WiFi.h"
#include "app_config.h"
#include "app_secrets_include.h"
#include "reset_info.h"

extern "C" ARM_DRIVER_WIFI Driver_WiFi0;     // Drivers/CMSIS-Driver/WiFi/ISM43362/WiFi_ISM43362.c

using namespace csp;

namespace {

ARM_DRIVER_WIFI& wifi = Driver_WiFi0;

constexpr uint32_t kJoinBackoffFirstMs = 5000;     // 5, 10, 20, 40, then every 60 s
constexpr uint32_t kJoinBackoffMaxMs   = 60000;
constexpr uint32_t kFailedUploadsBeforeRejoin = 3;
// The driver is initialised once and never uninitialised: in CMSIS-Driver 2.11.0, WiFi_Initialize()
// does not reset the module again after WiFi_Uninitialize() (module_initialized stays 1), and a
// failed WiFi_Initialize() leaks its two mutexes and five event flag objects (its cleanup calls
// WiFi_Uninitialize(), which returns at once because driver_initialized is still 0). Measured:
// docs/results/2026-11-12_uplink_recovery.md.
constexpr uint32_t kRecvTimeoutMs = 10000;         // per SocketRecv call
constexpr uint32_t kRestartDelayMs = 5000;         // after a failed Initialize(): time to print, then reset

#if APP_TEST_WRONG_PASSWORD
constexpr const char* kPassword = "wrong-password-for-the-recovery-test";
#else
constexpr const char* kPassword = APP_WIFI_PASSWORD;
#endif

uint32_t now() { return osKernelGetTickCount(); }   // 1 tick = 1 ms

} // namespace

static_assert(APP_UPLOAD_PERIOD_S * 1000U >= UplinkProcess::kMinPeriodMs,
              "APP_UPLOAD_PERIOD_S must be at least 15 s (ThingSpeak's minimum interval)");

void UplinkProcess::report(UplinkStatus st)
{
    st.time_ms = now();
    st.uploadsOk = uploadsOk_;
    st.uploadsRejected = uploadsRejected_;
    st.uploadsFailed = uploadsFailed_;
    st.joins = joins_;
    status_ << st;                  // KeepNewest: never blocks
}

void UplinkProcess::reportEvent(UplinkEvent e)
{
    UplinkStatus st{};
    st.event = e;
    report(st);
}

void UplinkProcess::waitKeepingLatest(uint32_t ms)
{
    const uint32_t deadline = now() + ms;
    for (;;) {
        const int32_t left = static_cast<int32_t>(deadline - now());
        if (left <= 0) return;
        Sample s{};
        RelTimeoutGuard timeout(Milliseconds(static_cast<uint32_t>(left)));
        Alternative alt(samples_ | s, timeout);
        if (alt.priSelect() == 0) {
            latest_ = s;
            haveSample_ = true;
        } else {
            return;
        }
    }
}

bool UplinkProcess::initModule()
{
    if (wifi.Initialize(nullptr) != ARM_DRIVER_OK) return false;
    return wifi.PowerControl(ARM_POWER_FULL) == ARM_DRIVER_OK;
}

bool UplinkProcess::join(uint32_t& durationMs, int32_t& code)
{
    ARM_WIFI_CONFIG_t cfg{};
    cfg.ssid = APP_WIFI_SSID;
    cfg.pass = kPassword;
    cfg.security = ARM_WIFI_SECURITY_WPA2;
    cfg.ch = 0;
    cfg.wps_method = ARM_WIFI_WPS_METHOD_NONE;
    const uint32_t t0 = now();
    code = wifi.Activate(0U, &cfg);          // interface 0 = station; blocks for several seconds
    durationMs = now() - t0;
    return code == ARM_DRIVER_OK;
}

void UplinkProcess::leave()
{
    (void)wifi.Deactivate(0U);
}

UplinkEvent UplinkProcess::upload(const Sample& s, UplinkStatus& st)
{
    st.sampleSeq = s.seq;
    const uint32_t t0 = now();

    uint8_t server[4] = {};
    uint32_t len = sizeof server;
    int32_t r = wifi.SocketGetHostByName(kThingSpeakHost, ARM_SOCKET_AF_INET, server, &len);
    if (r != ARM_DRIVER_OK) { st.stage = UplinkStage::Dns; st.code = r; return UplinkEvent::UploadFailed; }

    const int32_t sock = wifi.SocketCreate(ARM_SOCKET_AF_INET, ARM_SOCKET_SOCK_STREAM, ARM_SOCKET_IPPROTO_TCP);
    if (sock < 0) { st.stage = UplinkStage::Socket; st.code = sock; return UplinkEvent::UploadFailed; }

    UplinkEvent ev = UplinkEvent::UploadFailed;
    const uint32_t timeout = kRecvTimeoutMs;
    (void)wifi.SocketSetOpt(sock, ARM_SOCKET_SO_RCVTIMEO, &timeout, sizeof timeout);
    r = wifi.SocketConnect(sock, server, 4U, kThingSpeakPort);
    if (r != 0) {
        st.stage = UplinkStage::Connect; st.code = r;
    } else {
        const std::size_t n = tsBuildUpdate(request_, sizeof request_, APP_THINGSPEAK_WRITE_KEY,
                                            s.temperature_c, s.humidity_pct, s.pressure_hpa);
        std::size_t sent = 0;                       // send all of it, however SocketSend splits it
        while (sent < n) {
            const int32_t k = wifi.SocketSend(sock, request_ + sent, static_cast<uint32_t>(n - sent));
            if (k <= 0) { r = k; break; }
            sent += static_cast<std::size_t>(k);
        }
        if (n == 0 || sent < n) {
            st.stage = UplinkStage::Send; st.code = (n == 0) ? ARM_DRIVER_ERROR : r;
        } else {
            std::size_t got = 0;                    // bounded, always terminated
            TsResponse resp;
            while (got < sizeof response_ - 1) {
                const int32_t k = wifi.SocketRecv(sock, response_ + got, static_cast<uint32_t>(sizeof response_ - 1 - got));
                if (k <= 0) { r = k; break; }
                got += static_cast<std::size_t>(k);
                resp = tsParseResponse(response_, got);
                if (resp.bodyComplete) break;
            }
            response_[got] = '\0';
            if (!resp.headersComplete) {
                st.stage = UplinkStage::Receive; st.code = r;
            } else if (resp.status == 200 && resp.entryId > 0) {
                st.entryId = static_cast<int32_t>(resp.entryId);
                ev = UplinkEvent::Uploaded;
            } else {
                st.stage = UplinkStage::Http; st.code = resp.status; st.entryId = static_cast<int32_t>(resp.entryId);
                ev = UplinkEvent::Rejected;
            }
        }
    }
    (void)wifi.SocketClose(sock);
    st.durationMs = now() - t0;
    return ev;
}

void UplinkProcess::run()
{
    bool moduleReady = false, joined = false;
    uint32_t joinBackoff = kJoinBackoffFirstMs;
    uint32_t failedUploads = 0;
    uint32_t nextUploadAt = 0;      // first upload as soon as joined and a sample is there

    for (;;) {
        if (!moduleReady) {
            if (initModule()) {
                moduleReady = true;
                reset_info_wifi_started();      // consecutive failed starts end here
                reportEvent(UplinkEvent::ModuleReady);
            } else {
                // No retry in this run: a failed Initialize() has leaked 384 bytes of FreeRTOS heap
                // and a second one would not reset the module (driver bug above). Report, give the
                // Console time to print it, then restart the whole board; reset_info counts it.
                UplinkStatus st{}; st.event = UplinkEvent::ModuleFailed; st.retryInMs = kRestartDelayMs;
                report(st);
                waitKeepingLatest(kRestartDelayMs);
                reset_info_restart_for_wifi();
            }
            continue;
        }

        if (!joined) {
            uint32_t ms = 0; int32_t code = 0;
            if (join(ms, code)) {
                joined = true; ++joins_; failedUploads = 0;
                joinBackoff = kJoinBackoffFirstMs;
                UplinkStatus st{}; st.event = UplinkEvent::Joined; st.durationMs = ms;
                uint32_t len = sizeof st.ip;
                (void)wifi.GetOption(0U, ARM_WIFI_IP, st.ip, &len);
                report(st);
            } else {
                // Keep trying, with back-off up to one attempt per minute; never give up.
                UplinkStatus st{}; st.event = UplinkEvent::JoinFailed; st.code = code; st.durationMs = ms;
                st.retryInMs = joinBackoff;
                report(st);
                waitKeepingLatest(joinBackoff);
                joinBackoff = (joinBackoff * 2 > kJoinBackoffMaxMs) ? kJoinBackoffMaxMs : joinBackoff * 2;
            }
            continue;
        }

        // Joined: wait for the upload time, keeping the newest sample.
        const int32_t untilUpload = static_cast<int32_t>(nextUploadAt - now());
        if (untilUpload > 0) waitKeepingLatest(static_cast<uint32_t>(untilUpload));
        while (!haveSample_) waitKeepingLatest(1000);
        const Sample s = latest_;

        if (!s.valid) {
            UplinkStatus st{}; st.event = UplinkEvent::SampleInvalid; st.sampleSeq = s.seq;
            report(st);
            nextUploadAt = now() + periodMs_;
            continue;
        }

        UplinkStatus st{};
        const UplinkEvent ev = upload(s, st);
        st.event = ev;
        if (ev == UplinkEvent::Uploaded) {
            ++uploadsOk_; failedUploads = 0;
            nextUploadAt = now() + periodMs_;           // from the END of this attempt
        } else if (ev == UplinkEvent::Rejected) {
            ++uploadsRejected_; failedUploads = 0;      // the server answered: the network is fine
            nextUploadAt = now() + periodMs_;
        } else {
            ++uploadsFailed_; ++failedUploads;
            const uint32_t retry = kMinPeriodMs;         // never sooner than 15 s after an attempt
            st.retryInMs = retry;
            nextUploadAt = now() + retry;
        }
        report(st);

        if (ev == UplinkEvent::UploadFailed) {
            const bool lost = !wifi.IsConnected();      // the driver says the station is not connected
            if (lost || failedUploads >= kFailedUploadsBeforeRejoin) {
                UplinkStatus rj{}; rj.event = UplinkEvent::Rejoining;
                rj.code = lost ? 1 : 0;                  // 1: connection lost, 0: repeated failures
                report(rj);
                leave();
                joined = false; failedUploads = 0;
            }
        }
    }
}
