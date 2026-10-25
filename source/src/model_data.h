#pragma once
#ifndef LEGIO_PUBLIC_CAPTURE
#define LEGIO_PUBLIC_CAPTURE 1
#endif
#if LEGIO_PUBLIC_CAPTURE != 1
#error "Restricted captures were removed. Build with LEGIO_PUBLIC_CAPTURE=1."
#endif
#include "../models/riffsmith_clean_embedded.h"
