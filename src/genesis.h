
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
#include <deque>
#include <regex>
#include <list>
#include <set>

#include "../3rd_party/nlohmann/json.hpp"
#include "debug_logger.h"



#define PRODUCTION 0

#define JSON_SAVE(json, name)   json[#name] = name
#define JSON_LOAD(json, name)   name = json.value(#name, name)

#define JSON_SAVE2(json, s, name)   json[#name] = s.name
#define JSON_LOAD2(json, s, name)   s.name = json.value(#name, s.name)

#define SAFE_INDEX(cont, ind)   cont.at((ind) % cont.size())

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
    inline static uint64_t CODE_SEED           = 0;
    inline static uint64_t CODE_HASH0          = 1;
    inline static uint64_t CODE_HASH1          = 2;
    inline static uint64_t CODE_SIZE_MIN       = 10;

    inline static size_t seed = time(0);

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
    static uint64_t hash_mix(uint64_t h);
    static uint64_t fasthash64(const void *buf, size_t len, uint64_t seed);

    static uint64_t distance(const xy_pos_t& pos1, const xy_pos_t& pos2) {
      auto [x1, y1] = pos1;
      auto [x2, y2] = pos2;
      uint64_t dx = (x2 >= x1) ? (x2 - x1) : (x1 - x2);
      uint64_t dy = (y2 >= y1) ? (y2 - y1) : (y1 - y2);
      return std::hypot(dx, dy);
    }

    static uint64_t safe_index(const std::vector<uint8_t>& data, uint64_t index) {
      return data.at(index % data.size());
    }

    template <typename type_t>
    static void load_by_hash(const std::vector<uint8_t>& data, uint64_t index, uint64_t& value) {
      TRACE_GENESIS;
      value = {};
      for (size_t i{}; i < sizeof(type_t); ++i) {
        value += data.at(hash_mix(index + i) % data.size()) << 8 * i;
      }
    }

    template <typename type_t>
    static void save_by_hash(std::vector<uint8_t>& data, uint64_t index, uint64_t value) {
      TRACE_GENESIS;
      for (size_t i{}; i < sizeof(type_t); ++i) {
        data.at(hash_mix(index + i) % data.size()) = (value >> 8 * i) & 0xFF;
      }
    }
  };

  ////////////////////////////////////////////////////////////////////////////////

  struct area_json_t {
    using xy_pos_t = utils_t::xy_pos_t;

    // Количество добытого ресурса вычистяется по формуле
    //    y = factor * max(0, 1 - |t / radius| ^ sigma)
    //    t = ((x - x1) ^ 2 + (y - y1) ^ 2) ^ 0.5 - расстояние до центра источника
    //    y *= out.count

    std::string   name              = {};
    std::string   resource          = {};
    xy_pos_t      pos               = {};
    uint64_t      radius            = 100;
    double        factor            = 1;
    double        sigma             = 2;
  };

  struct resource_json_t {
    std::string   name;
    uint64_t      count   = utils_t::npos;
  };

  struct resource_info_json_t {
    std::string   name;
    uint64_t      stack_size    = utils_t::npos;
    bool          extractable   = false;
  };

  struct recipe_item_json_t {
    std::string   name    = {};
    uint64_t      count   = utils_t::npos;
  };

  struct recipe_json_t {
    using in_t  = std::map<std::string/*name*/, recipe_item_json_t>;
    using out_t = std::map<std::string/*name*/, recipe_item_json_t>;

    std::string   name              = {};
    in_t          in                = {};
    out_t         out               = {};
  };

  struct microbe_json_t {
    using resources_t   = std::unordered_map<std::string/*name*/, resource_json_t>;
    using code_t        = std::vector<uint8_t>;
    using xy_pos_t      = utils_t::xy_pos_t;

    std::string   family;
    code_t        code;
    resources_t   resources;
    xy_pos_t      pos;
    uint64_t      age;
    uint64_t      direction;
    int64_t       energy_remaining;
  };

  struct config_json_t {
    using debug_t       = std::set<std::string>;
    using resources_t   = std::unordered_map<std::string/*name*/, resource_info_json_t>;
    using areas_t       = std::vector<area_json_t>;
    using recipes_t     = std::unordered_map<std::string/*name*/, recipe_json_t>;
    using xy_pos_t      = utils_t::xy_pos_t;

    uint64_t      x_max                       = 1000;
    uint64_t      y_max                       = 1000;
    uint64_t      code_size                   = 32;
    uint64_t      age_max                     = 10000;
    uint64_t      energy_remaining            = 10;
    uint64_t      interval_update_world_ms    = 100;
    uint64_t      interval_save_world_ms      = 60 * 1000;
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
  };

  struct context_json_t {
    using microbes_t = std::unordered_map<utils_t::xy_pos_t, microbe_json_t, utils_t::xy_pos_hash_t>;

    config_json_t   config        = {};
    microbes_t      microbes      = {};
    stats_json_t    stats         = {};
  };

  ////////////////////////////////////////////////////////////////////////////////

  struct microbe_t {
    using resources_t   = std::unordered_map<std::string/*name*/, resource_json_t>;
    using code_t        = std::vector<uint8_t>;
    using xy_pos_t      = utils_t::xy_pos_t;

    std::string   family;
    code_t        code;
    resources_t   resources;
    xy_pos_t      pos;
    uint64_t      age;
    uint64_t      direction;
    int64_t       energy_remaining;

    bool from_json(const microbe_json_t& microbe_json);
    bool to_json(microbe_json_t& microbe_json) const;
    bool validation(const config_t& config);
    void init(const config_t& config);
  };

  struct config_t {
    using debug_t       = std::set<std::string>;
    using resources_t   = std::unordered_map<std::string/*name*/, resource_info_json_t>;
    using areas_t       = std::unordered_multimap<std::string/*resource*/, area_json_t>;
    using recipes_t     = std::unordered_map<std::string/*name*/, recipe_json_t>;
    using xy_pos_t      = utils_t::xy_pos_t;

    uint64_t      x_max;
    uint64_t      y_max;
    uint64_t      code_size;
    uint64_t      health_max;
    uint64_t      age_max;
    uint64_t      energy_remaining;
    uint64_t      interval_update_world_ms;
    uint64_t      interval_save_world_ms;
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

  struct context_t {
    using microbes_t = std::unordered_map<utils_t::xy_pos_t, microbe_t, utils_t::xy_pos_hash_t>;

    config_t        config        = {};
    microbes_t      microbes      = {};
    stats_json_t    stats         = {};

    bool from_json(const context_json_t& context_json);
    bool to_json(context_json_t& context_json) const;
  };

  struct world_t {
    // using microbes_t = std::unordered_map<utils_t::xy_pos_t, microbe_t, utils_t::xy_pos_hash_t>;

    context_t          ctx              = {};
    std::string        file_name        = {};

    uint64_t           time_ms          = {};
    uint64_t           update_world_ms  = {};
    uint64_t           save_world_ms    = {};

    void update();
    void update_ctx();
    void update_mind(microbe_t& microbe);
    void init();
    void load();
    void save();

    bool pos_valid(const utils_t::xy_pos_t& pos) const {
      TRACE_GENESIS;
      return pos.first < ctx.config.x_max && pos.second < ctx.config.y_max;
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
    JSON_SAVE2(json, area_json, name);
    JSON_SAVE2(json, area_json, resource);
    JSON_SAVE2(json, area_json, pos);
    JSON_SAVE2(json, area_json, radius);
    JSON_SAVE2(json, area_json, factor);
    JSON_SAVE2(json, area_json, sigma);
  }

  inline void from_json(const nlohmann::json& json, area_json_t& area_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, area_json, name);
    JSON_LOAD2(json, area_json, resource);
    JSON_LOAD2(json, area_json, pos);
    JSON_LOAD2(json, area_json, radius);
    JSON_LOAD2(json, area_json, factor);
    JSON_LOAD2(json, area_json, sigma);
  }

  inline void to_json(nlohmann::json& json, const resource_json_t& resource_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource_json, name);
    JSON_SAVE2(json, resource_json, count);
  }

  inline void from_json(const nlohmann::json& json, resource_json_t& resource_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource_json, name);
    JSON_LOAD2(json, resource_json, count);
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

  inline void to_json(nlohmann::json& json, const recipe_item_json_t& recipe_item_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, recipe_item_json, name);
    JSON_SAVE2(json, recipe_item_json, count);
  }

  inline void from_json(const nlohmann::json& json, recipe_item_json_t& recipe_item_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, recipe_item_json, name);
    JSON_LOAD2(json, recipe_item_json, count);
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

  inline void to_json(nlohmann::json& json, const microbe_json_t& microbe_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, microbe_json, family);
    JSON_SAVE2(json, microbe_json, code);
    JSON_SAVE2(json, microbe_json, resources);
    JSON_SAVE2(json, microbe_json, pos);
    JSON_SAVE2(json, microbe_json, age);
    JSON_SAVE2(json, microbe_json, direction);
    JSON_SAVE2(json, microbe_json, energy_remaining);
  }

  inline void from_json(const nlohmann::json& json, microbe_json_t& microbe_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, microbe_json, family);
    JSON_LOAD2(json, microbe_json, code);
    JSON_LOAD2(json, microbe_json, resources);
    JSON_LOAD2(json, microbe_json, pos);
    JSON_LOAD2(json, microbe_json, age);
    JSON_LOAD2(json, microbe_json, direction);
    JSON_LOAD2(json, microbe_json, energy_remaining);
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
    JSON_LOAD2(json, config_json, debug);
    JSON_LOAD2(json, config_json, resources);
    JSON_LOAD2(json, config_json, areas);
    JSON_LOAD2(json, config_json, recipes);
    JSON_LOAD2(json, config_json, spawn_pos);
    JSON_LOAD2(json, config_json, spawn_radius);
    JSON_LOAD2(json, config_json, spawn_min_count);
    JSON_LOAD2(json, config_json, spawn_max_count);
  }

  inline void to_json(nlohmann::json& json, const stats_json_t& stats_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, stats_json, age);
    JSON_SAVE2(json, stats_json, microbes_count);
  }

  inline void from_json(const nlohmann::json& json, stats_json_t& stats_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, stats_json, age);
    JSON_LOAD2(json, stats_json, microbes_count);
  }

  inline void to_json(nlohmann::json& json, const context_json_t& context_json) {
    TRACE_GENESIS;
    JSON_SAVE2(json, context_json, config);
    JSON_SAVE2(json, context_json, microbes);
    JSON_SAVE2(json, context_json, stats);
  }

  inline void from_json(const nlohmann::json& json, context_json_t& context_json) {
    TRACE_GENESIS;
    JSON_LOAD2(json, context_json, config);
    JSON_LOAD2(json, context_json, microbes);
    JSON_LOAD2(json, context_json, stats);
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

  bool microbe_t::from_json(const microbe_json_t& microbe_json) {
    TRACE_GENESIS;

    family             = microbe_json.family;
    code               = microbe_json.code;
    resources          = microbe_json.resources;
    pos                = microbe_json.pos;
    age                = microbe_json.age;
    direction          = microbe_json.direction;
    energy_remaining   = microbe_json.energy_remaining;

    return true;
  }

  bool microbe_t::to_json(microbe_json_t& microbe_json) const {
    TRACE_GENESIS;

    microbe_json.family             = family;
    microbe_json.code               = code;
    microbe_json.resources          = resources;
    microbe_json.pos                = pos;
    microbe_json.age                = age;
    microbe_json.direction          = direction;
    microbe_json.energy_remaining   = energy_remaining;

    return true;
  }

  bool microbe_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (family.empty()) {
      LOG_GENESIS(ERROR, "invalid microbe_t::family %s", family.c_str());
      return false;
    }

    while (code.size() < config.code_size) {
      code.push_back(utils_t::rand_u64() % 0xFF);
    }
    code.resize(config.code_size);

    for (auto& [key, resource] : resources) {
      if (key != resource.name || !config.resources.count(key)) {
        LOG_GENESIS(ERROR, "invalid microbe_t::resources %s", key.c_str());
        return false;
      }
      resource.count = std::min(resource.count, config.resources.at(key).stack_size);
    }

    for (auto& [_, resource] : config.resources) {
      if (!resources.count(resource.name)) {
        resources[resource.name] = {.name = resource.name, .count = {}};
      }
    }

    if (pos.first >= config.x_max || pos.second >= config.y_max) {
      LOG_GENESIS(ERROR, "invalid microbe_t::pos %zd %zd", pos.first, pos.second);
      return false;
    }

    // age

    direction %= utils_t::direction_max;

    return true;
  }

  void microbe_t::init(const config_t& config) {
    TRACE_GENESIS;

    family             = "r" + std::to_string(utils_t::rand_u64());
    // code
    resources          = {};
    pos                = {utils_t::rand_u64() % config.x_max, utils_t::rand_u64() % config.y_max};
    age                = {};
    direction          = utils_t::rand_u64() % utils_t::direction_max;
    energy_remaining   = {};

    resources[utils_t::ENERGY] = {utils_t::ENERGY, config.resources.at(utils_t::ENERGY).stack_size / 2 };
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

    for (auto& [key, resource] : resources) {
      if (key != resource.name) {
        LOG_GENESIS(ERROR, "invalid config_t::resources %s", key.c_str());
        return false;
      }
    }

    if (!resources.count(utils_t::ENERGY)) {
      LOG_GENESIS(ERROR, "invalid config_t::resources %s", utils_t::ENERGY.c_str());
      return false;
    }

    areas = {};
    for (const auto& area : config_json.areas) {
      if (!resources.count(area.resource)) {
        LOG_GENESIS(ERROR, "invalid config_t::area %s", area.name.c_str());
        return false;
      }
      areas.insert({area.resource, area});
    }

    if (areas.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::areas %zd", areas.size());
      return false;
    }

    recipes = config_json.recipes;
    if (recipes.size() < 1 || recipes.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::recipes %zd", recipes.size());
      return false;
    }

    for (auto& [key, recipe] : recipes) {
      if (key != recipe.name) {
        LOG_GENESIS(ERROR, "invalid config_t::recipe %s", key.c_str());
        return false;
      }
      for (auto& [key, recipe_item] : recipe.in) {
        if (key != recipe_item.name || !resources.count(key)) {
          LOG_GENESIS(ERROR, "invalid config_t::recipe %s", key.c_str());
          return false;
        }
      }
      for (auto& [key, recipe_item] : recipe.out) {
        if (key != recipe_item.name || !resources.count(key)) {
          LOG_GENESIS(ERROR, "invalid config_t::recipe %s", key.c_str());
          return false;
        }
      }
      if (recipe.in.empty() && recipe.out.empty()) {
        LOG_GENESIS(ERROR, "invalid config_t::recipe %s", key.c_str());
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
    config_json.debug                      = debug;
    config_json.resources                  = resources;
    config_json.areas                      = {};
    for (const auto& [_, area] : areas) {
      config_json.areas.push_back(area);
    }
    config_json.recipes                    = recipes;
    config_json.spawn_pos                  = spawn_pos;
    config_json.spawn_radius               = spawn_radius;
    config_json.spawn_min_count            = spawn_min_count;
    config_json.spawn_max_count            = spawn_max_count;

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool context_t::from_json(const context_json_t& context_json) {
    TRACE_GENESIS;

    if (!config.from_json(context_json.config)) {
      LOG_GENESIS(ERROR, "invalid config_json_t::config");
      return false;
    }

    microbes = {};
    for (const auto& [_, microbe_json] : context_json.microbes) {
      microbe_t microbe;
      microbe.from_json(microbe_json);
      if (microbe.validation(config)) {
        microbes.insert({microbe.pos, microbe});
      }
    }

    stats = context_json.stats;

    return true;
  }

  bool context_t::to_json(context_json_t& context_json) const {
    TRACE_GENESIS;

    if (!config.to_json(context_json.config)) {
      LOG_GENESIS(ERROR, "invalid context_t::config");
      return false;
    }

    context_json.microbes = {};
    for (const auto& [key, microbe] : microbes) {
      microbe_json_t microbe_json;
      microbe.to_json(microbe_json);
      context_json.microbes[key] = microbe_json;
    }

    context_json.stats = stats;

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  void world_t::update() {
    TRACE_GENESIS;

    time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    LOG_GENESIS(TIME, "time %zd", time_ms);

    if (update_world_ms < time_ms) {
      LOG_GENESIS(TIME, "update_world_ms %zd   %zd", time_ms, time_ms - update_world_ms);
      update_world_ms = time_ms + ctx.config.interval_update_world_ms;
      update_ctx();
    }

    if (save_world_ms < time_ms) {
      LOG_GENESIS(TIME, "save_world_ms %zd   %zd", time_ms, time_ms - save_world_ms);
      save_world_ms = time_ms + ctx.config.interval_save_world_ms;
      save();
    }

    LOG_GENESIS(TIME, "time end %zd", time_ms);
  }

  void world_t::update_ctx() {
    TRACE_GENESIS;

    for (auto& [_, microbe] : ctx.microbes) {
      microbe.energy_remaining += ctx.config.energy_remaining;
    }

    for (auto& [pos, microbe] : ctx.microbes) {
      // if (microbe.alive()) {
      //   continue;
      // }

      while (microbe.energy_remaining > 0) {
        microbe.energy_remaining--;
        update_mind(microbe);
        if (pos != microbe.pos) {
          auto nh = ctx.microbes.extract(pos);
          nh.key() = microbe.pos;
          ctx.microbes.insert(move(nh));
        }
      }

      microbe.age++;
      auto& count = microbe.resources[utils_t::ENERGY].count;
      if (count)
        count--;
    }

    std::erase_if(ctx.microbes, [this](auto& kv) {
        auto& microbe = kv.second;
        return microbe.resources[utils_t::ENERGY].count <= 0
            || microbe.age > ctx.config.age_max; });

    {
      if (ctx.microbes.size() <= ctx.config.spawn_min_count) {
        while (ctx.microbes.size() < ctx.config.spawn_max_count) {
          microbe_t microbe;
          microbe.init(ctx.config);
          microbe.pos = ctx.config.spawn_pos;
          microbe.pos.first  += 2 * utils_t::rand_u64() % ctx.config.spawn_radius - ctx.config.spawn_radius;
          microbe.pos.second += 2 * utils_t::rand_u64() % ctx.config.spawn_radius - ctx.config.spawn_radius;
          if (microbe.validation(ctx.config) && !ctx.microbes.contains(microbe.pos)) {
            ctx.microbes[microbe.pos] = std::move(microbe);
          } else {
            break;
          }
        }
      }
    }

    // stats
    {
      ctx.stats.age++;
      ctx.stats.microbes_count = ctx.microbes.size();
    }
  }

  void world_t::update_mind(microbe_t& microbe) {
    TRACE_GENESIS;

    auto& code = microbe.code;
    uint8_t seed = code.at(utils_t::CODE_SEED);

    utils_t::hash_data_t hd = {
      0,   // rip
      code.at(utils_t::CODE_HASH0),
      code.at(utils_t::CODE_HASH1),
      0,   // cmd
      0,   // arg
      0,   // reserved
    };

    uint64_t rip_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
    uint64_t rip = {};
    utils_t::load_by_hash<uint16_t>(code, rip_addr, rip);
    rip++;
    rip %= code.size();
    utils_t::save_by_hash<uint16_t>(code, rip_addr, rip);
    LOG_GENESIS(MIND, "rip: %zd", rip);

    hd.rip = rip;
    hd.arg++;

    uint64_t cmd_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
    uint64_t cmd = {};
    utils_t::load_by_hash<uint8_t>(code, cmd_addr, cmd);
    LOG_GENESIS(MIND, "cmd: %zd", cmd);

    switch (cmd % 32) {
      case 0: {
        LOG_GENESIS(MIND, "NOP");
        break;

      } case 1: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        LOG_GENESIS(MIND, "BR <%d>", opnd1);

        rip += opnd1;
        utils_t::save_by_hash<uint16_t>(code, rip_addr, rip);
        break;

      } case 2: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        LOG_GENESIS(MIND, "BR_ABS <%d>", opnd1);

        rip = opnd1;
        utils_t::save_by_hash<uint16_t>(code, rip_addr, rip);
        break;

      } case 3: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        hd.arg++;
        uint64_t opnd2 = {};
        uint64_t opnd2_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint8_t>(code, opnd2_addr, opnd2);

        LOG_GENESIS(MIND, "SET_U8 <%d> <%d>", opnd1, opnd2);

        code.at(opnd1 % code.size()) = opnd2;
        break;

      } case 4: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        hd.arg++;
        uint64_t opnd2 = {};
        uint64_t opnd2_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd2_addr, opnd2);

        LOG_GENESIS(MIND, "SET_U8 <%d> <%d>", opnd1, opnd2);

        code.at(opnd1 % code.size()) = code.at(opnd2 % code.size());
        break;

      } case 5: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        hd.arg++;
        uint64_t opnd2 = {};
        uint64_t opnd2_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd2_addr, opnd2);

        hd.arg++;
        uint64_t opnd3 = {};
        uint64_t opnd3_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd3_addr, opnd3);

        LOG_GENESIS(MIND, "ADD <%d> <%d> <%d>", opnd1, opnd2, opnd3);

        code.at(opnd1 % code.size()) = code.at(opnd2 % code.size()) + code.at(opnd3 % code.size());
        break;

      } case 6: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        hd.arg++;
        uint64_t opnd2 = {};
        uint64_t opnd2_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd2_addr, opnd2);

        hd.arg++;
        uint64_t opnd3 = {};
        uint64_t opnd3_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd3_addr, opnd3);

        LOG_GENESIS(MIND, "SUB <%d> <%d> <%d>", opnd1, opnd2, opnd3);

        code.at(opnd1 % code.size()) = code.at(opnd2 % code.size()) - code.at(opnd3 % code.size());
        break;

        // MULT
        // DIV
        // IF

      } case 16: {
        hd.arg++;
        uint64_t dir = {};
        uint64_t dir_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint8_t>(code, dir_addr, dir);

        LOG_GENESIS(MIND, "TURN <%zd>", dir);

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

        utils_t::xy_pos_t pos = microbe.pos;
        utils_t::xy_pos_t pos_n = pos_next(pos, microbe.direction);
        if (pos != pos_n && !ctx.microbes.contains(pos_n)) {
          microbe.pos = pos_n;
        }

        break;

      } case 19: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint8_t>(code, opnd1_addr, opnd1);

        hd.arg++;
        uint64_t opnd2 = {};
        uint64_t opnd2_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd2_addr, opnd2);

        LOG_GENESIS(MIND, "CLONE <%zd> <%zd>", opnd1, opnd2);

        uint64_t direction = opnd1 % utils_t::direction_max;
        double probability = 0.1 * opnd2 / 0xFFFF / code.size();

        utils_t::xy_pos_t pos_n = pos_next(microbe.pos, direction);
        if (microbe.resources.at(utils_t::ENERGY).count > 600 && !ctx.microbes.contains(pos_n)) {
          for (auto [_, resource]  : microbe.resources) {
            resource.count /= 2;
          }
          auto microbe_child = microbe;
          microbe_child.pos = pos_n;
          for (auto& byte : microbe_child.code) {
            if (utils_t::rand_u64() % 0xFFFF > probability) {
              byte = utils_t::rand_u64();
            }
          }
          if (microbe_child.validation(ctx.config)) {
            ctx.microbes.insert({microbe_child.pos, microbe_child});
          }
        }
        break;

      } case 20: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        LOG_GENESIS(MIND, "RECIPE <%zd>", opnd1);

        [this, &microbe, opnd1]() {
          const auto& recipes = ctx.config.recipes;
          auto it = recipes.begin();
          std::advance(it, opnd1 % recipes.size());
          const auto& recipe = it->second;
          LOG_GENESIS(MIND, "name %s", recipe.name.c_str());

          auto& resources = microbe.resources;
          for (const auto& [_, recipe_item] : recipe.in) {
            if (resources[recipe_item.name].count < recipe_item.count) {
              return;
            }
          }

          for (const auto& [_, recipe_item] : recipe.in) {
            resources[recipe_item.name].count -= recipe_item.count;
          }

          for (const auto& [_, recipe_item] : recipe.out) {
            uint64_t count = recipe_item.count;
            const auto& resource_info = ctx.config.resources.at(recipe_item.name);

            if (resource_info.extractable) {
              double multiplier = {};
              const auto range = ctx.config.areas.equal_range(recipe_item.name);

              for (auto it = range.first; it != range.second; ++it) {
                const auto& area = it->second;
                uint64_t dist = utils_t::distance(microbe.pos, area.pos);
                multiplier += area.factor * std::max(0.,
                    1. - std::pow(std::abs((double) dist / area.radius), area.sigma));
                LOG_GENESIS(MIND, "multiplier %f", multiplier);
              }
              count *= multiplier;
              LOG_GENESIS(MIND, "count %zd", count);
            }

            resources[recipe_item.name].count = std::min(resource_info.stack_size,
                resources[recipe_item.name].count + count);
          }
        }();

        break;

      } default: {
        LOG_GENESIS(MIND, "NOTHING");
        break;
      }
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
    if (!utils_t::load(json, file_name)) {
      LOG_GENESIS(ERROR, "can not load file %s", file_name.c_str());
      throw std::runtime_error("invalid json");
    }

    context_json_t ctx_json = json;

    if (!ctx.from_json(ctx_json)) {
      LOG_GENESIS(ERROR, "invalid context");
      throw std::runtime_error("invalid context");
    }
  }

  void world_t::save() {
    TRACE_GENESIS;

    context_json_t ctx_json;
    if (!ctx.to_json(ctx_json)) {
      LOG_GENESIS(ERROR, "invalid context");
      throw std::runtime_error("invalid context");
    }

    nlohmann::json json = ctx_json;

    utils_t::save(json, file_name);
  }

  ////////////////////////////////////////////////////////////////////////////////
}

