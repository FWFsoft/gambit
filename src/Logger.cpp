#include "Logger.h"

#include <spdlog/spdlog.h>

#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void Logger::init() {
  spdlog::set_pattern("[%T] [%^%l%$] %v");
#ifdef __EMSCRIPTEN__
  // Every line in the browser goes through the page's console and output
  // box, which is slow (about 1000 info lines at startup). Keep warnings and
  // errors by default; add ?log=info or ?log=debug to the URL for more.
  spdlog::set_level(spdlog::level::warn);
  // The page's ?log= parameter (trace, debug, info, warn, err, critical,
  // off), or "" when absent. The returned buffer belongs to Emscripten.
  const std::string param = emscripten_run_script_string(
      "(function(){try{return new URLSearchParams(window.location.search)"
      ".get('log')||'';}catch(e){return '';}})()");
  if (!param.empty()) {
    const spdlog::level::level_enum level = spdlog::level::from_str(param);
    // from_str answers "off" for anything it does not know; only honour a
    // literal "off".
    if (level != spdlog::level::off || param == "off") {
      spdlog::set_level(level);
    }
  }
#endif
  spdlog::info("Logger initialized");
}

void Logger::info(const std::string& message) { spdlog::info(message); }

void Logger::debug(const std::string& message) { spdlog::debug(message); }

void Logger::error(const std::string& message) { spdlog::error(message); }
