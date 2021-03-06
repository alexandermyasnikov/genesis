
#include <unordered_set>
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
#include <regex>
#include <list>
#include <set>

#include "../3rd_party/nlohmann/json.hpp"
#include "debug_logger.h"



// #define PRODUCTION
// #define VALGRIND

#define JSON_SAVE(json, name)   json[#name] = name
#define JSON_LOAD(json, name)   name = json.value(#name, name)

#define JSON_SAVE2(json, s, name)                  json[#name] = s.name
#define JSON_LOAD2(json, s, name)                  s.name = json.value(#name, s.name)

#define JSON_LOAD3(json, s, name, default_value)   s.name = json.value(#name, default_value)

#define SAFE_INDEX(cont, ind)   cont[(ind) % cont.size()]

#ifdef PRODUCTION
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

struct logger_indent_genesis_t : debug_logger_n::indent_t<logger_indent_genesis_t> { };



namespace genesis_n {
  struct config_t;
  struct world_t;

  using xy_pos_t    = std::pair<size_t, size_t>;
  using res_val_t   = int16_t;

  struct utils_t {
    inline static std::string TMP_SUFFIX       = ".tmp";
    inline static std::string TRACE            = "trace";
    inline static std::string ARGS             = "args ";
    inline static std::string STATS            = "stats";
    inline static std::string ERROR            = "error";
    inline static std::string DEBUG            = "debug";
    inline static std::string MIND             = "mind ";
    inline static std::string TIME             = "time ";
    inline static std::string DIR_L            = "L";
    inline static std::string DIR_R            = "R";
    inline static std::string DIR_U            = "U";
    inline static std::string DIR_D            = "D";
    inline static std::string DIR_LU           = "LU";
    inline static std::string DIR_LD           = "LD";
    inline static std::string DIR_RU           = "RU";
    inline static std::string DIR_RD           = "RD";
    inline static std::string ENERGY           = "energy";
    inline static size_t npos                  = std::string::npos;
    inline static size_t direction_max         = 9;
    inline static size_t REG_RIP1B             = 0;
    inline static size_t REGS_SIZE_MIN         = 10;
    inline static size_t RES_ENERGY            = 0;

    inline static size_t seed = {};

    inline static std::set<std::string> debug = { ERROR };

    inline static std::vector<std::string> directions = {
        DIR_R, DIR_U, DIR_U, DIR_LU, DIR_L, DIR_LD, DIR_D, DIR_RD };

    static void rename(const std::string& name_old, const std::string& name_new);
    static void remove(const std::string& name);
    static bool load(nlohmann::json& json, const std::string& name, bool binary = false);
    static bool save(const nlohmann::json& json, const std::string& name, bool binary = false);
    static uint64_t rand_u64();
    static uint64_t hash_mix(uint64_t h);
    static uint64_t fasthash64(const void *buf, size_t len, uint64_t seed);

    static size_t distance(const xy_pos_t& pos1, const xy_pos_t& pos2);

    static void normalize(auto& value, auto value_min, auto value_max) {
      TRACE_GENESIS;
      if (value < value_min) {
        value = value_min;
      } else if (value > value_max) {
        value = value_max;
      }
    }

    static inline void load_data(auto& value, const std::vector<uint8_t>& data, uint64_t index) {
      TRACE_GENESIS;
      auto dst = reinterpret_cast<void*>(&value);
      auto src = reinterpret_cast<const void*>(data.data() + (index % (data.size() - sizeof(value))));
      std::memcpy(dst, src, sizeof(value));
    }

    static inline void save_data(auto value, std::vector<uint8_t>& data, uint64_t index) {
      TRACE_GENESIS;
      auto dst = reinterpret_cast<void*>(data.data() + (index % (data.size() - sizeof(value))));
      auto src = reinterpret_cast<const void*>(&value);
      std::memcpy(dst, src, sizeof(value));
    }
  };

  ////////////////////////////////////////////////////////////////////////////////

  struct config_json_wrapper_t {
    config_t&   config;

    config_json_wrapper_t(config_t& config) : config(config) { }
    bool load(const std::string& file_name);
    bool save(const std::string& file_name);
  };

  ////////////////////////////////////////////////////////////////////////////////

  struct world_json_wrapper_t {
    world_t&   world;

    world_json_wrapper_t(world_t& world) : world(world) { }
    bool load(const std::string& file_name);
    bool save(const std::string& file_name);
  };

  ////////////////////////////////////////////////////////////////////////////////

  struct area_t {
    xy_pos_t      pos               = {};
    size_t        radius            = 100;
    double        frequency         = 0.01;
    double        factor            = 1;
    double        sigma             = 2;
  };

  struct resource_info_t {
    using areas_t = std::vector<area_t>;

    std::string   name          = {};
    res_val_t     stack_size    = 1000;
    areas_t       areas         = {};
  };

  struct recipe_json_t {
    using in_out_t = std::vector<std::pair<std::string/*name*/, res_val_t>>;

    std::string   name        = {};
    bool          available   = true;
    in_out_t      in_out      = {};
  };

  struct stats_t {
    uint64_t   age              = {};
    uint64_t   microbes_count   = {};
    double     microbes_age_avg = {};
    uint64_t   time_update      = {};
  };

  ////////////////////////////////////////////////////////////////////////////////

  struct microbe_t {
    using resources_t   = std::vector<res_val_t>;
    using data_t        = std::vector<uint8_t>;

    bool          alive                = false;
    data_t        code;
    data_t        regs;
    uint64_t      family; // TODO type
    resources_t   resources;
    xy_pos_t      pos;
    res_val_t     age;
    uint8_t       direction;
    int8_t        energy_remaining;

    void init(const config_t& config);
    bool validation(const config_t& config);
  };

  struct recipe_t {
    using in_out_t = std::vector<std::pair<size_t/*ind*/, res_val_t>>;

    std::string   name        = {};
    bool          available   = true;
    in_out_t      in_out      = {};
  };

  struct config_t {
    using debug_t       = std::set<std::string>;
    using resources_t   = std::vector<resource_info_t>;
    using recipes_t     = std::vector<recipe_t>;

    size_t        x_max;
    size_t        y_max;
    size_t        code_size;
    size_t        regs_size;
    size_t        health_max; // deprecated
    size_t        age_max; // XXX type
    size_t        age_max_delta;
    size_t        energy_remaining;
    size_t        interval_update_world_ms;
    size_t        interval_save_world_ms;
    double        mutation_probability;
    size_t        seed;
    debug_t       debug;
    resources_t   resources;
    recipes_t     recipes;
    size_t        recipe_init;
    size_t        recipe_step;
    size_t        recipe_clone;
    xy_pos_t      spawn_pos;
    size_t        spawn_radius;
    size_t        spawn_min_count;
    size_t        spawn_max_count;
    bool          binary_data; // deprecated
  };

  struct cell_t {
    using resources_t = std::vector<res_val_t>;

    microbe_t     microbe;
    resources_t   resources;
  };

  struct world_t {
    using cells_t = std::vector<cell_t>;

    std::string    config_file_name   = {};
    std::string    world_file_name    = {};

    config_t       config             = {};
    cells_t        cells              = {};
    stats_t        stats              = {};

    size_t         time_ms            = {};
    size_t         update_world_ms    = {};
    size_t         save_world_ms      = {};

    void update();
    void update_world();
    void update_mind(microbe_t& microbe);
    bool update_mind_recipe(const recipe_t& recipe, microbe_t& microbe);
    void init();
    void load_config();
    void save_config();
    void load_data();
    void save_data();

    size_t xy_pos_to_ind(const xy_pos_t& pos) {
      TRACE_GENESIS;
      return pos.first + config.x_max * pos.second;
    }

    xy_pos_t xy_pos_from_ind(size_t ind) {
      TRACE_GENESIS;
      return {ind % config.x_max, ind / config.x_max};
    }

    bool pos_valid(const xy_pos_t& pos) const {
      TRACE_GENESIS;
      return pos.first < config.x_max && pos.second < config.y_max;
    }

    xy_pos_t pos_next(const xy_pos_t& pos, uint8_t direction) const {
      TRACE_GENESIS;

      direction %= utils_t::direction_max;
      int dx = 0;
      int dy = 0;
      switch (direction) {
        case 0:  dx =  1; dy =  0; break;
        case 1:  dx =  1; dy = -1; break;
        case 2:  dx =  0; dy = -1; break;
        case 3:  dx = -1; dy = -1; break;
        case 4:  dx = -1; dy =  0; break;
        case 5:  dx = -1; dy =  1; break;
        case 6:  dx =  0; dy =  1; break;
        case 7:  dx =  1; dy =  1; break;
        default: dx =  0; dy =  0; break;
      }

      xy_pos_t pos_next = {pos.first + dx, pos.second + dy};

      if (!pos_valid(pos_next)) {
        return pos;
      }

      return pos_next;
    }
  };

  ////////////////////////////////////////////////////////////////////////////////

  inline void to_json(nlohmann::json& json, const area_t& area_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, area_json, pos);
    JSON_SAVE2(json, area_json, radius);
    JSON_SAVE2(json, area_json, frequency);
    JSON_SAVE2(json, area_json, factor);
    JSON_SAVE2(json, area_json, sigma);
  }

  inline void from_json(const nlohmann::json& json, area_t& area_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, area_json, pos);
    JSON_LOAD2(json, area_json, radius);
    JSON_LOAD2(json, area_json, frequency);
    JSON_LOAD2(json, area_json, factor);
    JSON_LOAD2(json, area_json, sigma);
  }

  inline void to_json(nlohmann::json& json, const resource_info_t& resource_info_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource_info_json, name);
    JSON_SAVE2(json, resource_info_json, stack_size);
    JSON_SAVE2(json, resource_info_json, areas);
  }

  inline void from_json(const nlohmann::json& json, resource_info_t& resource_info_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource_info_json, name);
    JSON_LOAD2(json, resource_info_json, stack_size);
    JSON_LOAD2(json, resource_info_json, areas);
  }

  inline void to_json(nlohmann::json& json, const recipe_json_t& recipe_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, recipe_json, name);
    JSON_SAVE2(json, recipe_json, available);
    JSON_SAVE2(json, recipe_json, in_out);
  }

  inline void from_json(const nlohmann::json& json, recipe_json_t& recipe_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, recipe_json, name);
    JSON_LOAD2(json, recipe_json, available);
    JSON_LOAD2(json, recipe_json, in_out);
  }

  inline void to_json(nlohmann::json& json, const stats_t& stats) {
    TRACE_GENESIS;
    JSON_SAVE2(json, stats, age);
    JSON_SAVE2(json, stats, microbes_count);
    JSON_SAVE2(json, stats, microbes_age_avg);
    JSON_SAVE2(json, stats, time_update);
  }

  inline void from_json(const nlohmann::json& json, stats_t& stats) {
    TRACE_GENESIS;
    JSON_LOAD2(json, stats, age);
    JSON_LOAD2(json, stats, microbes_count);
    JSON_LOAD2(json, stats, microbes_age_avg);
    JSON_LOAD2(json, stats, time_update);
  }

  inline void to_json(nlohmann::json& json, const microbe_t& microbe) {
    TRACE_GENESIS;
    JSON_SAVE2(json, microbe, alive);
    JSON_SAVE2(json, microbe, code);
    JSON_SAVE2(json, microbe, regs);
    JSON_SAVE2(json, microbe, family);
    JSON_SAVE2(json, microbe, resources);
    JSON_SAVE2(json, microbe, pos);
    JSON_SAVE2(json, microbe, age);
    JSON_SAVE2(json, microbe, direction);
    JSON_SAVE2(json, microbe, energy_remaining);
  }

  inline void from_json(const nlohmann::json& json, microbe_t& microbe) {
    TRACE_GENESIS;
    JSON_LOAD2(json, microbe, alive);
    JSON_LOAD2(json, microbe, code);
    JSON_LOAD2(json, microbe, regs);
    JSON_LOAD2(json, microbe, family);
    JSON_LOAD2(json, microbe, resources);
    JSON_LOAD2(json, microbe, pos);
    JSON_LOAD2(json, microbe, age);
    JSON_LOAD2(json, microbe, direction);
    JSON_LOAD2(json, microbe, energy_remaining);
  }

  inline void to_json(nlohmann::json& json, const cell_t& cell) {
    TRACE_GENESIS;
    JSON_SAVE2(json, cell, microbe);
    JSON_SAVE2(json, cell, resources);
  }

  inline void from_json(const nlohmann::json& json, cell_t& cell) {
    TRACE_GENESIS;
    JSON_LOAD2(json, cell, microbe);
    JSON_LOAD2(json, cell, resources);
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

  bool utils_t::load(nlohmann::json& json, const std::string& name, bool binary) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "name: %s", name.c_str());

    if (name.empty()) {
      LOG_GENESIS(ERROR, "empty name");
      return false;
    }

    try {
      std::ifstream file(name);
      std::istream_iterator<uint8_t> begin(file);
      std::istream_iterator<uint8_t> end;
      std::string data(begin, end);
      if (binary) {
        json = nlohmann::json::from_msgpack(data);
      } else {
        json = nlohmann::json::parse(data);
      }
      return true;
    } catch (const std::exception& e) {
      LOG_GENESIS(ERROR, "%s: %s", name.c_str(), e.what());
    }

    std::string name_tmp = name + TMP_SUFFIX;
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

  bool utils_t::save(const nlohmann::json& json, const std::string& name, bool binary) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "name: %s", name.c_str());

    std::string name_tmp = name + TMP_SUFFIX;
    try {
      std::ofstream file(name_tmp);
      std::ostream_iterator<uint8_t> it_out(file);
      if (binary) {
        auto data = nlohmann::json::to_msgpack(json);
        std::copy(data.begin(), data.end(), it_out);
      } else {
        auto data = json.dump(2);
        std::copy(data.begin(), data.end(), it_out);
      }

      utils_t::rename(name_tmp, name);
      utils_t::remove(name_tmp);
      return true;
    } catch (const std::exception& e) {
      LOG_GENESIS(ERROR, "%s", e.what());
      return false;
    }
  }

  uint64_t utils_t::rand_u64() {
    static std::mt19937 gen(seed);
    static std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
  }

  uint64_t utils_t::hash_mix(uint64_t h) {
    h ^= h >> 23;
    h *= 0x2127599bf4325c37ULL;
    h ^= h >> 47;
    return h;
  }

  uint64_t utils_t::fasthash64(const void *buf, size_t len, uint64_t seed) {
    const uint64_t m    = 0x880355f21e6d1965ULL;
    const uint64_t *pos = (const uint64_t *) buf;
    const uint64_t *end = pos + (len / 8);
    const unsigned char *pos2;
    uint64_t h = seed ^ (len * m);
    uint64_t v;

    while (pos != end) {
      v  = *pos++;
      h ^= hash_mix(v);
      h *= m;
    }

    pos2 = (const unsigned char*)pos;
    v = 0;

    switch (len & 7) {
      case 7: v ^= (uint64_t)pos2[6] << 48; [[fallthrough]];
      case 6: v ^= (uint64_t)pos2[5] << 40; [[fallthrough]];
      case 5: v ^= (uint64_t)pos2[4] << 32; [[fallthrough]];
      case 4: v ^= (uint64_t)pos2[3] << 24; [[fallthrough]];
      case 3: v ^= (uint64_t)pos2[2] << 16; [[fallthrough]];
      case 2: v ^= (uint64_t)pos2[1] << 8;  [[fallthrough]];
      case 1: v ^= (uint64_t)pos2[0];
              h ^= hash_mix(v);
              h *= m;
    }

    return hash_mix(h);
  }

  size_t utils_t::distance(const xy_pos_t& pos1, const xy_pos_t& pos2) {
    auto [x1, y1] = pos1;
    auto [x2, y2] = pos2;
    size_t dx = (x2 >= x1) ? (x2 - x1) : (x1 - x2);
    size_t dy = (y2 >= y1) ? (y2 - y1) : (y1 - y2);
    return std::hypot(dx, dy);
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool config_json_wrapper_t::load(const std::string& file_name) {
    TRACE_GENESIS;

    nlohmann::json json;
    if (!utils_t::load(json, file_name)) {
      LOG_GENESIS(ERROR, "can not load file %s", file_name.c_str());
      return false;
    }

    std::map<std::string, size_t>   resources_names;
    std::map<std::string, size_t>   recipes_names;
    std::vector<recipe_json_t>      recipes;

    config.x_max = 500;
    JSON_LOAD2(json, config, x_max);
    if (config.x_max < 5 || config.x_max > 100000) {
      LOG_GENESIS(ERROR, "invalid x_max %zd", config.x_max);
      return false;
    }

    config.y_max = 500;
    JSON_LOAD2(json, config, y_max);
    if (config.y_max < 5 || config.y_max > 100000) {
      LOG_GENESIS(ERROR, "invalid y_max %zd", config.y_max);
      return false;
    }

    config.code_size = 64;
    JSON_LOAD2(json, config, code_size);
    if (config.code_size < sizeof(uint64_t) || config.code_size > 0xFF) {
      LOG_GENESIS(ERROR, "invalid code_size %zd", config.code_size);
      return false;
    }

    config.regs_size = 32;
    JSON_LOAD2(json, config, regs_size);
    if (config.regs_size < utils_t::REGS_SIZE_MIN || config.regs_size > 0xFF) {
      LOG_GENESIS(ERROR, "invalid regs_size %zd", config.regs_size);
      return false;
    }

    config.age_max = 1000;
    JSON_LOAD2(json, config, age_max);

    config.age_max_delta = 1;
    JSON_LOAD2(json, config, age_max_delta);

    config.energy_remaining = 3;
    JSON_LOAD2(json, config, energy_remaining);

    config.interval_update_world_ms = 1;
    JSON_LOAD2(json, config, interval_update_world_ms);

    config.interval_save_world_ms = 10 * 60 * 1000;
    JSON_LOAD2(json, config, interval_save_world_ms);

    config.mutation_probability = 0.1;
    JSON_LOAD2(json, config, mutation_probability);
    if (config.mutation_probability < 0) {
      LOG_GENESIS(ERROR, "invalid mutation_probability %f", config.mutation_probability);
      return false;
    }

    config.seed = 0;
    JSON_LOAD2(json, config, seed);

    config.debug = {utils_t::ERROR};
    JSON_LOAD2(json, config, debug);

    config.resources = {}; // TODO
    JSON_LOAD2(json, config, resources);

    config.spawn_pos = {100, 100};
    JSON_LOAD2(json, config, spawn_pos);

    config.spawn_radius = 100;
    JSON_LOAD2(json, config, spawn_radius);

    config.spawn_min_count = 1000;
    JSON_LOAD2(json, config, spawn_min_count);

    config.spawn_max_count = 1001;
    JSON_LOAD2(json, config, spawn_max_count);

    config.binary_data = true;
    JSON_LOAD2(json, config, binary_data);

    for (size_t i{}; i < config.resources.size(); ++i) {
      resources_names[config.resources[i].name] = i;
    }

    config.recipes = {}; // TODO
    JSON_LOAD(json, recipes);
    for (const auto& recipe_tmp : recipes) {
      config.recipes.push_back({});
      auto& recipe = config.recipes.back();
      recipe.name      = recipe_tmp.name;
      recipe.available = recipe_tmp.available;
      for (const auto& [key, val] : recipe_tmp.in_out) {
        if (!resources_names.contains(key)) {
          LOG_GENESIS(ERROR, "invalid in_out %s", key.c_str());
          return false;
        }
        recipe.in_out.push_back({resources_names.at(key), val});
      }
    }
    if (config.recipes.empty() || config.recipes.size() > 0xFF) {
      LOG_GENESIS(ERROR, "invalid recipes %zd", config.recipes.size());
      return false;
    }

    for (size_t i{}; i < config.recipes.size(); ++i) {
      recipes_names[config.recipes[i].name] = i;
    }

    std::string recipe_init;
    JSON_LOAD(json, recipe_init);
    if (!recipes_names.contains(recipe_init)) {
      LOG_GENESIS(ERROR, "invalid recipe_init %s", recipe_init.c_str());
      return false;
    }
    config.recipe_init = recipes_names[recipe_init];

    std::string recipe_step;
    JSON_LOAD(json, recipe_step);
    if (!recipes_names.contains(recipe_step)) {
      LOG_GENESIS(ERROR, "invalid recipe_step %s", recipe_step.c_str());
      return false;
    }
    config.recipe_step = recipes_names[recipe_step];

    std::string recipe_clone;
    JSON_LOAD(json, recipe_clone);
    if (!recipes_names.contains(recipe_clone)) {
      LOG_GENESIS(ERROR, "invalid recipe_clone %s", recipe_clone.c_str());
      return false;
    }
    config.recipe_clone = recipes_names[recipe_clone];

    return true;
  }

  bool config_json_wrapper_t::save(const std::string& file_name) {
    TRACE_GENESIS;

    nlohmann::json json = {};

    JSON_SAVE2(json, config, x_max);
    JSON_SAVE2(json, config, y_max);
    JSON_SAVE2(json, config, code_size);
    JSON_SAVE2(json, config, regs_size);
    JSON_SAVE2(json, config, age_max);
    JSON_SAVE2(json, config, age_max_delta);
    JSON_SAVE2(json, config, energy_remaining);
    JSON_SAVE2(json, config, interval_update_world_ms);
    JSON_SAVE2(json, config, interval_save_world_ms);
    JSON_SAVE2(json, config, mutation_probability);
    JSON_SAVE2(json, config, seed);
    JSON_SAVE2(json, config, debug);
    JSON_SAVE2(json, config, resources);
    JSON_SAVE2(json, config, spawn_pos);
    JSON_SAVE2(json, config, spawn_radius);
    JSON_SAVE2(json, config, spawn_min_count);
    JSON_SAVE2(json, config, spawn_max_count);
    JSON_SAVE2(json, config, binary_data);

    std::vector<recipe_json_t> recipes = {};
    for (const auto& recipe_tmp : config.recipes) {
      recipes.push_back({});
      auto& recipe = recipes.back();
      recipe.name      = recipe_tmp.name;
      recipe.available = recipe_tmp.available;
      for (const auto& [ind, val] : recipe_tmp.in_out) {
        recipe.in_out.push_back({config.resources.at(ind).name, val});
      }
    }
    JSON_SAVE(json, recipes);

    auto recipe_init = config.recipes.at(config.recipe_init).name;
    JSON_SAVE(json, recipe_init);

    auto recipe_step = config.recipes.at(config.recipe_step).name;
    JSON_SAVE(json, recipe_step);

    auto recipe_clone = config.recipes.at(config.recipe_clone).name;
    JSON_SAVE(json, recipe_clone);

    if (!utils_t::save(json, file_name)) {
      LOG_GENESIS(ERROR, "can not save file %s", file_name.c_str());
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool world_json_wrapper_t::load(const std::string& file_name) {
    TRACE_GENESIS;

    nlohmann::json json;
    if (!utils_t::load(json, file_name, world.config.binary_data)) {
      LOG_GENESIS(ERROR, "can not load file %s", file_name.c_str());
      json = nlohmann::json::object();
    }

    JSON_LOAD2(json, world, cells);

    world.cells.resize(world.config.x_max * world.config.y_max);
    for (auto& cell : world.cells) {
      cell.resources.resize(world.config.resources.size());
      for (size_t ind{}; ind < cell.resources.size(); ++ind) {
        auto& resource = cell.resources[ind];
        auto stack_size = world.config.resources[ind].stack_size;
        utils_t::normalize(resource, 0, stack_size);
      }
      if (!cell.microbe.validation(world.config)) {
        cell.microbe = {};
      }
    }

    JSON_LOAD2(json, world, stats);

    return true;
  }

  bool world_json_wrapper_t::save(const std::string& file_name) {
    TRACE_GENESIS;

    nlohmann::json json = {};

    JSON_SAVE2(json, world, cells);
    JSON_SAVE2(json, world, stats);

    if (!utils_t::save(json, file_name, world.config.binary_data)) {
      LOG_GENESIS(ERROR, "can not save file %s", file_name.c_str());
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  void microbe_t::init(const config_t& config) {
    TRACE_GENESIS;

    alive              = true;
    family             = utils_t::rand_u64();
    // code
    // regs
    resources.assign(config.resources.size(), 0);
    pos                = {utils_t::rand_u64() % config.x_max, utils_t::rand_u64() % config.y_max};
    age                = config.age_max + utils_t::rand_u64() % config.age_max_delta - 0.5 * config.age_max_delta;
    direction          = utils_t::rand_u64() % utils_t::direction_max;
    energy_remaining   = {};
  }

  bool microbe_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (!alive) {
      return false;
    }

    if (code.size() != config.code_size) {
      code.reserve(config.code_size);
      while (code.size() < config.code_size) {
        code.push_back(utils_t::rand_u64() % 0xFF);
      }
      code.resize(config.code_size);
    }

    family = utils_t::fasthash64(code.data(), code.size(), 0);

    if (regs.size() != config.regs_size) {
      regs.reserve(config.regs_size);
      while (regs.size() < config.regs_size) {
        regs.push_back(utils_t::rand_u64() % 0xFF);
      }
      regs.resize(config.regs_size);
    }

    resources.resize(config.resources.size());
    for (size_t ind{}; ind < resources.size(); ++ind) {
      utils_t::normalize(resources[ind], 0, config.resources[ind].stack_size);
    }

    if (pos.first >= config.x_max || pos.second >= config.y_max) {
      LOG_GENESIS(ERROR, "invalid microbe_t::pos %zd %zd", pos.first, pos.second);
      return false;
    }

    utils_t::normalize(age, 0L, static_cast<int64_t>(config.age_max + config.age_max_delta));

    direction %= utils_t::direction_max;

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  void world_t::update() {
    TRACE_GENESIS;

    auto time_prev_ms = time_ms;
    time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    stats.time_update = time_ms - time_prev_ms;

    LOG_GENESIS(TIME, "time %zd", time_ms);

    if (update_world_ms < time_ms) {
      LOG_GENESIS(TIME, "update_world_ms %zd   %zd", time_ms, time_ms - update_world_ms);
      update_world_ms = time_ms + config.interval_update_world_ms;
      update_world();
    }

    if (save_world_ms < time_ms) {
      LOG_GENESIS(TIME, "save_world_ms %zd   %zd", time_ms, time_ms - save_world_ms);
      save_world_ms = time_ms + config.interval_save_world_ms;
      save_data();
    }

    LOG_GENESIS(TIME, "time end %zd", time_ms);
  }

  void world_t::update_world() {
    TRACE_GENESIS;

    stats.microbes_count = {};
    stats.microbes_age_avg = {};

    {
      for (size_t ind{}; ind < config.resources.size(); ++ind) {
        const auto& resource_info = config.resources[ind];
        for (const auto& area : resource_info.areas) {
          size_t count = area.frequency * 3.14 * area.radius * area.radius;
          for (size_t i{}; i < count; ++i) {
            size_t x = area.pos.first  + utils_t::rand_u64() % (2 * area.radius) - area.radius;
            size_t y = area.pos.second + utils_t::rand_u64() % (2 * area.radius) - area.radius;
            xy_pos_t pos = {x, y};
            size_t dist = utils_t::distance(pos, area.pos);
            double resource_delta = area.factor * std::max(0.,
                1. - std::pow(std::abs(1. * dist / area.radius), area.sigma));
            if (pos_valid(pos)) {
              size_t xy_ind = xy_pos_to_ind(pos);
              auto& resource = cells[xy_ind].resources[ind];
              resource += resource_delta;
              utils_t::normalize(resource, 0, resource_info.stack_size);
            }
          }
        }
      }
    }

    for (auto& cell : cells) {
      cell.microbe.energy_remaining = config.energy_remaining;
    }

    for (size_t ind{}; ind < cells.size(); ++ind) {
      auto& cell = cells[ind];
      auto& microbe = cell.microbe;

      if (!microbe.alive) {
        continue;
      }

      while (microbe.energy_remaining > 0) {
        microbe.energy_remaining--;
        update_mind(microbe);
        size_t ind_n = xy_pos_to_ind(microbe.pos);
        if (ind_n != ind) {
          auto& microbe_n = cells[ind_n].microbe;
          std::swap(microbe, microbe_n);
          break; // TODO
        }
      }

      if (!microbe.alive) {
        continue;
      }

      if (microbe.age <= 0 || microbe.resources[utils_t::RES_ENERGY] <= 0) {
        for (size_t i{}; i < config.resources.size(); ++i) {
          cell.resources[i] += microbe.resources[i] / 2;
          utils_t::normalize(cell.resources[i], 0, config.resources[i].stack_size);
        }
        microbe = {};
        microbe.alive = false;
        continue;
      }

      update_mind_recipe(config.recipes[config.recipe_step], microbe);

      microbe.age--;
      stats.microbes_count++;
      stats.microbes_age_avg += microbe.age;
    }

    {
      size_t count = stats.microbes_count;
      if (count <= config.spawn_min_count) {
        for (; count < config.spawn_max_count; ++count) {
          microbe_t microbe;
          microbe.init(config);
          microbe.pos = config.spawn_pos;
          microbe.pos.first  += utils_t::rand_u64() % config.spawn_radius - 0.5 * config.spawn_radius;
          microbe.pos.second += utils_t::rand_u64() % config.spawn_radius - 0.5 * config.spawn_radius;
          size_t ind = xy_pos_to_ind(microbe.pos);
          auto& microbe_n = cells[ind].microbe;
          if (microbe.validation(config) && !microbe_n.alive) {
            update_mind_recipe(config.recipes[config.recipe_init], microbe);
            microbe_n = std::move(microbe);
          } else {
            // break;
          }
        }
      }
    }

    // stats
    {
      stats.age++;
      stats.microbes_age_avg /= std::max(1UL, stats.microbes_count);
    }
  }

  void world_t::update_mind(microbe_t& microbe) {
    TRACE_GENESIS;

    LOG_GENESIS(MIND, "family: %zd", microbe.family);

    auto& code = microbe.code;
    auto& regs = microbe.regs;

    uint8_t rip = SAFE_INDEX(regs, utils_t::REG_RIP1B);
    LOG_GENESIS(MIND, "rip: %d", rip);

    uint8_t cmd = SAFE_INDEX(regs, rip++);
    LOG_GENESIS(MIND, "cmd: %d", cmd);

    switch (cmd) {
      case 0: {
        LOG_GENESIS(MIND, "NOP");
        break;

      } case 1: {
        uint8_t reg    = SAFE_INDEX(code, rip++);
        uint8_t offset = SAFE_INDEX(regs, reg);

        LOG_GENESIS(MIND, "BR <%zd>=%zd", reg, offset);

        rip += offset;
        break;

      } case 2: {
        uint8_t reg    = SAFE_INDEX(code, rip++);
        uint8_t offset = SAFE_INDEX(regs, reg);

        LOG_GENESIS(MIND, "BR_ABS <%zd>=%zd", reg, offset);

        rip = offset;
        break;

      } case 3: {
        uint8_t reg = SAFE_INDEX(code, rip++);
        uint8_t val = SAFE_INDEX(code, rip++);

        LOG_GENESIS(MIND, "SET_U8 <%zd> <%zd>", reg, val);

        SAFE_INDEX(regs, reg) = val;
        break;

      } case 4: {
        uint8_t reg = SAFE_INDEX(code, rip++);

        uint16_t val;
        utils_t::load_data(val, code, rip);
        rip += sizeof(uint16_t);

        LOG_GENESIS(MIND, "SET_U16 <%zd> <%zd>", reg, val);

        utils_t::save_data(val, regs, reg);
        break;

      } case 5: {
        uint8_t reg1 = SAFE_INDEX(code, rip++);
        uint8_t reg2 = SAFE_INDEX(code, rip++);
        uint8_t reg3 = SAFE_INDEX(code, rip++);

        LOG_GENESIS(MIND, "ADD_U8 <%zd> <%zd> <%zd>", reg1, reg2, reg3);

        uint8_t arg1 = SAFE_INDEX(regs, reg1);
        uint8_t arg2 = SAFE_INDEX(regs, reg2);
        SAFE_INDEX(regs, reg3) = arg1 + arg2;
        break;

      } case 6: {
        uint8_t reg1 = SAFE_INDEX(code, rip++);
        uint8_t reg2 = SAFE_INDEX(code, rip++);
        uint8_t reg3 = SAFE_INDEX(code, rip++);

        LOG_GENESIS(MIND, "SUB_U8 <%zd> <%zd> <%zd>", reg1, reg2, reg3);

        uint8_t arg1 = SAFE_INDEX(regs, reg1);
        uint8_t arg2 = SAFE_INDEX(regs, reg2);
        SAFE_INDEX(regs, reg3) = arg1 - arg2;
        break;

        // MULT
        // DIV
        // IF

      } case 16: {
        uint8_t reg = SAFE_INDEX(code, rip++);
        uint8_t dir = SAFE_INDEX(regs, reg);

        LOG_GENESIS(MIND, "TURN <%zd>=%zd", reg, dir);

        microbe.direction = (microbe.direction + dir) % utils_t::direction_max;
        break;

      } case 17: {
        /*
        uint8_t args = SAFE_INDEX(bacteria->code, bacteria->rip++);
        uint8_t arg1 = SAFE_INDEX(bacteria->code, utils_t::fasthash64(args++, bacteria->code[0]));
        uint8_t arg2 = SAFE_INDEX(bacteria->code, utils_t::fasthash64(args++, bacteria->code[0]));
        uint8_t arg3 = SAFE_INDEX(bacteria->code, utils_t::fasthash64(args++, bacteria->code[0]));
        bacteria->direction = (bacteria->direction + arg1) % 8;
        LOG_GENESIS(MIND, "LOOK <%d> <%d> <%d> <%d>", args, arg1, arg2, arg3);
        */
        // TODO
        break;

      } case 18: {
        LOG_GENESIS(MIND, "MOVE");

        auto pos = microbe.pos;
        auto pos_n = pos_next(pos, microbe.direction);
        uint64_t ind = xy_pos_to_ind(pos_n);
        if (pos != pos_n && !cells[ind].microbe.alive) {
          microbe.pos = pos_n;
        }

        break;

      } case 19: {
        uint8_t reg = SAFE_INDEX(code, rip++);
        uint8_t dir = SAFE_INDEX(regs, reg);

        LOG_GENESIS(MIND, "CLONE <%zd>=%zd", reg, dir);

        double probability = config.mutation_probability * 0xFFFF / (code.size() + regs.size());
        auto pos_n         = pos_next(microbe.pos, dir);
        auto ind           = xy_pos_to_ind(pos_n);
        auto& microbe_n    = cells[ind].microbe;

        if (!microbe_n.alive && update_mind_recipe(config.recipes[config.recipe_clone], microbe)) {
          microbe_t microbe_child = {};
          microbe_child.init(config);

          microbe_child.code   = microbe.code;
          microbe_child.regs   = microbe.regs;
          microbe_child.pos    = pos_n;
          for (auto& byte : microbe_child.code) {
            if (utils_t::rand_u64() % 0xFFFF < probability) {
              byte = utils_t::rand_u64();
            }
          }
          for (auto& byte : microbe_child.regs) {
            if (utils_t::rand_u64() % 0xFFFF < probability) {
              byte = utils_t::rand_u64();
            }
          }
          if (microbe_child.validation(config)) {
            update_mind_recipe(config.recipes[config.recipe_init], microbe_child);
            microbe_n = std::move(microbe_child);
          }
        }
        break;

      } case 20: {
        uint8_t reg = SAFE_INDEX(code, rip++);
        uint8_t ind = SAFE_INDEX(regs, reg);

        LOG_GENESIS(MIND, "RECIPE <%zd>=%zd", reg, ind);

        const auto& recipe = config.recipes[ind % config.recipes.size()];
        if (recipe.available) {
          update_mind_recipe(recipe, microbe);
        }
        break;

      } case 21: {
        uint8_t reg1 = SAFE_INDEX(code, rip++);
        uint8_t reg2 = SAFE_INDEX(code, rip++);

        uint8_t  dir = SAFE_INDEX(regs, reg1);
        uint16_t strength;
        utils_t::load_data(strength, regs, reg2);

        LOG_GENESIS(MIND, "ATTACK <%d>=%zd <%d>=%zd", reg1, dir, reg2, strength);

        auto stack_size = config.resources[utils_t::RES_ENERGY].stack_size;
        strength %= stack_size;

        auto pos_n = pos_next(microbe.pos, dir);
        uint64_t ind = xy_pos_to_ind(pos_n);

        auto& microbe_attacked = cells[ind].microbe;
        auto& energy           = microbe.resources[utils_t::RES_ENERGY];
        auto& energy_attacked  = microbe_attacked.resources[utils_t::RES_ENERGY];

        if (microbe.pos != pos_n
            && energy > strength
            && microbe_attacked.alive)
        {
          energy -= strength;
          energy_attacked -= strength;

          utils_t::normalize(energy, 0, stack_size);
          utils_t::normalize(energy_attacked, 0, stack_size);
        }
        break;

      } case 22: {
        uint8_t reg1 = SAFE_INDEX(code, rip++);
        uint8_t reg2 = SAFE_INDEX(code, rip++);
        uint8_t reg3 = SAFE_INDEX(code, rip++);

        uint8_t dir = SAFE_INDEX(regs, reg1);
        uint8_t res = SAFE_INDEX(regs, reg2);
        res_val_t val;
        utils_t::load_data(val, regs, reg3);

        LOG_GENESIS(MIND, "RESOURCE EXCHANGE <%zd>=%zd <%zd>=%zd <%zd>=%zd",
            reg1, dir, reg2, res, reg3, val);

        size_t resource         = res % config.resources.size();
        auto   pos_n            = pos_next(microbe.pos, dir);
        size_t ind              = xy_pos_to_ind(pos_n);
        auto   stack_size       = config.resources[resource].stack_size;
        auto&  microbe_resource = microbe.resources[resource];
        auto&  cell_resource    = cells[ind].resources[resource];

        if (microbe_resource + val >= 0
            && microbe_resource + val <= stack_size
            && cell_resource - val >= 0
            && cell_resource - val <= stack_size)
        {
          microbe_resource += val;
          cell_resource -= val;
        }

        break;

      } default: {
        LOG_GENESIS(MIND, "NOTHING");
        break;
      }
    }

    SAFE_INDEX(regs, utils_t::REG_RIP1B) = rip;
  }

  bool world_t::update_mind_recipe(const recipe_t& recipe, microbe_t& microbe) {
    auto& resources = microbe.resources;

    for (const auto& [ind, count] : recipe.in_out) {
      const auto& resource_info = config.resources[ind];
      auto count_n = resources[ind] + count;
      if (count_n < 0 || count_n > resource_info.stack_size) {
        return false;
      }
    }

    for (const auto& [ind, count] : recipe.in_out) {
      const auto& resource_info = config.resources[ind];
      auto count_n = count;

      auto& resource = resources[ind];
      resource += count_n;
      utils_t::normalize(resource, 0, resource_info.stack_size);
      LOG_GENESIS(MIND, "resource %zd", resource);
    }

    return true;
  }

  void world_t::init() {
    TRACE_GENESIS;

    load_config();
    load_data();

    utils_t::seed = config.seed ? config.seed : time(0);
    utils_t::debug = config.debug;

    save_config();
    // save_data();
  }

  void world_t::load_config() {
    TRACE_GENESIS;

    if (!config_json_wrapper_t(config).load(config_file_name)) {
      LOG_GENESIS(ERROR, "can not load config");
      throw std::runtime_error("can not load config");
    }
  }

  void world_t::save_config() {
    TRACE_GENESIS;

#ifndef VALGRIND
    if (!config_json_wrapper_t(config).save(config_file_name)) {
      LOG_GENESIS(ERROR, "can not save config");
      throw std::runtime_error("can not save config");
    }
#endif
  }

  void world_t::load_data() {
    TRACE_GENESIS;

    if (!world_json_wrapper_t(*this).load(world_file_name)) {
      LOG_GENESIS(ERROR, "can not load world");
      throw std::runtime_error("can not load world");
    }
  }

  void world_t::save_data() {
    TRACE_GENESIS;

#ifndef VALGRIND
    if (!world_json_wrapper_t(*this).save(world_file_name)) {
      LOG_GENESIS(ERROR, "can not save world");
      throw std::runtime_error("can not save world");
    }
#endif
  }

  ////////////////////////////////////////////////////////////////////////////////
}

