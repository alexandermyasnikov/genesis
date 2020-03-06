
#pragma once

#include "parameters.h"

namespace genesis_n {

  #define JSON_SAVE(json, name) json[#name] = name
  #define JSON_LOAD(json, name) name = json.value(#name, name)
  
  #define TRACE_GENESIS {   \
    if (utils_t::parameters.debug.contains(utils_t::TRACE)) {   \
      DEBUG_LOGGER(utils_t::TRACE.c_str(), logger_indent_genesis_t::indent);   \
    }   \
  }
  #define LOG_GENESIS(name, ...) {   \
    if (utils_t::parameters.debug.contains(utils_t::name)) {   \
      DEBUG_LOG(utils_t::name.c_str(), logger_indent_genesis_t::indent, __VA_ARGS__);   \
    }   \
  }

  struct utils_t {
    inline static size_t seed = time(0);
    inline static parameters_t parameters = {};

    inline static std::string TRACE = "trace";
    inline static std::string EPOLL = "epoll";
    inline static std::string STATS = "stats";
    inline static std::string ERROR = "error";
    inline static std::string DEBUG = "debug";
    inline static std::string MIND  = "mind ";
    inline static std::string TIME  = "time ";
    inline static size_t npos       = std::string::npos;

    static double rand_double();
    static size_t rand_u64();
    static size_t position(size_t position, size_t direction);
    static std::pair<size_t, size_t> position_to_xy(size_t position);
    static size_t position_from_xy(size_t x, size_t y);
    static size_t n();
    static size_t m();
    static void update_parameters(const std::string& name);
    static void rename(const std::string& name_old, const std::string& name_new);
    static void remove(const std::string& name);
    static bool load(nlohmann::json& json, const std::string& name);
    static bool save(const nlohmann::json& json, const std::string& name);
  };


}

