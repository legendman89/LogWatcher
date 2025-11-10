#pragma once

#include <cstddef>

namespace Logwatch {

#define FOREACH_BOOL_SETTING(S) \
    S(deepScan,                 false) \
    S(watchPapyrus,             true) \
    S(autoBoostOnBacklog,       true) \
    S(persistPins,              true) \

#define FOREACH_SIZE_SETTING(S) \
    S(cacheCap,					1000) \
    S(pollIntervalMs,			500) \
    S(maxChunkKB,               2048) \
    S(maxLineKB,                64) \
    S(papyrusMaxChunkKB,        8192) \
    S(papyrusMaxLineKB,         256) \
    S(backlogBoostThresholdMB,	8) \
    S(maxBoostCapMB,            10) \

#define FOREACH_FLT_SETTING(S) \
    S(backlogBoostFactor,       2.0f) \


#define BOOL2DEF(S, D)  bool     S = D;
#define SIZE2DEF(S, D)  int      S = D;
#define FLT2DEF(S, D)   float    S = D;

    struct LogWatcherSettings {

        FOREACH_BOOL_SETTING(BOOL2DEF);
        FOREACH_SIZE_SETTING(SIZE2DEF);
        FOREACH_FLT_SETTING(FLT2DEF);

    };

}
