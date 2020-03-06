
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <random>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <deque>
#include <list>
#include <set>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "../3rd_party/nlohmann/json.hpp"
#include "debug_logger.h"



#define PRODUCTION 0

#define JSON_SAVE(json, name) json[#name] = name
#define JSON_LOAD(json, name) name = json.value(#name, name)

#if PRODUCTION
  #define TRACE_GENESIS
  #define LOG_GENESIS(name, ...)
#else
  #define TRACE_GENESIS   \
    DEBUG_LOGGER(utils_t::TRACE.c_str(),   \
        logger_indent_genesis_t::indent,   \
        utils_t::debug.contains(utils_t::TRACE))
  #define LOG_GENESIS(name, ...)   \
    DEBUG_LOG(utils_t::name.c_str(),   \
        logger_indent_genesis_t::indent,   \
        utils_t::debug.contains(utils_t::name),   \
        __VA_ARGS__)
#endif



using namespace std::chrono_literals;

struct logger_indent_genesis_t   : debug_logger_n::indent_t<logger_indent_genesis_t> { };



namespace genesis_n {
  struct world_t;
  struct bot_t;

  using bot_sptr_t = std::shared_ptr<bot_t>;

  struct bot_t {
    size_t                 age            = 0;
    std::string            name           = "(empty)";
  };

  struct utils_t {
    inline static std::string CONFIG_PATH  = "config";
    inline static std::string TMP_PREFIX   = ".tmp";
    inline static std::string TRACE        = "trace";
    inline static std::string ARGS         = "args ";
    inline static std::string EPOLL        = "epoll";
    inline static std::string STATS        = "stats";
    inline static std::string ERROR        = "error";
    inline static std::string DEBUG        = "debug";
    inline static std::string MIND         = "mind ";
    inline static std::string TIME         = "time ";
    inline static size_t npos              = std::string::npos;

    inline static size_t seed = time(0);
    inline static std::set<std::string> debug = { ERROR };

    static void rename(const std::string& name_old, const std::string& name_new);
    static void remove(const std::string& name);
    static bool load(nlohmann::json& json, const std::string& name);
    static bool save(const nlohmann::json& json, const std::string& name);
  };

  struct config_t {
    int                       revision           = 0;
    std::set<std::string>     debug              = { utils_t::ERROR };

    void load(nlohmann::json& json);
    void save(nlohmann::json& json);
  };

  struct context_t {
    std::string               file_name        = "world.json";

    config_t   config;

    void load(nlohmann::json& json);
    void save(nlohmann::json& json);
  };

  struct http_parser_t {
    ;
  };

  struct world_t {
    context_t   ctx;

    void update();
    void init();
    void load();
    void save();
  };

  ////////////////////////////////////////////////////////////////////////////////

  void config_t::load(nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
      return;
    }

    JSON_LOAD(json, debug);
  }

  void config_t::save(nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      json = nlohmann::json::object();
    }

    JSON_SAVE(json, debug);
  }

  ////////////////////////////////////////////////////////////////////////////////

  void context_t::load(nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
      return;
    }

    // JSON_LOAD(json, file_name);
    config.load(json[utils_t::CONFIG_PATH]);
  }

  void context_t::save(nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      json = nlohmann::json::object();
    }

    JSON_SAVE(json, file_name);
    config.save(json[utils_t::CONFIG_PATH]);
  }

  ////////////////////////////////////////////////////////////////////////////////

  void utils_t::rename(const std::string& name_old, const std::string& name_new) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "name_old: %s", name_old.c_str());
    LOG_GENESIS(ARGS, "name_new: %s", name_new.c_str());

    std::error_code ec;
    std::filesystem::rename(name_old, name_new, ec);
    if (ec) {
      LOG_GENESIS(ERROR, "%s", ec.message().c_str());
    }
  }

  void utils_t::remove(const std::string& name) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "name: %s", name.c_str());

    std::error_code ec;
    std::filesystem::remove(name, ec);
    if (ec) {
      LOG_GENESIS(ERROR, "%s", ec.message().c_str());
    }
  }

  bool utils_t::load(nlohmann::json& json, const std::string& name) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "name: %s", name.c_str());

    try {
      std::ifstream file(name);
      file >> json;
      return true;
    } catch (const std::exception& e) {
      LOG_GENESIS(ERROR, "%s: %s", name.c_str(), e.what());
    }

    std::string name_tmp = name + TMP_PREFIX;
    try {
      std::ifstream file(name_tmp);
      file >> json;
      return true;
    } catch (const std::exception& e) {
      LOG_GENESIS(ERROR, "%s: %s", name.c_str(), e.what());
    }
    utils_t::remove(name_tmp);
    return false;
  }

  bool utils_t::save(const nlohmann::json& json, const std::string& name) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "name: %s", name.c_str());

    std::string name_tmp = name + TMP_PREFIX;
    try {
      std::ofstream file(name_tmp);
      file << std::setw(2) << json;

      utils_t::rename(name_tmp, name);
      utils_t::remove(name_tmp);
      return true;
    } catch (const std::exception& e) {
      LOG_GENESIS(ERROR, "%s", e.what());
      return false;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  void world_t::update() {
    TRACE_GENESIS;
  }

  void world_t::init() {
    TRACE_GENESIS;
    load();
    utils_t::debug = ctx.config.debug;
    save();
  }

  void world_t::load() {
    TRACE_GENESIS;
    nlohmann::json json;
    if (!utils_t::load(json, ctx.file_name)) {
      LOG_GENESIS(ERROR, "%s: can not load file", ctx.file_name.c_str());
      return;
    }
    ctx.load(json);
  }

  void world_t::save() {
    TRACE_GENESIS;
    nlohmann::json json;
    ctx.save(json);
    utils_t::save(json, ctx.file_name);
  }

  ////////////////////////////////////////////////////////////////////////////////
}



int main(int argc, char* argv[]) {

  if (argc <= 1) {
    std::cerr << "usage: " << (argc > 0 ? argv[0] : "<program>")
        << " <world_name.json>" << std::endl;
    return -1;
  }

  std::error_code ec;
  std::string file_name = std::filesystem::absolute(argv[1], ec);

  if (ec) {
    std::cerr << "invalid path: " << ec.message().c_str() << std::endl;
    return -1;
  }

  using namespace genesis_n;

  world_t world;
  world.ctx.file_name = file_name;
  world.init();

  while (true) {
    world.update();
    break;
  }

  return 0;
}

