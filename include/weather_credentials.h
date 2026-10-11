#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.h.example"
#endif

// Keep older local secrets files buildable until an API key is configured.
#ifndef OPENWEATHER_API_KEY
#define OPENWEATHER_API_KEY ""
#endif
