
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
  struct config_t;

  struct utils_t {
    inline static std::string CONFIG_PATH      = "config";
    inline static std::string CELLS_PATH       = "cells";
    inline static std::string BACTERIAS_PATH   = "bacterias";
    inline static std::string TMP_PREFIX       = ".tmp";
    inline static std::string TRACE            = "trace";
    inline static std::string ARGS             = "args ";
    inline static std::string EPOLL            = "epoll";
    inline static std::string STATS            = "stats";
    inline static std::string ERROR            = "error";
    inline static std::string DEBUG            = "debug";
    inline static std::string MIND             = "mind ";
    inline static std::string TIME             = "time ";
    inline static size_t npos                  = std::string::npos;

    inline static size_t seed = time(0);
    inline static std::set<std::string> debug = { ERROR };

    static void rename(const std::string& name_old, const std::string& name_new);
    static void remove(const std::string& name);
    static bool load(nlohmann::json& json, const std::string& name);
    static bool save(const nlohmann::json& json, const std::string& name);
    static size_t rand_u64();
  };

  struct bacteria_t {
    using code_t = std::vector<uint8_t>;

    code_t     code      = {};
    uint64_t   rip       = 0;
    uint64_t   pos       = utils_t::npos;

    void load(const nlohmann::json& json);
    void save(nlohmann::json& json) const;
    bool validation(const config_t& config);
  };

  using bacteria_sptr_t = std::shared_ptr<bacteria_t>;

  struct pipe_t {
    uint64_t   pos_next_cell       = 0;
    uint64_t   resource_index      = 0;
    uint64_t   resource_velocity   = 0;

    void load(const nlohmann::json& json);
    void save(nlohmann::json& json) const;
    bool validation(const config_t& config);
  };

  struct cell_t {
    using resources_t = std::map<size_t/*id*/, size_t/*value*/>;
    using pipes_t     = std::map<size_t/*pos*/, pipe_t>;

    uint64_t      id               = 0;
    uint64_t      pos              = 0;
    uint64_t      age              = 0;
    uint64_t      health           = 0;
    uint64_t      experience       = 0;
    uint64_t      type             = 0;
    resources_t   resources        = {};

    void load(const nlohmann::json& json);
    void save(nlohmann::json& json) const;
    bool validation(const config_t& config);
  };

  struct config_t {
    using debug_t = std::set<std::string>;

    uint64_t    revision           = 0;
    uint64_t    position_n         = 20;
    uint64_t    position_max       = 400;
    uint64_t    code_size          = 32;
    debug_t     debug              = { utils_t::ERROR };

    void load(const nlohmann::json& json);
    void save(nlohmann::json& json) const;
    bool validation();
  };

  struct context_t {
    using cells_t = std::vector<cell_t>;
    using bacterias_t = std::map<size_t/*pos*/, bacteria_sptr_t>;

    std::string   file_name        = "/tmp/world.json";
    config_t      config           = {};
    cells_t       cells            = {};
    bacterias_t   bacterias        = {};

    void load(const nlohmann::json& json);
    void save(nlohmann::json& json) const;
  };

  /*
  struct http_parser_t {
    ;
  };
  */

  struct world_t {
    context_t   ctx;

    void update();
    void init();
    void load();
    void save();
  };

  ////////////////////////////////////////////////////////////////////////////////

  void bacteria_t::load(const nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
      return;
    }

    JSON_LOAD(json, code);
    JSON_LOAD(json, rip);
    JSON_LOAD(json, pos);
  }

  void bacteria_t::save(nlohmann::json& json) const {
    TRACE_GENESIS;

    if (!json.is_object()) {
      json = nlohmann::json::object();
    }

    JSON_SAVE(json, code);
    JSON_SAVE(json, rip);
    JSON_SAVE(json, pos);
  }

  bool bacteria_t::validation(const config_t& config) {
    TRACE_GENESIS;

    while (code.size() < config.code_size) {
      code.push_back(utils_t::rand_u64() % 0xFF);
    }
    code.resize(config.code_size);

    rip %= config.code_size;

    if (pos >= config.position_max)
      return false;

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  void pipe_t::load(const nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
      return;
    }

    JSON_LOAD(json, pos_next_cell);
    JSON_LOAD(json, resource_index);
    JSON_LOAD(json, resource_velocity);
  }

  void pipe_t::save(nlohmann::json& json) const {
    TRACE_GENESIS;

    if (!json.is_object()) {
      json = nlohmann::json::object();
    }

    JSON_SAVE(json, pos_next_cell);
    JSON_SAVE(json, resource_index);
    JSON_SAVE(json, resource_velocity);
  }

  ////////////////////////////////////////////////////////////////////////////////

  void cell_t::load(const nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
      return;
    }

    JSON_LOAD(json, id);
    JSON_LOAD(json, pos);
    JSON_LOAD(json, age);
    JSON_LOAD(json, health);
    JSON_LOAD(json, experience);
    JSON_LOAD(json, type);
    // JSON_LOAD(json, resources);
  }

  void cell_t::save(nlohmann::json& json) const {
    TRACE_GENESIS;

    if (!json.is_object()) {
      json = nlohmann::json::object();
    }

    JSON_SAVE(json, id);
    JSON_SAVE(json, pos);
    JSON_SAVE(json, age);
    JSON_SAVE(json, health);
    JSON_SAVE(json, experience);
    JSON_SAVE(json, type);
    JSON_SAVE(json, resources);
  }

  bool cell_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (pos >= config.position_max)
      return false;

    // health
    // type
    // resources

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  void config_t::load(const nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
      return;
    }

    JSON_LOAD(json, revision);
    JSON_LOAD(json, position_n);
    JSON_LOAD(json, position_max);
    JSON_LOAD(json, code_size);
    JSON_LOAD(json, debug);
  }

  void config_t::save(nlohmann::json& json) const {
    TRACE_GENESIS;

    if (!json.is_object()) {
      json = nlohmann::json::object();
    }

    JSON_SAVE(json, revision);
    JSON_SAVE(json, position_n);
    JSON_SAVE(json, position_max);
    JSON_SAVE(json, code_size);
    JSON_SAVE(json, debug);
  }

  bool config_t::validation() {
    TRACE_GENESIS;
    position_max = (position_max / position_n) * position_n;
    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  void context_t::load(const nlohmann::json& json) {
    TRACE_GENESIS;

    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
      return;
    }

    // JSON_LOAD(json, file_name);
    config.load(json[utils_t::CONFIG_PATH]);
    config.validation();

    cells.assign(config.position_max, {});
    if (const auto& cells_json = json[utils_t::CELLS_PATH]; cells_json.is_array()) {
      for (const auto& cell_json : cells_json) {
        cell_t cell;
        cell.load(cell_json);
        if (cell.validation(config))
          cells[cell.pos] = cell;
      }
    }

    bacterias = {};
    if (const auto& bacterias_json = json[utils_t::BACTERIAS_PATH]; bacterias_json.is_array()) {
      for (const auto& bacteria_json : bacterias_json) {
        auto bacteria = std::make_shared<bacteria_t>();
        bacteria->load(bacteria_json);
        if (bacteria->validation(config))
          bacterias[bacteria->pos] = bacteria;
      }
    }
  }

  void context_t::save(nlohmann::json& json) const {
    TRACE_GENESIS;

    if (!json.is_object()) {
      json = nlohmann::json::object();
    }

    JSON_SAVE(json, file_name);
    config.save(json[utils_t::CONFIG_PATH]);
    for (size_t i{}; i < cells.size(); ++i) {
      cells[i].save(json[utils_t::CELLS_PATH][i]);
    }
    {
      size_t i{};
      for (const auto& [_, bacteria] : bacterias) {
        if (bacteria) {
          bacteria->save(json[utils_t::BACTERIAS_PATH][i]);
        }
        i++;
      }
    }
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

    if (name.empty()) {
      LOG_GENESIS(ERROR, "empty name");
      return false;
    }

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

  size_t utils_t::rand_u64() {
    TRACE_GENESIS;
    static std::mt19937 gen(seed);
    static std::uniform_int_distribution<size_t> dis;
    return dis(gen);
  }

  ////////////////////////////////////////////////////////////////////////////////

  void world_t::update() {
    TRACE_GENESIS;

    { // XXX
      auto bacteria = std::make_shared<bacteria_t>();
      bacteria->pos = 12;
      ctx.bacterias[bacteria->pos] = bacteria;
    }
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
    world.save();   // XXX
    break;
  }

  return 0;
}

