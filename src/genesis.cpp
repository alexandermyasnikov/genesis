
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
    inline static std::string TMP_PREFIX       = ".tmp";
    inline static std::string TRACE            = "trace";
    inline static std::string ARGS             = "args ";
    inline static std::string EPOLL            = "epoll";
    inline static std::string STATS            = "stats";
    inline static std::string ERROR            = "error";
    inline static std::string DEBUG            = "debug";
    inline static std::string MIND             = "mind ";
    inline static std::string TIME             = "time ";
    inline static std::string DIR_LN           = "LN";
    inline static std::string DIR_RN           = "RN";
    inline static std::string DIR_NU           = "NU";
    inline static std::string DIR_ND           = "ND";
    inline static std::string DIR_LU           = "LU";
    inline static std::string DIR_LD           = "LD";
    inline static std::string DIR_RU           = "RU";
    inline static std::string DIR_RD           = "RD";
    inline static size_t npos                  = std::string::npos;
    inline static std::string EXTRACTOR        = "extractor";
    inline static std::string PRODUCER         = "producer";
    inline static std::string SPORE            = "spore";
    inline static std::string DEFENDER         = "defender";
    inline static std::string TRANSFER         = "transfer";

    inline static size_t seed = time(0);

    inline static std::set<std::string> debug =
#if PRODUCTION
        { ERROR };
#else
        { ERROR, DEBUG, TRACE, ARGS };
#endif

    inline static std::set<std::string> directions = {
        DIR_LN, DIR_RN, DIR_NU, DIR_ND, DIR_LU, DIR_LD, DIR_RU, DIR_RD };
    inline static std::set<std::string> cell_types = { EXTRACTOR, PRODUCER, SPORE, DEFENDER, TRANSFER };

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
    std::string   name        = {};
    uint64_t      stack_size  = utils_t::npos;
    // loss_factor

    bool validation(const config_t& config);
  };

  struct area_t {
    // Для типа EXTRACTOR количество добытого ресурса вычистяется по формуле
    //    y = factor * max(0, 1 - |t / radius| ^ sigma)
    //    t = ((x - x1) ^ 2 + (y - y1) ^ 2) ^ 0.5 - расстояние до центра источника
    //    y = (uint64_t) y * out.count

    std::string   name              = {};
    std::string   resource          = {};
    uint64_t      x                 = utils_t::npos;
    uint64_t      y                 = utils_t::npos;
    uint64_t      radius            = utils_t::npos;
    double        factor            = 1;
    double        sigma             = 2;

    bool validation(const config_t& config);
  };

  struct recipe_item_t {
    std::string   name    = {};
    uint64_t      count   = utils_t::npos;

    bool validation(const config_t& config);
  };

  struct recipe_t {
    using in_t  = std::map<std::string/*name*/, recipe_item_t>;
    using out_t = std::map<std::string/*name*/, recipe_item_t>;

    std::string   name              = {};
    in_t          in                = {};
    out_t         out               = {};

    bool validation(const config_t& config);
  };

  struct cell_pipe_t {
    std::string   direction      = {};
    std::string   resource       = {};
    uint64_t      velocity       = utils_t::npos;

    bool validation(const config_t& config);
  };

  struct resource_t {
    std::string   name    = {};
    uint64_t      count   = utils_t::npos;

    bool validation(const config_t& config);
  };

  struct cell_t {
    using resources_t   = std::map<std::string/*name*/,     resource_t>;
    using pipes_t       = std::map<std::string/*resource*/, cell_pipe_t>;
    using recipes_t     = std::set<std::string>;

    uint64_t      pos              = utils_t::npos;
    uint64_t      age              = 0;
    std::string   type             = {};
    resources_t   resources        = {};
    pipes_t       pipes            = {};
    recipes_t     recipes          = {};

    bool validation(const config_t& config);
  };

  struct config_t {
    using debug_t       = std::set<std::string>;
    using resources_t   = std::map<std::string/*name*/,     resource_info_t>;
    using areas_t       = std::map<std::string/*resource*/, std::vector<area_t>>;
    using recipes_t     = std::map<std::string/*name*/,     recipe_t>;

    uint64_t      revision           = 0;
    uint64_t      position_n         = 20;
    uint64_t      position_max       = 400;
    uint64_t      code_size          = 32;
    debug_t       debug              = { utils_t::ERROR };
    resources_t   resources          = {};
    areas_t       areas              = {};
    recipes_t     recipes            = {};

    bool validation();
  };

  struct context_json_t {
    using cells_t       = std::vector<cell_t>;
    using bacterias_t   = std::vector<bacteria_t>;

    config_t      config        = {};
    cells_t       cells         = {};
    bacterias_t   bacterias     = {};

    bool validation();
  };

  struct context_t {
    using cells_t       = std::map<size_t/*pos*/, cell_t>;
    using bacterias_t   = std::map<size_t/*pos*/, bacteria_sptr_t>;

    cells_t       cells            = {};
    bacterias_t   bacterias        = {};
  };

  /*
  struct http_parser_t {
    ;
  };
  */

  struct world_t {
    context_json_t   ctx_json;
    context_t        ctx;
    std::string      file_name;

    void update();
    void update_cell_total(cell_t& cell);
    void update_cell_producer(cell_t& cell);
    void init();
    void load();
    void save();
  };

  ////////////////////////////////////////////////////////////////////////////////

  inline void to_json(nlohmann::json& json, const resource_info_t& resource_info) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource_info, name);
    JSON_SAVE2(json, resource_info, stack_size);
  }

  inline void from_json(const nlohmann::json& json, resource_info_t& resource_info) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource_info, name);
    JSON_LOAD2(json, resource_info, stack_size);
  }

  inline void to_json(nlohmann::json& json, const area_t& area) {
    TRACE_GENESIS;
    JSON_SAVE2(json, area, name);
    JSON_SAVE2(json, area, resource);
    JSON_SAVE2(json, area, x);
    JSON_SAVE2(json, area, y);
    JSON_SAVE2(json, area, radius);
    JSON_SAVE2(json, area, factor);
    JSON_SAVE2(json, area, sigma);
  }

  inline void from_json(const nlohmann::json& json, area_t& area) {
    TRACE_GENESIS;
    JSON_LOAD2(json, area, name);
    JSON_LOAD2(json, area, resource);
    JSON_LOAD2(json, area, x);
    JSON_LOAD2(json, area, y);
    JSON_LOAD2(json, area, radius);
    JSON_LOAD2(json, area, factor);
    JSON_LOAD2(json, area, sigma);
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
    JSON_SAVE2(json, recipe, name);
    JSON_SAVE2(json, recipe, in);
    JSON_SAVE2(json, recipe, out);
  }

  inline void from_json(const nlohmann::json& json, recipe_t& recipe) {
    TRACE_GENESIS;
    JSON_LOAD2(json, recipe, name);
    JSON_LOAD2(json, recipe, in);
    JSON_LOAD2(json, recipe, out);
  }

  inline void to_json(nlohmann::json& json, const resource_t& resource) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource, name);
    JSON_SAVE2(json, resource, count);
  }

  inline void from_json(const nlohmann::json& json, resource_t& resource) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource, name);
    JSON_LOAD2(json, resource, count);
  }

  inline void to_json(nlohmann::json& json, const cell_pipe_t& cell_pipe) {
    TRACE_GENESIS;
    JSON_SAVE2(json, cell_pipe, direction);
    JSON_SAVE2(json, cell_pipe, resource);
    JSON_SAVE2(json, cell_pipe, velocity);
  }

  inline void from_json(const nlohmann::json& json, cell_pipe_t& cell_pipe) {
    TRACE_GENESIS;
    JSON_LOAD2(json, cell_pipe, direction);
    JSON_LOAD2(json, cell_pipe, resource);
    JSON_LOAD2(json, cell_pipe, velocity);
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
    JSON_SAVE2(json, cell, pos);
    JSON_SAVE2(json, cell, age);
    JSON_SAVE2(json, cell, type);
    JSON_SAVE2(json, cell, resources);
    JSON_SAVE2(json, cell, pipes);
    JSON_SAVE2(json, cell, recipes);
  }

  inline void from_json(const nlohmann::json& json, cell_t& cell) {
    TRACE_GENESIS;
    JSON_LOAD2(json, cell, pos);
    JSON_LOAD2(json, cell, age);
    JSON_LOAD2(json, cell, type);
    JSON_LOAD2(json, cell, resources);
    JSON_LOAD2(json, cell, pipes);
    JSON_LOAD2(json, cell, recipes);
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

  inline void to_json(nlohmann::json& json, const context_json_t& context_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, context_json, config);
    JSON_SAVE2(json, context_json, cells);
    JSON_SAVE2(json, context_json, bacterias);
  }

  inline void from_json(const nlohmann::json& json, context_json_t& context_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, context_json, config);
    JSON_LOAD2(json, context_json, cells);
    JSON_LOAD2(json, context_json, bacterias);
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool resource_info_t::validation(const config_t&) {
    TRACE_GENESIS;

    if (name.empty()) {
      LOG_GENESIS(ERROR, "invalid resource_info_t::name %s", name.c_str());
      return false;
    }

    if (stack_size == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid resource_info_t::stack_size %zd", stack_size);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool bacteria_t::validation(const config_t& config) {
    TRACE_GENESIS;

    while (code.size() < config.code_size) {
      code.push_back(utils_t::rand_u64() % 0xFF);
    }
    code.resize(config.code_size);

    rip %= config.code_size;

    if (pos >= config.position_max) {
      LOG_GENESIS(ERROR, "invalid bacteria_t::pos %zd", pos);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool cell_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (pos >= config.position_max) {
      LOG_GENESIS(ERROR, "invalid cell_t::pos %zd", pos);
      return false;
    }

    // age

    if (!utils_t::cell_types.contains(type)) {
      LOG_GENESIS(ERROR, "invalid cell_t::type %s", type.c_str());
      return false;
    }

    for (auto& [key, resource] : resources) {
      if (key != resource.name || !resource.validation(config)) {
        LOG_GENESIS(ERROR, "invalid cell_t::resources");
        return false;
      }
    }

    for (auto& [key, pipe] : pipes) {
      if (key != pipe.resource || !pipe.validation(config)) {
        LOG_GENESIS(ERROR, "invalid cell_t::pipes");
        return false;
      }
    }

    for (auto& recipe_name : recipes) {
      if (!config.recipes.contains(recipe_name)) {
        LOG_GENESIS(ERROR, "invalid cell_t::recipes %s", recipe_name.c_str());
        return false;
      }
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool area_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (name.empty()) {
      LOG_GENESIS(ERROR, "invalid area_t::name %s", name.c_str());
      return false;
    }

    if (!config.resources.contains(resource)) {
      LOG_GENESIS(ERROR, "invalid area_t::resource %s", resource.c_str());
      return false;
    }

    if (x == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid area_t::x %zd", x);
      return false;
    }

    if (y == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid area_t::y %zd", y);
      return false;
    }

    if (radius == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid area_t::radius %zd", radius);
      return false;
    }

    if (factor <= 0) {
      LOG_GENESIS(ERROR, "invalid area_t::factor %f", factor);
      return false;
    }

    if (sigma <= 0) {
      LOG_GENESIS(ERROR, "invalid area_t::sigma %f", sigma);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool recipe_item_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (!config.resources.contains(name)) {
      LOG_GENESIS(ERROR, "invalid recipe_item_t::name %s", name.c_str());
      return false;
    }

    if (!count || count == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid recipe_item_t::count %s", count);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool recipe_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (name.empty()) {
      LOG_GENESIS(ERROR, "invalid recipe_t::name %s", name.c_str());
      return false;
    }

    for (auto& [key, recipe_item] : in) {
      if (key != recipe_item.name || !recipe_item.validation(config)) {
        LOG_GENESIS(ERROR, "invalid recipe_t::in");
        return false;
      }
    }

    for (auto& [key, recipe_item] : out) {
      if (key != recipe_item.name || !recipe_item.validation(config)) {
        LOG_GENESIS(ERROR, "invalid recipe_t::out");
        return false;
      }
    }

    if (in.empty() && out.empty()) {
      LOG_GENESIS(ERROR, "invalid recipe_t::[in, out]");
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool resource_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (!config.resources.contains(name)) {
      LOG_GENESIS(ERROR, "invalid resource_t::name %s", name.c_str());
      return false;
    }

    if (count == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid resource_t::count %zd", count);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool cell_pipe_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (!utils_t::directions.contains(direction)) {
      LOG_GENESIS(ERROR, "invalid cell_pipe_t::direction %s", direction.c_str());
      return false;
    }

    if (!config.resources.contains(resource)) {
      LOG_GENESIS(ERROR, "invalid cell_pipe_t::resource %s", resource.c_str());
      return false;
    }

    if (!velocity || velocity == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid cell_pipe_t::velocity %zd", velocity);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool config_t::validation() {
    TRACE_GENESIS;
    position_max = (position_max / position_n) * position_n;

    if (position_n < 5 || position_n > 1000) {
      LOG_GENESIS(ERROR, "invalid config_t::position_n %zd", position_n);
      return false;
    }

    if (position_max < 5 * 5 || position_max > 1000 * 1000) {
      LOG_GENESIS(ERROR, "invalid config_t::position_max %zd", position_max);
      return false;
    }

    if (code_size < 5 || code_size > 1000) {
      LOG_GENESIS(ERROR, "invalid config_t::code_size %zd", code_size);
      return false;
    }

    if (debug.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::debug %zd", debug.size());
      return false;
    }

    if (resources.empty() || resources.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::resources %zd", resources.size());
      return false;
    }

    for (auto& [key, resource] : resources) {
      if (key != resource.name || !resource.validation(*this)) {
        LOG_GENESIS(ERROR, "invalid config_t::resources");
        return false;
      }
    }

    if (areas.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::areas %zd", areas.size());
      return false;
    }

    for (auto& [key, area_v] : areas) {
      for (auto& area : area_v) {
        if (key != area.resource || !area.validation(*this)) {
          LOG_GENESIS(ERROR, "invalid config_t::area");
          return false;
        }
      }
    }

    if (recipes.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::recipes %zd", recipes.size());
      return false;
    }

    for (auto& [key, recipe] : recipes) {
      if (key != recipe.name || !recipe.validation(*this)) {
        LOG_GENESIS(ERROR, "invalid config_t::recipe");
        return false;
      }
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool context_json_t::validation() {
    TRACE_GENESIS;

    if (!config.validation()) {
      LOG_GENESIS(ERROR, "invalid config_json_t::config");
      return false;
    }

    for (auto& cell : cells) {
      if (!cell.validation(config)) {
        LOG_GENESIS(ERROR, "invalid config_json_t::cells");
        return false;
      }
    }

    for (auto& bacteria : bacterias) {
      if (!bacteria.validation(config)) {
        LOG_GENESIS(ERROR, "invalid config_json_t::bacterias");
        return false;
      }
    }

    return true;
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

    for (auto& [_, cell] : ctx.cells) {
      LOG_GENESIS(DEBUG, "cell.pos %zd", cell.pos);
      try {
        update_cell_total(cell);
        if (cell.type == utils_t::EXTRACTOR) {
          update_cell_producer(cell); // TODO
        } else if (cell.type == utils_t::PRODUCER) {
          update_cell_producer(cell);
        } else {
          LOG_GENESIS(ERROR, "unknown cell.type %s", cell.type);
        }
      } catch (const std::exception& e) {
        LOG_GENESIS(ERROR, "exception %s", e.what());
      }
    }

    for (auto& [_, bacteria] : ctx.bacterias) {
      if (!bacteria) {
        continue;
      }

      LOG_GENESIS(DEBUG, "bacteria.pos %zd", bacteria->pos);
    }
  }

  void world_t::update_cell_total(cell_t& cell) {
    TRACE_GENESIS;

    // pipes

    cell.age++;
  }

  void world_t::update_cell_producer(cell_t& cell) {
    TRACE_GENESIS;

    for (const auto& recipe_name : cell.recipes) {
      const auto& recipes = ctx_json.config.recipes;
      if (!recipes.contains(recipe_name)) {
        LOG_GENESIS(ERROR, "recipe_name not found: %s", recipe_name.c_str());
        continue;
      }

      const auto& recipe = recipes.at(recipe_name);
      LOG_GENESIS(DEBUG, "recipe_name %s", recipe.name.c_str());

      bool need_produce = false;

      for (const auto& [_, recipe_item] : recipe.out) {
        if (!cell.resources.contains(recipe_item.name)) {
          continue;
        }

        auto& cell_resource = cell.resources.at(recipe_item.name);
        const auto& resource_info = ctx_json.config.resources.at(cell_resource.name);
        if (cell_resource.count < resource_info.stack_size) {
          need_produce = true;
        }
      }

      need_produce = true;

      for (const auto& [_, recipe_item] : recipe.in) {
        if (!cell.resources.contains(recipe_item.name)) {
          need_produce = false;
          break;
        }

        const auto& cell_resource = cell.resources.at(recipe_item.name);
        LOG_GENESIS(DEBUG, "resource.count %zd", cell_resource.count);
        LOG_GENESIS(DEBUG, "recipe_item.count %zd", recipe_item.count);
        if (cell_resource.count < recipe_item.count) {
          need_produce = false;
          break;
        }
      }

      if (!need_produce) {
        continue;
      }

      for (const auto& [_, recipe_item] : recipe.in) {
        auto& cell_resource = cell.resources.at(recipe_item.name);
        cell_resource.count -= recipe_item.count;
      }

      for (const auto& [_, recipe_item] : recipe.out) {
        if (!cell.resources.contains(recipe_item.name)) {
          continue;
        }

        auto& resource = cell.resources.at(recipe_item.name);
        const auto& resource_info = ctx_json.config.resources.at(cell_resource.name);
        // TODO areas
        cell_resource.count += recipe_item.count;
        cell_resource.count = std::min(cell_resource.count, resource_info.stack_size);
      }
    }
  }

  void world_t::init() {
    TRACE_GENESIS;
    load();
    utils_t::debug = ctx_json.config.debug;
    save();
  }

  void world_t::load() {
    TRACE_GENESIS;
    nlohmann::json json;
    if (!utils_t::load(json, file_name)) {
      LOG_GENESIS(ERROR, "can not load file %s", file_name.c_str());
      throw std::runtime_error("invalid json");
    }

    ctx_json = json;

    if (!ctx_json.validation()) {
      LOG_GENESIS(ERROR, "invalid config");
      throw std::runtime_error("invalid config");
    }

    ctx.cells = {};
    for (auto& cell : ctx_json.cells) {
      if (cell.validation(ctx_json.config)) {
        ctx.cells[cell.pos] = cell;
      }
    }

    ctx.bacterias = {};
    for (auto& bacteria : ctx_json.bacterias) {
      if (bacteria.validation(ctx_json.config)) {
        ctx.bacterias[bacteria.pos] = std::make_shared<bacteria_t>(bacteria);
      }
    }
  }

  void world_t::save() {
    TRACE_GENESIS;

    ctx_json.cells = {};
    ctx_json.cells.reserve(ctx.cells.size());
    for (auto& [_, cell] : ctx.cells) {
      if (cell.validation(ctx_json.config)) {
        ctx_json.cells.push_back(cell);
      }
    }

    ctx_json.bacterias = {};
    ctx_json.bacterias.reserve(ctx.bacterias.size());
    for (auto& [_, bacteria] : ctx.bacterias) {
      if (bacteria->validation(ctx_json.config)) {
        ctx_json.bacterias.push_back(*bacteria);
      }
    }

    nlohmann::json json;
    json = ctx_json;
    utils_t::save(json, file_name);
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
  world.file_name = file_name;
  world.init();

  for (size_t i{}; i < 10; ++i) {
    world.update();
    world.save();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "end" << std::endl;

  return 0;
}

