
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <random>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <list>

#include "json.hpp"
#include "debug_logger.h"



#define TRACE_TEST                 DEBUG_LOGGER("test ", logger_indent_test_t::indent)
#define LOG_TEST(...)              DEBUG_LOG("test ", logger_indent_test_t::indent, __VA_ARGS__)

struct logger_indent_test_t   : logger_indent_t<logger_indent_test_t> { };



using namespace std::chrono_literals;



struct env_t {
  struct tile_t;
  struct bot_t;
  struct system_t;
  struct world_t;

  using system_sptr_t = std::shared_ptr<system_t>;
  using bot_sptr_t = std::shared_ptr<bot_t>;

  struct config_t {
    size_t n;
    size_t m;

    config_t() :
      n(10),
      m(20)
    { }
  };

  struct utils_t {
    inline static size_t seed;

    static void init(const config_t& config) {
      seed = time(0);
    }

    static double rand_double() {
      static std::mt19937 gen(seed);
      static std::uniform_real_distribution<double> dis;
      return dis(gen);
    }

    static size_t rand_u64() {
      static std::mt19937 gen(seed);
      static std::uniform_int_distribution<size_t> dis;
      return dis(gen);
    }

    static bool load(const nlohmann::json& json) {
      try {
        auto& json_utils = json.at("utils");
        seed = json_utils.at("seed");
        return true;
      } catch (const std::exception& e) {
        std::cerr << "WARN: " << e.what() << std::endl;
        return false;
      }
    }

    static void save(nlohmann::json& json) {
      auto& json_utils = json["utils"];
      json_utils["seed"] = seed;
    }

    static bool load(nlohmann::json& json, const std::string& name) {
      TRACE_TEST;
      try {
        std::ifstream file(name);
        file >> json;
        return true;
      } catch (const std::exception& e) {
        return false;
      }
    }

    static bool save(const nlohmann::json& json, const std::string& name) {
      TRACE_TEST;
      try {
        std::ofstream file(name);
        file << /*std::setw(2) <<*/ json;
        return true;
      } catch (const std::exception& e) {
        std::cerr << "WARN: " << e.what() << std::endl;
        return false;
      }
    }
  };

  struct world_t {
    size_t n;
    size_t m;
    std::vector<bot_sptr_t>      bots;
    std::vector<tile_t>          tiles;
    std::vector<system_sptr_t>   systems;

    void init(const config_t& config) {
      n = config.n;
      m = config.m;
      // systems.push_back(nullptr);
    }

    void update() {
      for (auto& system : systems) {
        // system.update(*this, );
      }
    }

    bool load(const nlohmann::json& json) {
      try {
        auto& json_world = json.at("world");
        n = json_world.at("n");
        m = json_world.at("m");
        return true;
      } catch (const std::exception& e) {
        std::cerr << "WARN: " << e.what() << std::endl;
        return false;
      }
    }

    void save(nlohmann::json& json) {
      auto& json_world = json["world"];
      json_world["n"] = n;
      json_world["m"] = m;
    }
  };

  struct skill_t {
    uint16_t   value;
    uint8_t    volatility;
  };

  struct tile_t {
    bool                   is_impassable;
    uint32_t               age;
    std::vector<skill_t>   skills;
  };

  struct bot_t {
    std::vector<uint8_t>   data;          // 64
    std::vector<uint8_t>   regs;          // 32
    std::vector<uint8_t>   interrupts;    // 8
    std::vector<skill_t>   skills;
    uint8_t                rip;
    uint8_t                energy_daily;
    size_t                 position;

    void update(world_t& world) {
      TRACE_TEST;
    }
  };

  struct system_t {
    virtual void update(world_t& world, bot_t& bot) = 0;
  };



  world_t                      world;



  void update() {
    world.update();
  }

  void init(const config_t& config) {
    utils_t::init(config);
    world.init(config);
  }

  bool load(const std::string& name) {
    nlohmann::json json;
    return !utils_t::load(json, name)
      || !utils_t::load(json)
      || !world.load(json);
  }

  void save(const std::string& name) {
    nlohmann::json json;
    utils_t::save(json);
    world.save(json);
    utils_t::save(json, name);
  }
};



int main(int argc, char* argv[]) {
  TRACE_TEST;

  env_t env;
  std::string name = "default.json";

  if (argc > 1) {
    name = argv[1];
  }

  if (!env.load(name)) {
    env_t::config_t config;
    env.init(config);
  }

  env.update();

  env.save(name);

  return 0;
}

