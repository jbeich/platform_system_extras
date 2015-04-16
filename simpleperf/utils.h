#ifndef SIMPLE_PERF_UTILS_H_
#define SIMPLE_PERF_UTILS_H_

void LogError(const char* fmt, ...);
void LogInfo(const char* fmt, ...);

#define LOGE(fmt, ...) LogError(fmt, ##__VA_ARGS__)

#if defined(DEBUG)
#define LOGW(fmt, ...) LogError(fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LogInfo(fmt, ##__VA_ARGS__)
#else
#define LOGW(fmt, ...)
#define LOGI(fmt, ...)
#endif

void PrintWithSpace(int space, const char* fmt, ...);

#endif  // SIMPLE_PERF_UTILS_H_
