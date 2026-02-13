#pragma once
#include <stdint.h>
#include "config.h"

#define LOG_LEVEL_ALL       -1
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_WARNING   1
#define LOG_LEVEL_INFO      2
#define LOG_LEVEL_DEBUG     3
#define LOG_LEVEL_TRACE     4
#define LOG_LEVEL_MAX       LOG_LEVEL_TRACE


void logger (const char *file, int line, const char *func, int lvl, const char* fmt, ...);

#define LOG_FILENAME()      ((const char *)(__FILE__))
#define LOG_FUNCTION()      ((const char *)(__PRETTY_FUNCTION__))

#define log_all(FMT, ...)   logger(LOG_FILENAME(), __LINE__, LOG_FUNCTION(), LOG_LEVEL_ALL,      FMT __VA_OPT__ (,) __VA_ARGS__)
#define log_err(FMT, ...)   logger(LOG_FILENAME(), __LINE__, LOG_FUNCTION(), LOG_LEVEL_ERROR,    FMT __VA_OPT__ (,) __VA_ARGS__)
#define log_wrn(FMT, ...)   logger(LOG_FILENAME(), __LINE__, LOG_FUNCTION(), LOG_LEVEL_WARNING,  FMT __VA_OPT__ (,) __VA_ARGS__)
#define log_inf(FMT, ...)   logger(LOG_FILENAME(), __LINE__, LOG_FUNCTION(), LOG_LEVEL_INFO,     FMT __VA_OPT__ (,) __VA_ARGS__)
#define log_dbg(FMT, ...)   logger(LOG_FILENAME(), __LINE__, LOG_FUNCTION(), LOG_LEVEL_DEBUG,    FMT __VA_OPT__ (,) __VA_ARGS__)
#define log_trc(FMT, ...)   logger(LOG_FILENAME(), __LINE__, LOG_FUNCTION(), LOG_LEVEL_TRACE,    FMT __VA_OPT__ (,) __VA_ARGS__)