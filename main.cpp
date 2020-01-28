
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



#define TRACE_TEST                 DEBUG_LOGGER("test ", LoggerIndentTest::indent)
#define LOG_TEST(...)              DEBUG_LOG("test ", LoggerIndentTest::indent, __VA_ARGS__)

struct LoggerIndentTest    : LoggerIndent<LoggerIndentTest> { };



using namespace std::chrono_literals;



struct env_t {
  struct bot_t;
  struct system_t;
  struct world_t;

  struct config_t {
    size_t seed;
    size_t n;
    size_t m;

    config_t() :
      n(10),
      m(20),
      seed(0)
    { }
  };

  struct utils_t {
    inline static size_t seed;

    static void init(const config_t& config) {
      seed = config.seed;
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
    enum class tile_t : uint8_t {
      EMPTY,
      WALL,
      FOOD,
      POISON,
    };

    size_t n;
    size_t m;
    // std::vector<tile_t> tiles;
    // std::vector<std::shared_ptr<bot_t>> bots;
    std::vector<std::vector<std::shared_ptr<bot_t>>> bots;
    std::vector<std::shared_ptr<system_t>> systems;

    void init(const config_t& config) {
      n = config.n;
      m = config.m;
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

  struct bot_t {
    std::array<uint8_t, 64> genes;
    size_t rip;
    size_t ind;
    size_t age;
    size_t energy;
    // size_t direction; // TODO

    void init(const config_t& config) {
      TRACE_TEST;
      for (auto& gene : genes) {
        gene = utils_t::rand_u64() % 256;
      }
      rip = 0;
      ind = 1;
      age = 0;
      energy = 100;
    }

    void update(world_t& world) {
      TRACE_TEST;
    }
  };

  struct system_t {
  };



  world_t world;



  void init(const config_t& config) {
    utils_t::init(config);
    world.init(config);
  }

  bool load(const std::string& name) {
    nlohmann::json json;
    if (!utils_t::load(json, name))
      return false;
    if (!utils_t::load(json))
      return false;
    if (!world.load(json))
      return false;
    return true;
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

  env.save(name);

  return 0;
}

