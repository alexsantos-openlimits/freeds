#include "Logger.h"
#include "AppConfig.h"

static Logger::Sink g_weblogSink = nullptr;

void Logger::setWeblogSink(Logger::Sink sink) { g_weblogSink = sink; }

int Logger::info(const char *format, ...) {
  char buffer[1024];
  va_list arg;
  va_start(arg, format);
  int rcode = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);

  const SystemConfig &sys = ConfigStore::get().system;

  if (sys.serialLogEnabled) {
    Serial.print(buffer);
  }
  if (sys.weblogEnabled && g_weblogSink) {
    g_weblogSink(buffer);
  }
  return rcode;
}

bool Logger::debugEnabled() { return (ConfigStore::get().system.debugFlags & 0x01) != 0; }
