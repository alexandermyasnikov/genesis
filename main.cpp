
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



namespace genesis_n {
  struct world_t;
  struct system_t;
  struct tile_t;
  struct bot_t;
  struct tail_t;

  using bot_sptr_t = std::shared_ptr<bot_t>;
  using system_sptr_t = std::shared_ptr<system_t>;

  struct utils_t {
    inline static size_t seed = time(0);

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

  struct parameters_t {
    size_t   position_m;
    size_t   position_max;

    size_t   bot_code_size;
    size_t   bot_regs_size;
    size_t   bot_interrupts_size;
    size_t   bot_energy_daily;

    size_t   system_min_bot_count;

    parameters_t() :
      position_m(10),
      position_max(100),
      bot_code_size(64),
      bot_regs_size(32),
      bot_interrupts_size(8),
      bot_energy_daily(10),
      system_min_bot_count(position_max / 2)
    { }

    void load(nlohmann::json& json) {
      TRACE_TEST;
      if (!json.is_object())
        return;

      position_m             = json.value("position_m",             position_m);
      position_max           = json.value("position_max",           position_max);
      bot_code_size          = json.value("bot_code_size",          bot_code_size);
      bot_regs_size          = json.value("bot_regs_size",          bot_regs_size);
      bot_interrupts_size    = json.value("bot_interrupts_size",    bot_interrupts_size);
      bot_energy_daily       = json.value("bot_energy_daily",       bot_energy_daily);
      system_min_bot_count   = json.value("system_min_bot_count",   system_min_bot_count);

      position_max = (position_max / position_m) * position_m;   // correct, position_max % position_m == 0
    }

    void save(nlohmann::json& json) {
      TRACE_TEST;
      if (!json.is_object())
        json = nlohmann::json::object();

      json["position_m"]             = position_m;
      json["position_max"]           = position_max;
      json["bot_code_size"]          = bot_code_size;
      json["bot_regs_size"]          = bot_regs_size;
      json["bot_interrupts_size"]    = bot_interrupts_size;
      json["bot_energy_daily"]       = bot_energy_daily;
      json["system_min_bot_count"]   = system_min_bot_count;
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
    std::vector<uint8_t>   code;
    std::vector<uint8_t>   regs;
    std::vector<uint8_t>   interrupts;
    std::vector<skill_t>   skills;
    uint8_t                rip;
    uint8_t                energy_daily;
    size_t                 position;

    void update_parameters(parameters_t& parameters) {
      TRACE_TEST;
      code.resize(parameters.bot_code_size);
      regs.resize(parameters.bot_regs_size);
      interrupts.resize(parameters.bot_interrupts_size);

      std::for_each(code.begin(), code.end(), [](auto& b) { b = utils_t::rand_u64() % 0xFF; });
      std::for_each(regs.begin(), regs.end(), [](auto& b) { b = utils_t::rand_u64() % 0xFF; });
      std::for_each(interrupts.begin(), interrupts.end(), [](auto& b) { b = utils_t::rand_u64() % 0xFF; });

      // skill_t TODO

      rip = utils_t::rand_u64() % code.size();
      energy_daily = parameters.bot_energy_daily;
      position = utils_t::rand_u64() % (parameters.position_max);
    }

    bool load(nlohmann::json& json) {
      TRACE_TEST;
      if (!json.is_object())
        return false;

      code           = json.value("code", code);
      regs           = json.value("regs", regs);
      interrupts     = json.value("interrupts", interrupts);
      rip            = json.value("rip", rip);
      energy_daily   = json.value("energy_daily", energy_daily);
      position       = json.value("position", position);
      // skills      = json.value("skills", skills); // TODO

      // code.resize(parameters.bot_code_size);
      // regs.resize(parameters.bot_regs_size);
      // interrupts.resize(parameters.bot_interrupts_size);

      return true;
    }

    void save(nlohmann::json& json) {
      TRACE_TEST;

      if (!json.is_object())
        json = nlohmann::json::object();

      json["code"]           = code;
      json["regs"]           = regs;
      json["interrupts"]     = interrupts;
      json["rip"]            = rip;
      json["energy_daily"]   = energy_daily;
      json["position"]       = position;
      // json["skills"]      = skills; // TODO
    }
  };

  struct world_t {
    stats_t                      stats;
    parameters_t                 parameters;
    std::vector<bot_sptr_t>      bots;
    std::vector<tile_t>          tiles;
    std::vector<system_sptr_t>   systems;

    world_t();

    void update();

    void update_parameters(const std::string& name) {
      TRACE_TEST;
      nlohmann::json json;
      utils_t::load(json, name);
      parameters.load(json);
      parameters.save(json);
      utils_t::save(json, name);
    }

    void load(const std::string& name) {
      TRACE_TEST;
      nlohmann::json json;
      utils_t::load(json, name);

      bots.resize(parameters.position_max);
      tiles.resize(parameters.position_max);

      auto& bots_json = json["bots"];
      if (bots_json.is_array()) {
        for (size_t i{}; i < bots_json.size(); ++i) {
          bot_t bot;
          bot.update_parameters(parameters);
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

      if (world.stats.bots_alive < world.parameters.system_min_bot_count) {
        bot_t bot_new;
        bot_new.update_parameters(world.parameters);
        auto& bot = world.bots[bot_new.position];
        if (!world.bots[bot_new.position]) {
          bot = std::make_shared<bot_t>(bot_new);
        }
      }
    }
  };



  world_t::world_t() {
    systems.push_back(std::make_shared<system_bot_stats_t>());
    systems.push_back(std::make_shared<system_bot_generator_t>());
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

  world_t world;
  world.update_parameters(parameters_fname);
  world.load(world_fname);

  for (size_t i{}; i < 1; ++i)
    world.update();

  world.save(world_fname);

  return 0;
}

