/*
 * app_secrets_include.h -- include this instead of app_secrets.h, for a clear message if the
 * file has not been created yet (it is git-ignored, so a fresh clone does not have it).
 */
#ifndef APP_SECRETS_INCLUDE_H
#define APP_SECRETS_INCLUDE_H

#if __has_include("app_secrets.h")
#include "app_secrets.h"
#else
#error "Core/Inc/app_secrets.h is missing. In the project folder run:  cp Core/Inc/app_secrets.h.template Core/Inc/app_secrets.h  then enter your Wi-Fi SSID, password and ThingSpeak write key in it (docs/03-build-and-run.md)."
#endif

#endif /* APP_SECRETS_INCLUDE_H */
