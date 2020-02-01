
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

#define JSON_SAVE(json, name) json[#name] = name
#define JSON_LOAD(json, name) name = json.value(#name, name)

#define TRACE_TEST                 DEBUG_LOGGER("test ", logger_indent_test_t::indent)
#define LOG_TEST(...)              DEBUG_LOG("test ", logger_indent_test_t::indent, __VA_ARGS__)

using namespace std::chrono_literals;

struct logger_indent_test_t   : logger_indent_t<logger_indent_test_t> { };




namespace genesis_n {
  struct world_t;
  struct system_t;
  struct bot_t;

  using bot_sptr_t = std::shared_ptr<bot_t>;
  using system_sptr_t = std::shared_ptr<system_t>;

  struct parameters_t {
    size_t   position_m;
    size_t   position_max;

    size_t   bot_code_size;
    size_t   bot_regs_size;
    size_t   bot_interrupts_size;
    size_t   bot_energy_max;
    size_t   bot_energy_daily;

    size_t   system_min_bot_count;

    parameters_t() :
      position_m(10),
      position_max(100),
      bot_code_size(64),
      bot_regs_size(32),
      bot_interrupts_size(8),
      bot_energy_max(100),
      bot_energy_daily(5),
      system_min_bot_count(position_max / 2)
    { }

    void load(nlohmann::json& json) {
      TRACE_TEST;
      if (!json.is_object())
        return;

      JSON_LOAD(json, position_m);
      JSON_LOAD(json, position_max);
      JSON_LOAD(json, bot_code_size);
      JSON_LOAD(json, bot_regs_size);
      JSON_LOAD(json, bot_interrupts_size);
      JSON_LOAD(json, bot_energy_max);
      JSON_LOAD(json, bot_energy_daily);
      JSON_LOAD(json, system_min_bot_count);

      position_max = (position_max / position_m) * position_m;   // correct, position_max % position_m == 0
    }

    void save(nlohmann::json& json) {
      TRACE_TEST;
      if (!json.is_object())
        json = nlohmann::json::object();

      JSON_SAVE(json, position_m);
      JSON_SAVE(json, position_max);
      JSON_SAVE(json, bot_code_size);
      JSON_SAVE(json, bot_regs_size);
      JSON_SAVE(json, bot_interrupts_size);
      JSON_SAVE(json, bot_energy_max);
      JSON_SAVE(json, bot_energy_daily);
      JSON_SAVE(json, system_min_bot_count);
    }
  };

  struct utils_t {
    inline static size_t seed = time(0);
    inline static parameters_t parameters = {};

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

    static void update_parameters(const std::string& name) {
      TRACE_TEST;
      nlohmann::json json;
      utils_t::load(json, name);
      parameters.load(json);
      json = {};
      parameters.save(json);
      utils_t::save(json, name);
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
        file << std::setw(2) << json;
        return true;
      } catch (const std::exception& e) {
        std::cerr << "WARN: " << e.what() << std::endl;
        return false;
      }
    }
  };

  struct stats_t {
    size_t   bots_alive;

    stats_t() :
      bots_alive(0)
    { }
  };

  struct bot_t {
    std::vector<uint8_t>   code;
    std::vector<uint8_t>   regs;   // ?uint16_t
    std::vector<uint8_t>   interrupts;
    size_t                 rip;
    size_t                 mineral;
    size_t                 sunlight;
    size_t                 energy;
    size_t                 energy_daily;
    size_t                 position;

    void update_parameters() {
      TRACE_TEST;
      code.resize(utils_t::parameters.bot_code_size);
      regs.resize(utils_t::parameters.bot_regs_size);
      interrupts.resize(utils_t::parameters.bot_interrupts_size);

      std::for_each(code.begin(), code.end(), [](auto& b) { b = utils_t::rand_u64() % 0xFF; });
      std::for_each(regs.begin(), regs.end(), [](auto& b) { b = utils_t::rand_u64() % 0xFF; });
      std::for_each(interrupts.begin(), interrupts.end(), [](auto& b) { b = utils_t::rand_u64() % 0xFF; });

      rip            = utils_t::rand_u64() % code.size();
      mineral        = utils_t::parameters.bot_energy_max;
      sunlight       = utils_t::parameters.bot_energy_max;
      energy         = utils_t::parameters.bot_energy_max;
      energy_daily   = utils_t::parameters.bot_energy_daily;
      position       = utils_t::rand_u64() % (utils_t::parameters.position_max);
    }

    bool load(nlohmann::json& json) {
      TRACE_TEST;
      if (!json.is_object())
        return false;

      JSON_LOAD(json, code);
      JSON_LOAD(json, regs);
      JSON_LOAD(json, interrupts);
      JSON_LOAD(json, rip);
      JSON_LOAD(json, mineral);
      JSON_LOAD(json, sunlight);
      JSON_LOAD(json, energy);
      JSON_LOAD(json, energy_daily);
      JSON_LOAD(json, position);

      code.resize(utils_t::parameters.bot_code_size);
      regs.resize(utils_t::parameters.bot_regs_size);
      interrupts.resize(utils_t::parameters.bot_interrupts_size);

      return true;
    }

    void save(nlohmann::json& json) {
      TRACE_TEST;

      if (!json.is_object())
        json = nlohmann::json::object();

      JSON_SAVE(json, code);
      JSON_SAVE(json, regs);
      JSON_SAVE(json, interrupts);
      JSON_SAVE(json, rip);
      JSON_SAVE(json, mineral);
      JSON_SAVE(json, sunlight);
      JSON_SAVE(json, energy);
      JSON_SAVE(json, energy_daily);
      JSON_SAVE(json, position);
    }
  };

  struct world_t {
    stats_t                      stats;
    std::vector<bot_sptr_t>      bots;
    std::vector<system_sptr_t>   systems;

    world_t();

    void update();

    void load(const std::string& name) {
      TRACE_TEST;
      nlohmann::json json;
      utils_t::load(json, name);

      bots.resize(utils_t::parameters.position_max);

      auto& bots_json = json["bots"];
      if (bots_json.is_array()) {
        for (size_t i{}; i < bots_json.size(); ++i) {
          bot_t bot;
          bot.update_parameters();
          if (bot.load(bots_json[i]) && bot.position < bots.size()) {
            bots[bot.position] = std::make_shared<bot_t>(bot);
          }
        }
      }
    }

    void save(const std::string& name) {
      TRACE_TEST;
      nlohmann::json json;

      auto& bots_json = json["bots"];
      bots_json = nlohmann::json::array();
      for (auto& bot : bots) {
        if (bot) {
          nlohmann::json json;
          bot->save(json);
          bots_json.push_back(json);
        }
      }

      utils_t::save(json, name);
    }
  };

  struct system_t {
    virtual void update(world_t& world) = 0;
  };

  struct system_bot_stats_t : system_t {
    void update(world_t& world) override {
      TRACE_TEST;
      auto& stats = world.stats;
      stats.bots_alive = 0;
      std::for_each(world.bots.begin(), world.bots.end(),
          [&stats](const auto& bot) { if (bot) stats.bots_alive++; });

      LOG_TEST("stats.bots_alive: %zd", stats.bots_alive);
    }
  };

  struct system_bot_generator_t : system_t {
    void update(world_t& world) override {
      TRACE_TEST;

      if (world.stats.bots_alive < utils_t::parameters.system_min_bot_count) {
        bot_t bot_new;
        bot_new.update_parameters();
        auto& bot = world.bots[bot_new.position];
        if (!world.bots[bot_new.position]) {
          bot = std::make_shared<bot_t>(bot_new);
        }
      }
    }
  };

  struct system_bot_updater_t : system_t {
    void update(world_t& world) override {
      TRACE_TEST;
    }
  };



  world_t::world_t() {
    systems.push_back(std::make_shared<system_bot_stats_t>());
    systems.push_back(std::make_shared<system_bot_generator_t>());
    systems.push_back(std::make_shared<system_bot_updater_t>());
  }

  void world_t::update() {
    TRACE_TEST;
    for (auto& system : systems) {
      system->update(*this);
    }
  }
}



int main(int argc, char* argv[]) {
  TRACE_TEST;

  std::string parameters_fname = "parameters.json";
  std::string world_fname = "world.json";

  if (argc > 2) {
    parameters_fname = argv[1];
    world_fname = argv[1];
  }

  using namespace genesis_n;

  utils_t::update_parameters(parameters_fname);

  world_t world;
  world.load(world_fname);

  for (size_t i{}; i < 1; ++i)
    world.update();

  world.save(world_fname);

  return 0;
}

