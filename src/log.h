#pragma once

//simple debug logging

enum { LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL };
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define log_trace(...) platform_config.log_log(false, false, LOG_TRACE, __FILENAME__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_debug(...) platform_config.log_log(false, false, LOG_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_info(...) platform_config.log_log(false, false, LOG_INFO, __FILENAME__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_warn(...) platform_config.log_log(false, false, LOG_WARN, __FILENAME__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_error(...) platform_config.log_log(false, false, LOG_ERROR, __FILENAME__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_fatal(...) platform_config.log_log(false, false, LOG_FATAL, __FILENAME__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_append(...) platform_config.log_log(true, false, LOG_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_end() platform_config.log_log(true, true, LOG_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, "\r\n")