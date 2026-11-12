// wifi_upload_test.hpp -- phase 3, step 2: one ThingSpeak update, outside the process network.
//
// Reads one sample from the sensors, joins the Wi-Fi network in Core/Inc/app_secrets.h, resolves
// api.thingspeak.com, sends one update (GET /update ... HTTP/1.1, Connection: close), prints the
// HTTP status and the entry id, closes the socket and leaves the network. Started by MainApp
// instead of the Sensors/Console network when APP_DIAGNOSTIC_MODE is APP_DIAG_WIFI_ONE_UPLOAD
// (Core/Inc/app_config.h); in that mode it is the only user of I2C2 and of the Wi-Fi module, and
// the only printer.
//
// Never printed: the Wi-Fi password, the write key (only its first 4 characters), the SSID (only
// its first 3 characters).
#pragma once

#include "csp/csp4cmsis.h"

class WifiUploadTest : public csp::CSProcessStatic<1024> {   // 1024 words = 4 KB (printf, buffers static)
public:
    const char* name() const override { return "UploadTest"; }
    void run() override;
};
