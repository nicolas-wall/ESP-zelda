#pragma once
#include <Arduino.h>

// WiFi (con portal de configuración), hora por NTP y página web de recordatorios.
void netBegin();
void netLoop();
bool netConnected();
bool netPortalActive();
String netIP();
String netSSID();
void netResetWifi();
bool timeValid();
