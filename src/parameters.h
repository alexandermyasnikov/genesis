
#pragma once

#include <cstddef>
#include <set>

#include "../json.hpp"
// #include "utils.h"

// XXX
#define JSON_SAVE(json, name) json[#name] = name
#define JSON_LOAD(json, name) name = json.value(#name, name)

namespace genesis_n {

  struct parameters_t {
    size_t   position_n               = 10;
    size_t   position_max             = 100;
    size_t   bot_code_size            = 64;
    size_t   bot_regs_size            = 32;
    size_t   bot_interrupts_size      = 8;
    size_t   bot_energy_max           = 100;
    size_t   bot_energy_daily         = 1;
    size_t   system_min_bot_count     = position_max / 10;
    size_t   system_max_bot_count     = position_max;
    size_t   system_interval_stats    = 1000;
    size_t   system_epoll_port        = 8282;
    size_t   time_ms                  = 0;
    size_t   interval_update_world_ms = 100;
    size_t   interval_save_world_ms   = 60 * 1000;
    size_t   interval_load_parameters_ms   = 60 * 1000;
    std::string   system_epoll_ip     = "127.0.0.1";
    std::string   world_file          = "world.json";
    std::string   parameters_file     = "parameters.json";
    std::set<std::string>   debug     = { "error" };

    void load(nlohmann::json& json);
    void save(nlohmann::json& json);
  };

  void parameters_t::load(nlohmann::json& json) {
    // TRACE_GENESIS;
    if (!json.is_object()) {
      // LOG_GENESIS(ERROR, "json is not object");
      return;
    }

    JSON_LOAD(json, position_n);
    JSON_LOAD(json, position_max);
    JSON_LOAD(json, bot_code_size);
    JSON_LOAD(json, bot_regs_size);
    JSON_LOAD(json, bot_interrupts_size);
    JSON_LOAD(json, bot_energy_max);
    JSON_LOAD(json, bot_energy_daily);
    JSON_LOAD(json, system_min_bot_count);
    JSON_LOAD(json, system_max_bot_count);
    JSON_LOAD(json, system_interval_stats);
    JSON_LOAD(json, system_epoll_port);
    JSON_LOAD(json, system_epoll_ip);
    JSON_LOAD(json, world_file);
    JSON_LOAD(json, parameters_file);
    // JSON_LOAD(json, time_ms);
    JSON_LOAD(json, interval_update_world_ms);
    JSON_LOAD(json, interval_save_world_ms);
    JSON_LOAD(json, interval_load_parameters_ms);
    JSON_LOAD(json, debug);

    position_max = (position_max / position_n) * position_n;
  }

  void parameters_t::save(nlohmann::json& json) {
    // TRACE_GENESIS;
    if (!json.is_object()) {
      json = nlohmann::json::object();
    }

    JSON_SAVE(json, position_n);
    JSON_SAVE(json, position_max);
    JSON_SAVE(json, bot_code_size);
    JSON_SAVE(json, bot_regs_size);
    JSON_SAVE(json, bot_interrupts_size);
    JSON_SAVE(json, bot_energy_max);
    JSON_SAVE(json, bot_energy_daily);
    JSON_SAVE(json, system_min_bot_count);
    JSON_SAVE(json, system_max_bot_count);
    JSON_SAVE(json, system_interval_stats);
    JSON_SAVE(json, system_epoll_port);
    JSON_SAVE(json, system_epoll_ip);
    JSON_SAVE(json, world_file);
    JSON_SAVE(json, parameters_file);
    JSON_SAVE(json, time_ms);
    JSON_SAVE(json, interval_update_world_ms);
    JSON_SAVE(json, interval_save_world_ms);
    JSON_SAVE(json, interval_load_parameters_ms);
    JSON_SAVE(json, debug);
  }

  ////////////////////////////////////////////////////////////////////////////////

}

