
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



#define PRODUCTION 1
#define VALGRIND 0

#define JSON_SAVE(json, name)   json[#name] = name
#define JSON_LOAD(json, name)   name = json.value(#name, name)

#define JSON_SAVE2(json, s, name)   json[#name] = s.name
#define JSON_LOAD2(json, s, name)   s.name = json.value(#name, s.name)

#define SAFE_INDEX(cont, ind)   cont[(ind) % cont.size()]

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
  struct config_t;
  struct world_t;

  struct utils_t {
    struct hash_data_t {
      uint16_t rip;
      uint8_t  hash0;
      uint8_t  hash1;
      uint8_t  cmd;
      uint8_t  arg;
      uint16_t reserved;
    } __attribute__((packed));

    using xy_pos_t = std::pair<uint64_t, uint64_t>;

    struct xy_pos_hash_t {
      uint64_t operator()(const xy_pos_t& pos) const {
        auto hash1 = std::hash<uint64_t>{}(pos.first);
        auto hash2 = std::hash<uint64_t>{}(pos.second);
        return (hash1 << 32) + (hash1 >> 32) + hash2;
      }
    };

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
    inline static uint64_t npos                = std::string::npos;
    inline static uint64_t direction_max       = 9;
    inline static uint64_t CODE_RIP2B          = 0;
    inline static uint64_t CODE_SEED           = 2;
    inline static uint64_t CODE_SIZE_MIN       = 10;
    inline static uint64_t RES_ENERGY          = 0;

    inline static size_t seed = {};

    inline static std::set<std::string> debug =
#if PRODUCTION
        { ERROR };
#else
        { ERROR, DEBUG, TRACE, ARGS };
#endif

    inline static std::vector<std::string> directions = {
        DIR_R, DIR_U, DIR_U, DIR_LU, DIR_L, DIR_LD, DIR_D, DIR_RD };

    static void rename(const std::string& name_old, const std::string& name_new);
    static void remove(const std::string& name);
    static bool load(nlohmann::json& json, const std::string& name);
    static bool save(const nlohmann::json& json, const std::string& name);
    static uint64_t rand_u64();
    static inline uint64_t hash_mix(uint64_t h);
    static inline uint64_t fasthash64(const void *buf, size_t len, uint64_t seed);

    static uint64_t distance(const xy_pos_t& pos1, const xy_pos_t& pos2) {
      auto [x1, y1] = pos1;
      auto [x2, y2] = pos2;
      uint64_t dx = (x2 >= x1) ? (x2 - x1) : (x1 - x2);
      uint64_t dy = (y2 >= y1) ? (y2 - y1) : (y1 - y2);
      return std::hypot(dx, dy);
    }

    static inline void load_by_hash(auto& value, const std::vector<uint8_t>& data, uint64_t index) {
      TRACE_GENESIS;
      auto dst = reinterpret_cast<void*>(&value);
      auto src = reinterpret_cast<const void*>(data.data() + (index % (data.size() - sizeof(value))));
      std::memcpy(dst, src, sizeof(value));
    }

    static inline void save_by_hash(auto value, std::vector<uint8_t>& data, uint64_t index) {
      TRACE_GENESIS;
      auto dst = reinterpret_cast<void*>(data.data() + (index % (data.size() - sizeof(value))));
      auto src = reinterpret_cast<const void*>(&value);
      std::memcpy(dst, src, sizeof(value));
    }
  };

  ////////////////////////////////////////////////////////////////////////////////

  struct area_json_t {
    using xy_pos_t = utils_t::xy_pos_t;

    // Количество добытого ресурса вычистяется по формуле
    //    y = factor * max(0, 1 - |t / radius| ^ sigma)
    //    t = ((x - x1) ^ 2 + (y - y1) ^ 2) ^ 0.5 - расстояние до центра источника
    //    y *= out.count

    xy_pos_t      pos               = {};
    uint64_t      radius            = 100;
    double        factor            = 1;
    double        sigma             = 2;
  };

  struct resource_info_json_t {
    std::string   name;
    int64_t       stack_size    = utils_t::npos;
    bool          extractable   = false;
  };

  struct recipe_json_t {
    using in_t    = std::map<size_t/*ind*/, int64_t/*count*/>;
    using out_t   = std::map<size_t/*ind*/, int64_t/*count*/>;

    std::string   name              = {};
    in_t          in                = {};
    out_t         out               = {};
  };

  struct config_json_t {
    using debug_t       = std::set<std::string>;
    using resources_t   = std::vector<resource_info_json_t>;
    using areas_t       = std::vector<std::vector<area_json_t>>;
    using recipes_t     = std::vector<recipe_json_t>;
    using xy_pos_t      = utils_t::xy_pos_t;

    uint64_t      x_max                       = 1000;
    uint64_t      y_max                       = 1000;
    uint64_t      code_size                   = 32;
    uint64_t      age_max                     = 10000;
    uint64_t      energy_remaining            = 10;
    uint64_t      interval_update_world_ms    = 100;
    uint64_t      interval_save_world_ms      = 60 * 1000;
    uint64_t      seed                        = {};
    debug_t       debug                       = { utils_t::ERROR };
    resources_t   resources                   = {};
    areas_t       areas                       = {};
    recipes_t     recipes                     = {};
    xy_pos_t      spawn_pos                   = {};
    uint64_t      spawn_radius                = {};
    uint64_t      spawn_min_count             = {};
    uint64_t      spawn_max_count             = {};
  };

  struct stats_json_t {
    uint64_t   age              = {};
    uint64_t   microbes_count   = {};
    double     microbes_age_avg = {};
    uint64_t   time_update      = {};
  };

  ////////////////////////////////////////////////////////////////////////////////

  struct microbe_t {
    using resources_t   = std::map<size_t/*ind*/, int64_t/*count*/>;
    using code_t        = std::vector<uint8_t>;
    using xy_pos_t      = utils_t::xy_pos_t;

    uint64_t      family;
    code_t        code;
    resources_t   resources;
    xy_pos_t      pos;
    uint64_t      age;
    uint64_t      direction;
    int64_t       energy_remaining;
    bool          alive                = false;

    bool validation(const config_t& config);
    void init(const config_t& config);
  };

  struct config_t {
    using debug_t       = std::set<std::string>;
    using resources_t   = std::vector<resource_info_json_t>;
    using areas_t       = std::vector<std::vector<area_json_t>>;
    using recipes_t     = std::vector<recipe_json_t>;
    using xy_pos_t      = utils_t::xy_pos_t;

    uint64_t      x_max;
    uint64_t      y_max;
    uint64_t      code_size;
    uint64_t      health_max;
    uint64_t      age_max;
    uint64_t      energy_remaining;
    uint64_t      interval_update_world_ms;
    uint64_t      interval_save_world_ms;
    uint64_t      seed;
    debug_t       debug;
    resources_t   resources;
    areas_t       areas;
    recipes_t     recipes;
    xy_pos_t      spawn_pos;
    uint64_t      spawn_radius;
    uint64_t      spawn_min_count;
    uint64_t      spawn_max_count;

    bool from_json(const config_json_t& config_json);
    bool to_json(config_json_t& config_json) const;
  };

  struct world_t {
    using microbes_t = std::vector<microbe_t>;

    std::string   config_file_name   = {};
    std::string   world_file_name    = {};

    config_t      config             = {};
    microbes_t    microbes           = {};
    stats_json_t  stats              = {};

    uint64_t      time_ms            = {};
    uint64_t      update_world_ms    = {};
    uint64_t      save_world_ms      = {};

    void update();
    void update_ctx();
    void update_mind(microbe_t& microbe);
    void init();
    void load_config();
    void load_data();
    void save_config();
    void save_data();

    uint64_t xy_pos_to_ind(const utils_t::xy_pos_t& pos) {
      TRACE_GENESIS;
      return pos.first + config.x_max * pos.second;
    }

    utils_t::xy_pos_t xy_pos_from_ind(uint64_t ind) {
      TRACE_GENESIS;
      return {ind % config.x_max, ind / config.x_max};
    }

    bool pos_valid(const utils_t::xy_pos_t& pos) const {
      TRACE_GENESIS;
      return pos.first < config.x_max && pos.second < config.y_max;
    }

    utils_t::xy_pos_t pos_next(const utils_t::xy_pos_t& pos, uint64_t direction) const {
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

      utils_t::xy_pos_t pos_next = {pos.first + dx, pos.second + dy};

      if (!pos_valid(pos_next)) {
        return pos;
      }

      return pos_next;
    }
  };

  ////////////////////////////////////////////////////////////////////////////////

  inline void to_json(nlohmann::json& json, const area_json_t& area_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, area_json, pos);
    JSON_SAVE2(json, area_json, radius);
    JSON_SAVE2(json, area_json, factor);
    JSON_SAVE2(json, area_json, sigma);
  }

  inline void from_json(const nlohmann::json& json, area_json_t& area_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, area_json, pos);
    JSON_LOAD2(json, area_json, radius);
    JSON_LOAD2(json, area_json, factor);
    JSON_LOAD2(json, area_json, sigma);
  }

  inline void to_json(nlohmann::json& json, const resource_info_json_t& resource_info_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource_info_json, name);
    JSON_SAVE2(json, resource_info_json, stack_size);
    JSON_SAVE2(json, resource_info_json, extractable);
  }

  inline void from_json(const nlohmann::json& json, resource_info_json_t& resource_info_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource_info_json, name);
    JSON_LOAD2(json, resource_info_json, stack_size);
    JSON_LOAD2(json, resource_info_json, extractable);
  }

  inline void to_json(nlohmann::json& json, const recipe_json_t& recipe_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, recipe_json, name);
    JSON_SAVE2(json, recipe_json, in);
    JSON_SAVE2(json, recipe_json, out);
  }

  inline void from_json(const nlohmann::json& json, recipe_json_t& recipe_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, recipe_json, name);
    JSON_LOAD2(json, recipe_json, in);
    JSON_LOAD2(json, recipe_json, out);
  }

  inline void to_json(nlohmann::json& json, const config_json_t& config_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, config_json, x_max);
    JSON_SAVE2(json, config_json, y_max);
    JSON_SAVE2(json, config_json, code_size);
    JSON_SAVE2(json, config_json, age_max);
    JSON_SAVE2(json, config_json, energy_remaining);
    JSON_SAVE2(json, config_json, interval_update_world_ms);
    JSON_SAVE2(json, config_json, interval_save_world_ms);
    JSON_SAVE2(json, config_json, seed);
    JSON_SAVE2(json, config_json, debug);
    JSON_SAVE2(json, config_json, resources);
    JSON_SAVE2(json, config_json, areas);
    JSON_SAVE2(json, config_json, recipes);
    JSON_SAVE2(json, config_json, spawn_pos);
    JSON_SAVE2(json, config_json, spawn_radius);
    JSON_SAVE2(json, config_json, spawn_min_count);
    JSON_SAVE2(json, config_json, spawn_max_count);
  }

  inline void from_json(const nlohmann::json& json, config_json_t& config_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, config_json, x_max);
    JSON_LOAD2(json, config_json, y_max);
    JSON_LOAD2(json, config_json, code_size);
    JSON_LOAD2(json, config_json, age_max);
    JSON_LOAD2(json, config_json, energy_remaining);
    JSON_LOAD2(json, config_json, interval_update_world_ms);
    JSON_LOAD2(json, config_json, interval_save_world_ms);
    JSON_LOAD2(json, config_json, seed);
    JSON_LOAD2(json, config_json, debug);
    JSON_LOAD2(json, config_json, resources);
    JSON_LOAD2(json, config_json, areas);
    JSON_LOAD2(json, config_json, recipes);
    JSON_LOAD2(json, config_json, spawn_pos);
    JSON_LOAD2(json, config_json, spawn_radius);
    JSON_LOAD2(json, config_json, spawn_min_count);
    JSON_LOAD2(json, config_json, spawn_max_count);
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

  bool utils_t::save(const nlohmann::json& json, const std::string& name) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "name: %s", name.c_str());

    std::string name_tmp = name + TMP_SUFFIX;
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

  ////////////////////////////////////////////////////////////////////////////////

  inline void to_json(nlohmann::json& json, const microbe_t& microbe) {
    TRACE_GENESIS;
    JSON_SAVE2(json, microbe, family);
    JSON_SAVE2(json, microbe, code);
    JSON_SAVE2(json, microbe, resources);
    JSON_SAVE2(json, microbe, pos);
    JSON_SAVE2(json, microbe, age);
    JSON_SAVE2(json, microbe, direction);
    JSON_SAVE2(json, microbe, energy_remaining);
    JSON_SAVE2(json, microbe, alive);
  }

  inline void from_json(const nlohmann::json& json, microbe_t& microbe) {
    TRACE_GENESIS;
    JSON_LOAD2(json, microbe, family);
    JSON_LOAD2(json, microbe, code);
    JSON_LOAD2(json, microbe, resources);
    JSON_LOAD2(json, microbe, pos);
    JSON_LOAD2(json, microbe, age);
    JSON_LOAD2(json, microbe, direction);
    JSON_LOAD2(json, microbe, energy_remaining);
    JSON_LOAD2(json, microbe, alive);
  }

  bool microbe_t::validation(const config_t& config) {
    TRACE_GENESIS;

    // family

    while (code.size() < config.code_size) {
      code.push_back(utils_t::rand_u64() % 0xFF);
    }
    code.resize(config.code_size);

    for (auto& [ind, count] : resources) {
      if (ind >= config.resources.size()) {
        LOG_GENESIS(ERROR, "invalid microbe_t::resources %ld", ind);
        return false;
      }
      count = std::min(count, config.resources[ind].stack_size);
    }

    if (pos.first >= config.x_max || pos.second >= config.y_max) {
      LOG_GENESIS(ERROR, "invalid microbe_t::pos %zd %zd", pos.first, pos.second);
      return false;
    }

    // age

    direction %= utils_t::direction_max;

    // alive

    return true;
  }

  void microbe_t::init(const config_t& config) {
    TRACE_GENESIS;

    family             = utils_t::rand_u64();
    // code
    resources          = {};
    pos                = {utils_t::rand_u64() % config.x_max, utils_t::rand_u64() % config.y_max};
    age                = {};
    direction          = utils_t::rand_u64() % utils_t::direction_max;
    energy_remaining   = {};
    alive              = true;

    if (resources[utils_t::RES_ENERGY] <= 0) {
      resources[utils_t::RES_ENERGY] = config.resources[utils_t::RES_ENERGY].stack_size / 2;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool config_t::from_json(const config_json_t& config_json) {
    TRACE_GENESIS;

    x_max = config_json.x_max;
    if (x_max < 5 || x_max > 100000) {
      LOG_GENESIS(ERROR, "invalid config_t::x_max %zd", x_max);
      return false;
    }

    y_max = config_json.y_max;
    if (y_max < 5 || y_max > 100000) {
      LOG_GENESIS(ERROR, "invalid config_t::x_max %zd", y_max);
      return false;
    }

    code_size = config_json.code_size;
    if (code_size < utils_t::CODE_SIZE_MIN || code_size > 1000) {
      LOG_GENESIS(ERROR, "invalid config_t::code_size %zd", code_size);
      return false;
    }

    age_max = config_json.age_max;

    energy_remaining = config_json.energy_remaining;

    interval_update_world_ms = config_json.interval_update_world_ms;

    interval_save_world_ms = config_json.interval_save_world_ms;

    seed = config_json.seed;

    debug = config_json.debug;
    if (debug.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::debug %zd", debug.size());
      return false;
    }

    resources = config_json.resources;
    if (resources.empty() || resources.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::resources %zd", resources.size());
      return false;
    }

    if (resources[utils_t::RES_ENERGY].name != utils_t::ENERGY) {
      LOG_GENESIS(ERROR, "invalid config_t::resources %s", utils_t::ENERGY.c_str());
      return false;
    }

    areas = config_json.areas;
    areas.resize(resources.size());

    recipes = config_json.recipes;
    if (recipes.size() < 1 || recipes.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::recipes %zd", recipes.size());
      return false;
    }

    for (auto& recipe : recipes) {
      for (auto& [ind, _] : recipe.in) {
        if (ind >= config_json.resources.size()) {
          LOG_GENESIS(ERROR, "invalid config_t::resources %ld", ind);
          return false;
        }
      }
      for (auto& [ind, _] : recipe.out) {
        if (ind >= config_json.resources.size()) {
          LOG_GENESIS(ERROR, "invalid config_t::resources %ld", ind);
          return false;
        }
      }
      if (recipe.in.empty() && recipe.out.empty()) {
        LOG_GENESIS(ERROR, "invalid config_t::recipe");
        return false;
      }
    }

    spawn_pos = config_json.spawn_pos;
    if (spawn_pos.first >= x_max || spawn_pos.second >= y_max) {
      LOG_GENESIS(ERROR, "invalid config_t::spawn_pos");
      return false;
    }

    spawn_radius = config_json.spawn_radius;
    if (spawn_radius < 5) {
      LOG_GENESIS(ERROR, "invalid config_t::spawn_radius");
      return false;
    }

    spawn_min_count = config_json.spawn_min_count;

    spawn_max_count = config_json.spawn_max_count;
    if (spawn_max_count <= spawn_min_count) {
      LOG_GENESIS(ERROR, "invalid config_t::spawn_max_count");
      return false;
    }

    return true;
  }

  bool config_t::to_json(config_json_t& config_json) const {
    TRACE_GENESIS;

    config_json.x_max                      = x_max;
    config_json.y_max                      = y_max;
    config_json.code_size                  = code_size;
    config_json.age_max                    = age_max;
    config_json.energy_remaining           = energy_remaining;
    config_json.interval_update_world_ms   = interval_update_world_ms;
    config_json.interval_save_world_ms     = interval_save_world_ms;
    config_json.seed                       = seed;
    config_json.debug                      = debug;
    config_json.resources                  = resources;
    config_json.areas                      = areas;
    config_json.recipes                    = recipes;
    config_json.spawn_pos                  = spawn_pos;
    config_json.spawn_radius               = spawn_radius;
    config_json.spawn_min_count            = spawn_min_count;
    config_json.spawn_max_count            = spawn_max_count;

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
      update_ctx();
    }

    if (save_world_ms < time_ms) {
      LOG_GENESIS(TIME, "save_world_ms %zd   %zd", time_ms, time_ms - save_world_ms);
      save_world_ms = time_ms + config.interval_save_world_ms;
      save_data();
    }

    LOG_GENESIS(TIME, "time end %zd", time_ms);
  }

  void world_t::update_ctx() {
    TRACE_GENESIS;

    stats.microbes_count = {};
    stats.microbes_age_avg = {};

    for (auto& microbe : microbes) {
      microbe.energy_remaining = config.energy_remaining;
    }

    for (size_t ind{}; ind < microbes.size(); ind++) {
      auto& microbe = microbes[ind];

      if (!microbe.alive) {
        continue;
      }

      while (microbe.energy_remaining > 0) {
        microbe.energy_remaining--;
        update_mind(microbe);
        uint64_t ind_n = xy_pos_to_ind(microbe.pos);
        if (ind_n != ind) {
          auto& microbe_n = microbes[ind_n];
          std::swap(microbe, microbe_n);
          break; // TODO
        }
      }

      if (microbe.age >= config.age_max
          || microbe.resources[utils_t::RES_ENERGY] <= 0)
      {
        microbe = {};
        microbe.alive = false;
        continue;
      }

      microbe.age++;
      microbe.resources[utils_t::RES_ENERGY]--;

      stats.microbes_count++;
      stats.microbes_age_avg += microbe.age;
    }

    {
      size_t count = stats.microbes_count;
      if (count <= config.spawn_min_count) {
        while (count < config.spawn_max_count) {
          microbe_t microbe;
          microbe.init(config);
          microbe.pos = config.spawn_pos;
          microbe.pos.first  += 2 * utils_t::rand_u64() % config.spawn_radius - config.spawn_radius;
          microbe.pos.second += 2 * utils_t::rand_u64() % config.spawn_radius - config.spawn_radius;
          uint64_t ind = xy_pos_to_ind(microbe.pos);
          if (microbe.validation(config) && !microbes[ind].alive) {
            microbes[ind] = std::move(microbe);
          } else {
            break;
          }
        }
      }
    }

    // stats
    {
      stats.age++;
      stats.microbes_age_avg /= stats.microbes_count;
    }
  }

  void world_t::update_mind(microbe_t& microbe) {
    TRACE_GENESIS;

    LOG_GENESIS(MIND, "family: %zd", microbe.family);

    auto& code = microbe.code;

    uint8_t seed;
    utils_t::load_by_hash(seed, code, utils_t::CODE_SEED);

    uint16_t rip;
    utils_t::load_by_hash(rip, code, utils_t::CODE_RIP2B);
    LOG_GENESIS(MIND, "rip: %d", rip);

    uint8_t cmd;
    utils_t::load_by_hash(cmd, code, rip++);
    LOG_GENESIS(MIND, "cmd: %d", cmd);

    uint8_t args;
    utils_t::load_by_hash(args, code, rip++);
    LOG_GENESIS(MIND, "args: %d", args);

    switch (cmd) {
      case 0: {
        LOG_GENESIS(MIND, "NOP");
        break;

      } case 1: {
        uint16_t opnd_offset;
        utils_t::load_by_hash(opnd_offset, code, utils_t::hash_mix((seed ^ args) + 0));

        LOG_GENESIS(MIND, "BR <%d>", opnd_offset);

        rip += opnd_offset;
        break;

      } case 2: {
        uint16_t opnd_rip;
        utils_t::load_by_hash(opnd_rip, code, utils_t::hash_mix((seed ^ args) + 0));

        LOG_GENESIS(MIND, "BR_ABS <%d>", opnd_rip);

        rip = opnd_rip;
        break;

      } case 3: {
        uint16_t opnd_ind;
        utils_t::load_by_hash(opnd_ind, code, utils_t::hash_mix((seed ^ args) + 0));

        uint8_t opnd_val;
        utils_t::load_by_hash(opnd_val, code, utils_t::hash_mix((seed ^ args) + 1));

        LOG_GENESIS(MIND, "SET_U8 <%d> <%d>", opnd_ind, opnd_val);

        utils_t::save_by_hash(opnd_val, code, opnd_ind);
        break;

      } case 4: {
        uint16_t opnd_ind;
        utils_t::load_by_hash(opnd_ind, code, utils_t::hash_mix((seed ^ args) + 0));

        uint16_t opnd_val;
        utils_t::load_by_hash(opnd_val, code, utils_t::hash_mix((seed ^ args) + 1));

        LOG_GENESIS(MIND, "SET_U16 <%d> <%d>", opnd_ind, opnd_val);

        utils_t::save_by_hash(opnd_val, code, opnd_ind);
        break;

      } case 5: {
        uint16_t opnd_ind;
        utils_t::load_by_hash(opnd_ind, code, utils_t::hash_mix((seed ^ args) + 0));

        uint8_t opnd2;
        utils_t::load_by_hash(opnd2, code, utils_t::hash_mix((seed ^ args) + 1));

        uint8_t opnd3;
        utils_t::load_by_hash(opnd3, code, utils_t::hash_mix((seed ^ args) + 2));

        LOG_GENESIS(MIND, "ADD_U8 <%d> <%d> <%d>", opnd_ind, opnd2, opnd3);

        uint8_t res = opnd2 + opnd3;
        utils_t::save_by_hash(res, code, opnd_ind);
        break;

      } case 6: {
        uint16_t opnd_ind;
        utils_t::load_by_hash(opnd_ind, code, utils_t::hash_mix((seed ^ args) + 0));

        uint8_t opnd2;
        utils_t::load_by_hash(opnd2, code, utils_t::hash_mix((seed ^ args) + 1));

        uint8_t opnd3;
        utils_t::load_by_hash(opnd3, code, utils_t::hash_mix((seed ^ args) + 2));

        LOG_GENESIS(MIND, "SUB_U8 <%d> <%d> <%d>", opnd_ind, opnd2, opnd3);

        uint8_t res = opnd2 - opnd3;
        utils_t::save_by_hash(res, code, opnd_ind);
        break;

        // MULT
        // DIV
        // IF

      } case 16: {
        uint8_t opnd_dir;
        utils_t::load_by_hash(opnd_dir, code, utils_t::hash_mix((seed ^ args) + 0));

        LOG_GENESIS(MIND, "TURN <%zd>", opnd_dir);

        microbe.direction = (microbe.direction + opnd_dir) % utils_t::direction_max;
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
        if (pos != pos_n && !microbes[ind].alive) {
          microbe.pos = pos_n;
        }

        break;

      } case 19: {
        uint8_t opnd_dir;
        utils_t::load_by_hash(opnd_dir, code, utils_t::hash_mix((seed ^ args) + 0));

        uint16_t opnd_probability;
        utils_t::load_by_hash(opnd_probability, code, utils_t::hash_mix((seed ^ args) + 1));

        LOG_GENESIS(MIND, "CLONE <%d> <%d>", opnd_dir, opnd_probability);

        uint64_t direction = opnd_dir % utils_t::direction_max;
        double probability = 0.1 * opnd_probability / 0xFFFF / code.size();

        LOG_GENESIS(MIND, "energy %zd", microbe.resources[utils_t::RES_ENERGY]);
        auto pos_n = pos_next(microbe.pos, direction);
        uint64_t ind = xy_pos_to_ind(pos_n);
        if (microbe.resources[utils_t::RES_ENERGY]
            > 0.6 * config.resources[utils_t::RES_ENERGY].stack_size
            && !microbes[ind].alive)
        {
          LOG_GENESIS(MIND, "energy ok");
          for (auto& [_, count] : microbe.resources) {
            count /= 2;
          }
          auto microbe_child = microbe;
          microbe_child.init(config);
          microbe_child.pos = pos_n;
          for (auto& byte : microbe_child.code) {
            if (utils_t::rand_u64() % 0xFFFF < probability) {
              byte = utils_t::rand_u64();
            }
          }
          if (microbe_child.validation(config)) {
            microbes[ind] = std::move(microbe_child);
          }
        }
        break;

      } case 20: {
        uint16_t opnd_recipe;
        utils_t::load_by_hash(opnd_recipe, code, utils_t::hash_mix((seed ^ args) + 0));

        LOG_GENESIS(MIND, "RECIPE <%zd>", opnd_recipe);

        [this, &microbe, opnd_recipe]() {
          const auto& recipe = config.recipes[opnd_recipe % config.recipes.size()];
          LOG_GENESIS(MIND, "name %s", recipe.name.c_str());

          auto& resources = microbe.resources;
          for (const auto& [ind, count] : recipe.in) {
            if (resources[ind] < count) {
              return;
            }
          }

          for (const auto& [ind, count] : recipe.in) {
            resources[ind] -= count;
          }

          for (const auto& [ind, count] : recipe.out) {
            const auto& resource_info = config.resources[ind];
            int64_t count_n = count;

            if (resource_info.extractable) {
              double multiplier = {};
              LOG_GENESIS(MIND, "ind %zd", ind);
              const auto& areas = config.areas[ind];

              for (const auto& area : areas) {
                uint64_t dist = utils_t::distance(microbe.pos, area.pos);
                multiplier += area.factor * std::max(0.,
                    1. - std::pow(std::abs((double) dist / area.radius), area.sigma));
                LOG_GENESIS(MIND, "multiplier %f", multiplier);
              }
              count_n *= multiplier;
              LOG_GENESIS(MIND, "count_n %zd", count_n);
            }

            resources[ind] = std::min(resource_info.stack_size, resources[ind] + count_n);

            LOG_GENESIS(MIND, "resource %zd", resources[ind]);
          }
        }();

        break;

      } default: {
        LOG_GENESIS(MIND, "NOTHING");
        break;
      }
    }

    utils_t::save_by_hash(rip, code, utils_t::CODE_RIP2B);
  }

  void world_t::init() {
    TRACE_GENESIS;
    load_config();
    load_data();
    utils_t::seed = config.seed ? config.seed : time(0);
    utils_t::debug = config.debug;
    save_config();
    save_data();
  }

  void world_t::load_config() {
    TRACE_GENESIS;

    nlohmann::json json;
    if (!utils_t::load(json, config_file_name)) {
      LOG_GENESIS(ERROR, "can not load file %s", config_file_name.c_str());
      throw std::runtime_error("invalid config json");
    }

    config_json_t config_json = json;

    if (!config.from_json(config_json)) {
      LOG_GENESIS(ERROR, "invalid config");
      throw std::runtime_error("invalid config");
    }
  }

  void world_t::load_data() {
    TRACE_GENESIS;

    nlohmann::json json;
    if (!utils_t::load(json, world_file_name)) {
      LOG_GENESIS(ERROR, "can not load file %s", world_file_name.c_str());
      microbes = {};
    } else {
      microbes = (world_t::microbes_t) json;
    }

    microbes.resize(config.x_max * config.y_max);
    for (auto& microbe : microbes) {
      if (!microbe.validation(config)) {
        microbe = {};
      }
    }
  }

  void world_t::save_config() {
    TRACE_GENESIS;

#if !VALGRIND
    config_json_t config_json;
    if (!config.to_json(config_json)) {
      LOG_GENESIS(ERROR, "invalid config");
      throw std::runtime_error("invalid config");
    }

    nlohmann::json json = config_json;
    utils_t::save(json, config_file_name);
#endif
  }

  void world_t::save_data() {
    TRACE_GENESIS;

#if !VALGRIND
    nlohmann::json json = microbes;
    utils_t::save(json, world_file_name);
#endif
  }

  ////////////////////////////////////////////////////////////////////////////////
}

