
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

#define JSON_SAVE2(json, s, name) json[#name] = s.name
#define JSON_LOAD2(json, s, name) s.name = json.value(#name, s.name)

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
#if PRODUCTION
    inline static std::set<std::string> debug = { ERROR };
#else
    inline static std::set<std::string> debug = { utils_t::ERROR, utils_t::DEBUG, utils_t::TRACE, utils_t::ARGS };
#endif

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

    bool validation(const config_t& config);
  };

  using bacteria_sptr_t = std::shared_ptr<bacteria_t>;

  struct resource_info_t {
    uint64_t      id     = 0;
    std::string   name   = {};
    // loss_factor

    bool validation(const config_t& config);
  };

  struct area_t {
    std::string   name              = {};
    std::string   resource          = {};
    uint64_t      x0                = {};
    uint64_t      x1                = {};
    uint64_t      y0                = {};
    uint64_t      y1                = {};
    double        radius            = {};
    double        mean              = {};
    double        sigma             = {};
    double        coef              = {};

    bool validation(const config_t& config); // TODO
  };

  struct recipe_item_t {
    std::string   name    = {};
    uint64_t      count   = {};

    bool validation(const config_t& config); // TODO
  };

  struct recipe_t {
    using in_t  = std::vector<recipe_item_t>;
    using out_t = std::vector<recipe_item_t>;

    uint64_t      id                = {};
    std::string   name              = {};
    in_t          in                = {};
    out_t         out               = {};

    bool validation(const config_t& config); // TODO
  };

  struct cell_pipe_t {
    uint64_t   pos_next_cell       = 0;
    uint64_t   resource_index      = 0;
    uint64_t   resource_velocity   = 0;

    bool validation(const config_t& config);
  };

  struct resource_t {
    uint64_t   id      = {};
    uint64_t   value   = {};

    bool validation(const config_t& config); // XXX
  };

  struct cell_t {
    using resources_t = std::vector<resource_t>;
    using pipes_t     = std::vector<cell_pipe_t>;

    uint64_t      id               = 0;
    uint64_t      pos              = 0;
    uint64_t      age              = 0;
    uint64_t      health           = 0;
    uint64_t      experience       = 0;
    uint64_t      type             = 0;
    resources_t   resources        = {};
    pipes_t       pipes            = {};

    bool validation(const config_t& config);
  };

  struct config_t {
    using debug_t       = std::set<std::string>;
    using resources_t   = std::vector<resource_info_t>;
    using areas_t       = std::vector<area_t>;
    using recipes_t     = std::vector<recipe_t>;

    uint64_t      revision           = 0;
    uint64_t      position_n         = 20;
    uint64_t      position_max       = 400;
    uint64_t      code_size          = 32;
    debug_t       debug              = { utils_t::ERROR };
    resources_t   resources          = { {1, "energy"} };
    areas_t       areas              = {};
    recipes_t     recipes            = {};

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

  inline void to_json(nlohmann::json& json, const resource_info_t& resource) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource, id);
    JSON_SAVE2(json, resource, name);
  }

  inline void from_json(const nlohmann::json& json, resource_info_t& resource) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource, id);
    JSON_LOAD2(json, resource, name);
  }

  inline void to_json(nlohmann::json& json, const area_t& area) {
    TRACE_GENESIS;
    JSON_SAVE2(json, area, name);
    JSON_SAVE2(json, area, resource);
    JSON_SAVE2(json, area, x0);
    JSON_SAVE2(json, area, x1);
    JSON_SAVE2(json, area, y0);
    JSON_SAVE2(json, area, y1);
    JSON_SAVE2(json, area, radius);
    JSON_SAVE2(json, area, mean);
    JSON_SAVE2(json, area, sigma);
    JSON_SAVE2(json, area, coef);
  }

  inline void from_json(const nlohmann::json& json, area_t& area) {
    TRACE_GENESIS;
    JSON_LOAD2(json, area, name);
    JSON_LOAD2(json, area, resource);
    JSON_LOAD2(json, area, x0);
    JSON_LOAD2(json, area, x1);
    JSON_LOAD2(json, area, y0);
    JSON_LOAD2(json, area, y1);
    JSON_LOAD2(json, area, radius);
    JSON_LOAD2(json, area, mean);
    JSON_LOAD2(json, area, sigma);
    JSON_LOAD2(json, area, coef);
  }

  inline void to_json(nlohmann::json& json, const recipe_item_t& recipe_item) {
    TRACE_GENESIS;
    JSON_SAVE2(json, recipe_item, name);
    JSON_SAVE2(json, recipe_item, count);
  }

  inline void from_json(const nlohmann::json& json, recipe_item_t& recipe_item) {
    TRACE_GENESIS;
    JSON_LOAD2(json, recipe_item, name);
    JSON_LOAD2(json, recipe_item, count);
  }

  inline void to_json(nlohmann::json& json, const recipe_t& recipe) {
    TRACE_GENESIS;
    JSON_SAVE2(json, recipe, id);
    JSON_SAVE2(json, recipe, name);
    JSON_SAVE2(json, recipe, in);
    JSON_SAVE2(json, recipe, out);
  }

  inline void from_json(const nlohmann::json& json, recipe_t& recipe) {
    TRACE_GENESIS;
    JSON_LOAD2(json, recipe, id);
    JSON_LOAD2(json, recipe, name);
    JSON_LOAD2(json, recipe, in);
    JSON_LOAD2(json, recipe, out);
  }

  inline void to_json(nlohmann::json& json, const resource_t& resource) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource, id);
    JSON_SAVE2(json, resource, value);
  }

  inline void from_json(const nlohmann::json& json, resource_t& resource) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource, id);
    JSON_LOAD2(json, resource, value);
  }

  inline void to_json(nlohmann::json& json, const cell_pipe_t& cell_pipe) {
    TRACE_GENESIS;
    JSON_SAVE2(json, cell_pipe, pos_next_cell);
    JSON_SAVE2(json, cell_pipe, resource_index);
    JSON_SAVE2(json, cell_pipe, resource_velocity);
  }

  inline void from_json(const nlohmann::json& json, cell_pipe_t& cell_pipe) {
    TRACE_GENESIS;
    JSON_LOAD2(json, cell_pipe, pos_next_cell);
    JSON_LOAD2(json, cell_pipe, resource_index);
    JSON_LOAD2(json, cell_pipe, resource_velocity);
  }

  inline void to_json(nlohmann::json& json, const config_t& config) {
    TRACE_GENESIS;
    JSON_SAVE2(json, config, revision);
    JSON_SAVE2(json, config, position_n);
    JSON_SAVE2(json, config, position_max);
    JSON_SAVE2(json, config, code_size);
    JSON_SAVE2(json, config, debug);
    JSON_SAVE2(json, config, resources);
    JSON_SAVE2(json, config, areas);
    JSON_SAVE2(json, config, recipes);
  }

  inline void from_json(const nlohmann::json& json, config_t& config) {
    TRACE_GENESIS;
    JSON_LOAD2(json, config, revision);
    JSON_LOAD2(json, config, position_n);
    JSON_LOAD2(json, config, position_max);
    JSON_LOAD2(json, config, code_size);
    JSON_LOAD2(json, config, debug);
    JSON_LOAD2(json, config, resources);
    JSON_LOAD2(json, config, areas);
    JSON_LOAD2(json, config, recipes);
  }

  inline void to_json(nlohmann::json& json, const cell_t& cell) {
    TRACE_GENESIS;
    JSON_SAVE2(json, cell, id);
    JSON_SAVE2(json, cell, pos);
    JSON_SAVE2(json, cell, age);
    JSON_SAVE2(json, cell, health);
    JSON_SAVE2(json, cell, experience);
    JSON_SAVE2(json, cell, type);
    JSON_SAVE2(json, cell, resources);
    JSON_SAVE2(json, cell, pipes);
  }

  inline void from_json(const nlohmann::json& json, cell_t& cell) {
    TRACE_GENESIS;
    JSON_LOAD2(json, cell, id);
    JSON_LOAD2(json, cell, pos);
    JSON_LOAD2(json, cell, age);
    JSON_LOAD2(json, cell, health);
    JSON_LOAD2(json, cell, experience);
    JSON_LOAD2(json, cell, type);
    JSON_LOAD2(json, cell, resources);
    JSON_LOAD2(json, cell, pipes);
  }

  inline void to_json(nlohmann::json& json, const bacteria_t& bacteria) {
    TRACE_GENESIS;
    JSON_SAVE2(json, bacteria, code);
    JSON_SAVE2(json, bacteria, rip);
    JSON_SAVE2(json, bacteria, pos);
  }

  inline void from_json(const nlohmann::json& json, bacteria_t& bacteria) {
    TRACE_GENESIS;
    JSON_LOAD2(json, bacteria, code);
    JSON_LOAD2(json, bacteria, rip);
    JSON_LOAD2(json, bacteria, pos);
  }

  /*
  template <typename type_t>
  inline void to_json(nlohmann::json& json, const std::shared_ptr<type_t>& shared) {
    TRACE_GENESIS;
    if (shared) {
      json["value"] = *shared;
    } else {
      ;
    }
  }

  template <typename type_t>
  inline void from_json(const nlohmann::json& json, std::shared_ptr<type_t>& shared) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "shared: %p", shared.get());

    shared = std::make_shared<type_t>();
    *shared = json.value("value", *shared);
  }
  */

  ////////////////////////////////////////////////////////////////////////////////

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

    JSON_LOAD(json, config);
    if (!config.validation()) {
      LOG_GENESIS(ERROR, "invalid config");
    }

    cells.assign(config.position_max, {});
    if (const auto& cells_json = json[utils_t::CELLS_PATH]; cells_json.is_array()) {
      for (const auto& cell_json : cells_json) {
        cell_t cell;
        cell = cell_json;
        if (cell.validation(config))
          cells[cell.pos] = cell;
      }
    }

    if (const auto& bacterias_json = json[utils_t::BACTERIAS_PATH]; bacterias_json.is_array()) {
      for (const auto& bacteria_json : bacterias_json) {
        auto bacteria = std::make_shared<bacteria_t>();
        *bacteria = bacteria_json;
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
    JSON_SAVE(json, config);

    auto& cells_json = json[utils_t::CELLS_PATH];
    for (const auto& cell : cells) {
      if (cell.type) {
        cells_json.push_back({});
        cells_json.back() = cell;
      }
    }

    auto& bacterias_json = json[utils_t::BACTERIAS_PATH];
    for (const auto& [_, bacteria] : bacterias) {
      if (bacteria) {
        bacterias_json.push_back({});
        bacterias_json.back() = *bacteria;
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
      bacteria->pos = 11;
      if (bacteria->validation(ctx.config))
        ctx.bacterias[bacteria->pos] = bacteria;
    }

    { // XXX
      cell_t cell;
      cell.pos = 7;
      cell.type = 1;
      if (cell.validation(ctx.config))
        ctx.cells[cell.pos] = cell;
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
    world.save();
    break;
  }

  return 0;
}

