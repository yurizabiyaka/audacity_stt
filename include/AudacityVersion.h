#pragma once

// Audacity version constants for module compatibility
#define AUDACITY_VERSION 3
#define AUDACITY_RELEASE 7
#define AUDACITY_REVISION 1
#define AUDACITY_BUILD_LEVEL 0

// Construct version string
#define AUDACITY_MAKE_VERSION_STRING(a,b,c) #a "." #b "." #c
#define AUDACITY_VERSION_STRING L ## AUDACITY_MAKE_VERSION_STRING(AUDACITY_VERSION, AUDACITY_RELEASE, AUDACITY_REVISION)
