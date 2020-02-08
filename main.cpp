
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


#include "json.hpp"
#include "debug_logger.h"

#define JSON_SAVE(json, name) json[#name] = name
#define JSON_LOAD(json, name) name = json.value(#name, name)

#define TRACE_GENESIS {   \
  if (utils_t::parameters.debug.contains(utils_t::TRACE)) {   \
    DEBUG_LOGGER(utils_t::TRACE.c_str(), logger_indent_genesis_t::indent);   \
  }   \
}
#define LOG_GENESIS(name, ...) {   \
  if (utils_t::parameters.debug.contains(utils_t::name)) {   \
    DEBUG_LOG(utils_t::name.c_str(), logger_indent_genesis_t::indent, __VA_ARGS__);   \
  }   \
}



using namespace std::chrono_literals;

struct logger_indent_genesis_t   : logger_indent_t<logger_indent_genesis_t> { };



namespace genesis_n {
  struct world_t;
  struct system_t;
  struct bot_t;

  using bot_sptr_t = std::shared_ptr<bot_t>;
  using system_sptr_t = std::shared_ptr<system_t>;

  struct parameters_t {
    size_t   position_n               = 10;
    size_t   position_max             = 100;
    size_t   bot_code_size            = 64;
    size_t   bot_regs_size            = 32;
    size_t   bot_interrupts_size      = 8;
    size_t   bot_energy_max           = 100;
    size_t   bot_energy_daily         = 1;
    size_t   system_min_bot_count     = position_max / 10;
    size_t   system_interval_stats    = 1000;
    size_t   system_epoll_port        = 8282;
    size_t   time_ms                  = 0;
    size_t   interval_update_world_ms = 100;
    size_t   interval_save_world_ms   = 60 * 1000;
    size_t   interval_load_parameters_ms   = 60 * 1000;
    std::string   system_epoll_ip     = "127.0.0.1";
    std::string   world_file          = "world.json";
    std::string   parameters_file     = "parameters.json";
    std::set<std::string>   debug     = {"error"};

    void load(nlohmann::json& json);
    void save(nlohmann::json& json);
  };

  struct utils_t {
    inline static size_t seed = time(0);
    inline static parameters_t parameters = {};

    inline static std::string TRACE = "trace";
    inline static std::string EPOLL = "epoll";
    inline static std::string STATS = "stats";
    inline static std::string ERROR = "error";
    inline static std::string DEBUG = "debug";
    inline static std::string MIND  = "mind ";
    inline static std::string TIME  = "time ";
    inline static size_t npos       = std::string::npos;

    static double rand_double();
    static size_t rand_u64();
    static size_t position(size_t position, size_t direction);
    static std::pair<size_t, size_t> position_to_xy(size_t position);
    static size_t position_from_xy(size_t x, size_t y);
    static size_t n();
    static size_t m();
    static void update_parameters(const std::string& name);
    static void rename(const std::string& name_old, const std::string& name_new);
    static void remove(const std::string& name);
    static bool load(nlohmann::json& json, const std::string& name);
    static bool save(const nlohmann::json& json, const std::string& name);
  };

  struct http_parser_t {
    ;
  };

  struct stats_t {
    size_t   bots_alive   = 0;
    size_t   age_max      = 0;
    double   age_avg      = 0;
    size_t   energy       = 0;
    double   energy_avg   = 0;
  };

  struct bot_t {
    std::vector<uint8_t>   code;
    std::vector<uint8_t>   regs;          // TODO uint32_t
    std::vector<uint8_t>   interrupts;
    size_t                 rip            = 0;
    size_t                 mineral        = utils_t::parameters.bot_energy_max;
    size_t                 sunlight       = utils_t::parameters.bot_energy_max;
    size_t                 energy         = utils_t::parameters.bot_energy_max;
    size_t                 energy_daily   = utils_t::parameters.bot_energy_daily;
    size_t                 position       = utils_t::rand_u64() % (utils_t::parameters.position_max);
    size_t                 age            = 0;
    std::string            name           = "r" + std::to_string(utils_t::rand_u64());

    void update_parameters();
    bool load(nlohmann::json& json);
    void save(nlohmann::json& json);
  };

  struct world_t {
    stats_t                      stats;
    std::vector<bot_sptr_t>      bots;
    std::vector<system_sptr_t>   systems;
    size_t                       time_update_world_ms = 0;
    size_t                       time_save_words_ms = 0;
    size_t                       time_load_parameters_ms = 0;

    world_t();

    void update();
    void update_parameters();
    void load();
    void save();
  };

  struct system_t {
    virtual void update_parameters() { }
    virtual void update(world_t& world) = 0;
  };

  struct system_bot_stats_t : system_t {
    size_t time_update_stats_ms = 0;

    void update(world_t& world) override;
  };

  struct system_bot_generator_t : system_t {
    void update(world_t& world) override;
  };

  struct system_bot_updater_t : system_t {
    void update(world_t& world) override;
  };

  struct system_epoll_t : system_t {
    using buffers_t = std::map<int/*fd*/, std::pair<std::deque<char>/*req*/, std::deque<char>/*resp*/>>;

    static const size_t  events_count = 16;
    int                  fd_listen;
    int                  fd_epoll;
    epoll_event          events[events_count];
    buffers_t            buffers;
    char                 buffer_tmp[1024];
    bool                 is_run;

    inline static const std::string GET = "GET ";
    inline static const std::string NL = "\r\n";
    inline static const std::string NL2 = "\r\n\r\n";
    inline static const std::string ct_html = "text/html; charset=UTF-8";
    inline static const std::string ct_json = "application/json";

    void update_parameters() override;
    void update(world_t& world) override;
    std::string get_response(const std::string ct, const std::string& body);
    void close_all();
    int set_nonblock(int fd);
    int set_epoll_ctl(int fd, int events, int op);
    void process_write(int fd);
    void process_read(int fd);
  };

  struct system_time_t : system_t {
    void update(world_t& world) override;
  };

  ////////////////////////////////////////////////////////////////////////////////

  void parameters_t::load(nlohmann::json& json) {
    TRACE_GENESIS;
    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
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
    TRACE_GENESIS;
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

  double utils_t::rand_double() {
    static std::mt19937 gen(seed);
    static std::uniform_real_distribution<double> dis;
    return dis(gen);
  }

  size_t utils_t::rand_u64() {
    static std::mt19937 gen(seed);
    static std::uniform_int_distribution<size_t> dis;
    return dis(gen);
  }

  size_t utils_t::position(size_t position, size_t direction) {
    direction = direction % 8;
    LOG_GENESIS(DEBUG, "position: %zd", position);
    LOG_GENESIS(DEBUG, "direction: %zd", direction);
    auto [x, y] = position_to_xy(position);
    ;
    switch (direction) {
      case 0: x--; y--; LOG_GENESIS(DEBUG, "LU"); break;
      case 1:      y--; LOG_GENESIS(DEBUG, " U"); break;
      case 2: x++; y--; LOG_GENESIS(DEBUG, "RU"); break;
      case 3: x--;    ; LOG_GENESIS(DEBUG, "L "); break;
      case 4: x++;    ; LOG_GENESIS(DEBUG, "R "); break;
      case 5: x--; y++; LOG_GENESIS(DEBUG, "LD"); break;
      case 6:      y++; LOG_GENESIS(DEBUG, " D"); break;
      case 7: x++; y++; LOG_GENESIS(DEBUG, "RD"); break;
      default: break;
    }
    size_t position_new = position_from_xy(x, y);
    if (position_new != npos)
      position = position_new;
    LOG_GENESIS(DEBUG, "position new: %zd", position);
    return position;
  }

  std::pair<size_t, size_t> utils_t::position_to_xy(size_t position) {
    if (position > parameters.position_max)
      return {npos, npos};
    size_t x = position % parameters.position_n;
    size_t y = position / parameters.position_n;
    return {x, y};
  }

  size_t utils_t::position_from_xy(size_t x, size_t y) {
    if (x >= n() || y >= m()) {
      return npos;
    }
    return y * parameters.position_n + x;
  }

  size_t utils_t::n() {
    return parameters.position_n;
  }

  size_t utils_t::m() {
    return parameters.position_max / parameters.position_n;
  }

  void utils_t::update_parameters(const std::string& name) {
    TRACE_GENESIS;
    nlohmann::json json;
    if (!utils_t::load(json, name)) {
      LOG_GENESIS(ERROR, "%s: can not load file", name.c_str());
      return;
    }
    parameters.load(json);
    json = {};
    parameters.save(json);
    utils_t::save(json, name);
  }

  void utils_t::rename(const std::string& name_old, const std::string& name_new) {
    TRACE_GENESIS;
    std::error_code ec;
    std::filesystem::rename(name_old, name_new, ec);
    if (ec) {
      LOG_GENESIS(ERROR, "%s", ec.message().c_str());
    }
  }

  void utils_t::remove(const std::string& name) {
    TRACE_GENESIS;
    std::error_code ec;
    std::filesystem::remove(name, ec);
    if (ec) {
      LOG_GENESIS(ERROR, "%s", ec.message().c_str());
    }
  }

  bool utils_t::load(nlohmann::json& json, const std::string& name) {
    TRACE_GENESIS;
    try {
      std::ifstream file(name);
      file >> json;
      return true;
    } catch (const std::exception& e) {
      LOG_GENESIS(ERROR, "%s: %s", name.c_str(), e.what());
    }

    std::string name_tmp = name + ".tmp";
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
    std::string name_tmp = name + ".tmp";
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

  ////////////////////////////////////////////////////////////////////////////////

  void bot_t::update_parameters() {
    TRACE_GENESIS;
    while (code.size() < utils_t::parameters.bot_code_size) {
      code.push_back(utils_t::rand_u64() % 0xFF);
    }
    code.resize(utils_t::parameters.bot_code_size);

    while (regs.size() < utils_t::parameters.bot_regs_size) {
      regs.push_back(utils_t::rand_u64() % 0xFF);
    }
    regs.resize(utils_t::parameters.bot_regs_size);

    while (interrupts.size() < utils_t::parameters.bot_interrupts_size) {
      interrupts.push_back(utils_t::rand_u64() % 0xFF);
    }
    interrupts.resize(utils_t::parameters.bot_interrupts_size);

    mineral        = std::min(mineral,      utils_t::parameters.bot_energy_max);
    sunlight       = std::min(sunlight,     utils_t::parameters.bot_energy_max);
    energy         = std::min(energy,       utils_t::parameters.bot_energy_max);
    energy_daily   = std::min(energy_daily, utils_t::parameters.bot_energy_daily);
  }

  bool bot_t::load(nlohmann::json& json) {
    TRACE_GENESIS;
    if (!json.is_object()) {
      LOG_GENESIS(ERROR, "json is not object");
      return false;
    }

    JSON_LOAD(json, code);
    JSON_LOAD(json, regs);
    JSON_LOAD(json, interrupts);
    JSON_LOAD(json, rip);
    JSON_LOAD(json, mineral);
    JSON_LOAD(json, sunlight);
    JSON_LOAD(json, energy);
    JSON_LOAD(json, energy_daily);
    JSON_LOAD(json, position);
    JSON_LOAD(json, age);
    JSON_LOAD(json, name);

    return true;
  }

  void bot_t::save(nlohmann::json& json) {
    TRACE_GENESIS;

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
    JSON_SAVE(json, age);
    JSON_SAVE(json, name);
  }

  ////////////////////////////////////////////////////////////////////////////////

  world_t::world_t() {
    systems.push_back(std::make_shared<system_time_t>());
    systems.push_back(std::make_shared<system_bot_stats_t>());
    systems.push_back(std::make_shared<system_bot_generator_t>());
    systems.push_back(std::make_shared<system_bot_updater_t>());
    systems.push_back(std::make_shared<system_epoll_t>());
  }

  void world_t::update() {
    TRACE_GENESIS;
    for (auto& system : systems) {
      system->update(*this);
    }

    LOG_GENESIS(TIME, "time_ms: %zd", utils_t::parameters.time_ms);
    LOG_GENESIS(TIME, "time_save_words_ms:      %zd", time_save_words_ms);
    LOG_GENESIS(TIME, "time_load_parameters_ms: %zd", time_load_parameters_ms);
    LOG_GENESIS(TIME, "time_update_world_ms:    %zd", time_update_world_ms);

    if (time_save_words_ms < utils_t::parameters.time_ms) {
      LOG_GENESIS(TIME, "save");
      save();
      time_save_words_ms = utils_t::parameters.time_ms
        + utils_t::parameters.interval_save_world_ms;
    }

    if (time_load_parameters_ms < utils_t::parameters.time_ms) {
      LOG_GENESIS(TIME, "update");
      utils_t::update_parameters(utils_t::parameters.parameters_file);
      update_parameters();
      time_load_parameters_ms = utils_t::parameters.time_ms
        + utils_t::parameters.interval_load_parameters_ms;
    }

    if (time_update_world_ms > utils_t::parameters.time_ms) {
      size_t dt = time_update_world_ms - utils_t::parameters.time_ms;
      LOG_GENESIS(TIME, "dt: %zd / %zd", dt, utils_t::parameters.interval_update_world_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(dt));
      time_update_world_ms = time_update_world_ms
        + utils_t::parameters.interval_update_world_ms;
    } else {
      size_t dt = utils_t::parameters.time_ms- time_update_world_ms;
      LOG_GENESIS(TIME, ">> dt: %zd / %zd", dt, utils_t::parameters.interval_update_world_ms);
      time_update_world_ms = utils_t::parameters.time_ms
        + utils_t::parameters.interval_update_world_ms;
    }
  }


  void world_t::update_parameters() {
    TRACE_GENESIS;
    for (auto& system : systems) {
      system->update_parameters();
    }
    bots.resize(utils_t::parameters.position_max);
    for (auto& bot : bots) {
      if (bot)
        bot->update_parameters();
    }
  }

  void world_t::load() {
    TRACE_GENESIS;
    nlohmann::json json;
    utils_t::load(json, utils_t::parameters.world_file);

    auto& bots_json = json["bots"];
    if (bots_json.is_array()) {
      bots.resize(utils_t::parameters.position_max);
      for (size_t i{}; i < bots_json.size(); ++i) {
        if (bot_t bot; bot.load(bots_json[i]) && bot.position < bots.size()) {
          bots[bot.position] = std::make_shared<bot_t>(bot);
        }
      }
    }
  }

  void world_t::save() {
    TRACE_GENESIS;
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

    utils_t::save(json, utils_t::parameters.world_file);
  }

  ////////////////////////////////////////////////////////////////////////////////

  void system_bot_stats_t::update(world_t& world) {
    TRACE_GENESIS;

    if (time_update_stats_ms >= utils_t::parameters.time_ms) {
      return;
    }

    LOG_GENESIS(STATS, "time_update_stats_ms: %zd", time_update_stats_ms);

    time_update_stats_ms = utils_t::parameters.time_ms
      + utils_t::parameters.system_interval_stats;

    auto& stats = world.stats;
    stats.bots_alive = 0;
    stats.age_max = 0;
    stats.age_avg = 0;
    stats.energy = 0;
    stats.energy_avg = 0;

    for (auto& bot : world.bots) {
      if (!bot)
        continue;

      stats.bots_alive++;
      stats.age_max = std::max(bot->age, stats.age_max);
      stats.age_avg += bot->age;
      stats.energy += bot->energy;
    }

    stats.age_avg = 1. * stats.age_max / std::max(stats.bots_alive, 1UL);
    stats.energy_avg = 1. * stats.energy / std::max(stats.bots_alive, 1UL);

    LOG_GENESIS(STATS, "stats.bots_alive: %zd", stats.bots_alive);
    LOG_GENESIS(STATS, "stats.age_max:    %zd", stats.age_max);
    LOG_GENESIS(STATS, "stats.age_avg:    %zd", (size_t) stats.age_avg);
    LOG_GENESIS(STATS, "stats.energy:     %zd", stats.energy);
    LOG_GENESIS(STATS, "stats.energy_avg: %zd", (size_t) stats.energy_avg);
  }

  ////////////////////////////////////////////////////////////////////////////////

  void system_bot_generator_t::update(world_t& world) {
    TRACE_GENESIS;

    if (world.stats.bots_alive < utils_t::parameters.system_min_bot_count) {
      /*for (size_t i{} ; i < utils_t::parameters.system_min_bot_count - world.stats.bots_alive; ++i)*/ {
        bot_t bot_new;
        bot_new.update_parameters();
        if (bot_new.position < world.bots.size() && !world.bots[bot_new.position]) {
          world.bots[bot_new.position] = std::make_shared<bot_t>(bot_new);
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  void system_bot_updater_t::update(world_t& world) {
    TRACE_GENESIS;

    for (auto bot : world.bots) { // copy
      if (!bot)
        continue;

      LOG_GENESIS(MIND, "");
      LOG_GENESIS(MIND, "bot:          %p",  bot.get());
      LOG_GENESIS(MIND, "mineral:      %zd", bot->mineral);
      LOG_GENESIS(MIND, "sunlight:     %zd", bot->sunlight);
      LOG_GENESIS(MIND, "energy:       %zd", bot->energy);
      LOG_GENESIS(MIND, "position:     %zd", bot->position);
      LOG_GENESIS(MIND, "rip:          %zd", bot->rip);
      LOG_GENESIS(MIND, "age:          %zd", bot->age);
      LOG_GENESIS(MIND, "energy_daily: %zd", bot->energy_daily);

      if (!bot->energy_daily)
        continue;

      // TODO for (x : bot->energy_daily)
      if (!bot->energy || bot->energy < bot->energy_daily) {
        if (bot->position < world.bots.size()) {
          world.bots[bot->position].reset();
        }
        LOG_GENESIS(MIND, "DEAD");
        continue;
      }

      bot->energy -= bot->energy_daily;

      // TODO interrupts
      {
        // * сосед рядом: <%0> = <направление>
        // * враг рядом: <%1> = <направление>
        // * атакован: <%2> = <направление>
        // * эненргия кончается
        // ...
      }

      bot->rip = bot->rip % bot->code.size();
      size_t cmd = bot->code[(bot->rip++) % bot->code.size()];
      LOG_GENESIS(MIND, "cmd: %zd", (size_t) cmd);
      switch (cmd) {
        // RISC instructions:
        case 0: {
          LOG_GENESIS(MIND, "NOP");
          break;
        } case 1: {
          LOG_GENESIS(MIND, "RET");
          bot->rip = 0;
          break;
        } case 2: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "BR <%%%d>", arg1);
          bot->rip += bot->regs[arg1];
          break;
        } case 3: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()];
          LOG_GENESIS(MIND, "SET <%%%d> <%d>", arg1, arg2);
          bot->regs[arg1] = arg2;
          break;
        } case 4: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "MOV <%%%d> = <%%%d>", arg1, arg2);
          bot->regs[arg1] = bot->regs[arg2];
          break;
        } case 5: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "ADD <%%%d> <%%%d> <%%%d>", arg1, arg2, arg3);
          bot->regs[arg1] = bot->regs[arg2] + bot->regs[arg3];
          break;
        } case 6: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "SUB <%%%d> <%%%d> <%%%d>", arg1, arg2, arg3);
          bot->regs[arg1] = bot->regs[arg2] - bot->regs[arg3];
          break;
        } case 7: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "MULT <%%%d> <%%%d> <%%%d>", arg1, arg2, arg3);
          bot->regs[arg1] = bot->regs[arg2] * bot->regs[arg3];
          break;
        } case 8: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "DIV <%%%d> <%%%d> <%%%d>", arg1, arg2, arg3);
          bot->regs[arg1] = bot->regs[arg2] / std::max(uint8_t(1), bot->regs[arg3]);
          break;
        } case 9: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "IF > <%%%d> <%%%d> <%%%d>", arg1, arg2, arg3);
          if (bot->regs[arg1] > bot->regs[arg2])
            bot->rip += bot->regs[arg3];
          break;
        } case 10: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "IF < <%%%d> <%%%d> <%%%d>", arg1, arg2, arg3);
          if (bot->regs[arg1] < bot->regs[arg2])
            bot->rip += bot->regs[arg3];
          break;

          // Bot instructions:
        } case 32: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "MOVE <%%%d>", arg1);
          size_t position = utils_t::position(bot->position, arg1);
          size_t energy_cost = 3;
          if (position < world.bots.size() && !world.bots[position] && bot->energy > energy_cost) {
            world.bots[position] = bot;
            world.bots[bot->position].reset();
            bot->position = position;
            bot->energy -= energy_cost;
          }
          break;
        } case 33: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "ATTACK <%%%d>", arg1);
          size_t position = utils_t::position(bot->position, arg1);
          size_t strength = utils_t::rand_u64() % 30;
          size_t energy_cost = 10;
          if (position < world.bots.size() && world.bots[position] && bot->energy > energy_cost) {
            auto& bot_attacked = world.bots[position];
            if (bot_attacked->energy < strength) {
              bot->energy   = std::min(bot->energy   + bot_attacked->energy,   utils_t::parameters.bot_energy_max);
              bot->mineral  = std::min(bot->mineral  + bot_attacked->mineral,  utils_t::parameters.bot_energy_max);
              bot->sunlight = std::min(bot->sunlight + bot_attacked->sunlight, utils_t::parameters.bot_energy_max);
              bot_attacked.reset();
            } else {
              bot->energy   = std::min(bot->energy + strength, utils_t::parameters.bot_energy_max);
              bot_attacked->energy -= strength;
            }
            bot->energy -= energy_cost;
          }
          break;
        } case 34: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "GET MINERAL <%%%d>", arg1);
          bot->regs[arg1] = bot->mineral % 0xFF;
          break;
        } case 35: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "GET SUNLIGHT <%%%d>", arg1);
          bot->regs[arg1] = bot->sunlight % 0xFF;
          break;
        } case 36: {
          uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
          LOG_GENESIS(MIND, "GET ENERGY <%%%d>", arg1);
          bot->regs[arg1] = bot->energy % 0xFF;
          break;
        } case 37: {
          LOG_GENESIS(MIND, "EXTRACT MINERAL");
          size_t energy_cost = 5;
          if (bot->energy > energy_cost) {
            bot->mineral = utils_t::parameters.bot_energy_max;
            bot->energy -= energy_cost;
          }
          break;
        } case 38: {
          LOG_GENESIS(MIND, "EXTRACT SUNLIGHT");
          size_t energy_cost = 5;
          if (bot->energy > energy_cost) {
            bot->sunlight = utils_t::parameters.bot_energy_max;
            bot->energy -= energy_cost;
          }
          break;
        } case 39: {
          LOG_GENESIS(MIND, "CONVERT MINERAL");
          size_t energy_cost = 5;
          if (bot->energy > energy_cost) {
            bot->energy = (bot->energy + bot->mineral) % utils_t::parameters.bot_energy_max;
            bot->mineral = 0;
            bot->energy -= energy_cost;
          }
          break;
        } case 40: {
          LOG_GENESIS(MIND, "CONVERT SUNLIGHT");
          size_t energy_cost = 5;
          if (bot->energy > energy_cost) {
            bot->energy = (bot->energy + bot->sunlight) % utils_t::parameters.bot_energy_max;
            bot->sunlight = 0;
            bot->energy -= energy_cost;
          }
          break;
        } case 41: {
          LOG_GENESIS(MIND, "CLONE");
          size_t direction = utils_t::rand_u64() % 8;
          size_t position = utils_t::position(bot->position, direction);
          size_t energy_cost = 10 + utils_t::parameters.bot_energy_max / 4;
          if (bot->energy > energy_cost && position < world.bots.size() && !world.bots[position]) {
            auto bot_child = std::make_shared<bot_t>();
            bot_child->code         = bot->code;
            bot_child->regs         = bot->regs;
            bot_child->interrupts   = bot->interrupts;
            bot_child->name         = bot->name;
            bot_child->position     = position;

            bot->energy -= 10;

            bot_child->energy       = bot->energy;   bot_child->energy   /= 2; bot->energy   /= 2;
            bot_child->mineral      = bot->mineral;  bot_child->mineral  /= 2; bot->mineral  /= 2;
            bot_child->sunlight     = bot->sunlight; bot_child->sunlight /= 2; bot->sunlight /= 2;

            { // mutation
              bool mutation_code       = (0 == utils_t::rand_u64() % 20);
              bool mutation_regs       = (0 == utils_t::rand_u64() % 20);
              bool mutation_interrupts = (0 == utils_t::rand_u64() % 20);
              if (mutation_code) {
                size_t ind = utils_t::rand_u64() % bot_child->code.size();
                size_t val = utils_t::rand_u64() % 0xFF;
                bot_child->code[ind] = val;
              }
              if (mutation_regs) {
                size_t ind = utils_t::rand_u64() % bot_child->regs.size();
                size_t val = utils_t::rand_u64() % 0xFF;
                bot_child->regs[ind] = val;
              }
              if (mutation_interrupts) {
                size_t ind = utils_t::rand_u64() % bot_child->interrupts.size();
                size_t val = utils_t::rand_u64() % 0xFF;
                bot_child->interrupts[ind] = val;
              }
              if (mutation_code || mutation_regs || mutation_regs)
                bot_child->name = "r" + std::to_string(utils_t::rand_u64());
            }

            world.bots[position] = bot_child;

          }
          break;
        } default: {
          /*NOTHING*/
          break;
        }
      }
      bot->age++;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  void system_epoll_t::update_parameters() {
    TRACE_GENESIS;

    if (is_run) // TODO reinit
      return;

    is_run = false;

    if (fd_epoll = epoll_create1(0); fd_epoll < 0) {
      LOG_GENESIS(ERROR, "fd_epoll: %d", fd_epoll);
      close_all();
      return;
    }

    if (fd_listen = socket(AF_INET, SOCK_STREAM, 0); fd_listen < 0) {
      LOG_GENESIS(ERROR, "fd_listen: %d", fd_listen);
      close_all();
      return;
    }

    int on = 1;
    if (int ret = setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); ret < 0) {
      LOG_GENESIS(ERROR, "setsockopt(...): %d", ret);
      close_all();
      return;
    }

    in_addr ip_addr = {0};
    if (int ret = inet_pton(AF_INET, utils_t::parameters.system_epoll_ip.c_str(), &ip_addr); ret < 0) {
      LOG_GENESIS(ERROR, "inet_pton(...): %d", ret);
      close_all();
      return;
    }

    sockaddr_in addr;
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ip_addr.s_addr;
    addr.sin_port        = htons((uint16_t) utils_t::parameters.system_epoll_port);

    if (int ret = bind(fd_listen, (sockaddr *) &addr, sizeof(addr)); ret < 0) {
      LOG_GENESIS(ERROR, "bind(...): %d", ret);
      close_all();
      return;
    }

    if (set_nonblock(fd_listen) < 0) {
      close_all();
      return;
    }

    if (int ret = listen(fd_listen, SOMAXCONN); ret < 0) {
      LOG_GENESIS(ERROR, "listen(...): %d", ret);
      close_all();
      return;
    }

    if (set_epoll_ctl(fd_listen, EPOLLIN, EPOLL_CTL_ADD) < 0) {
      close_all();
      return;
    }

    is_run = true;
  }

  void system_epoll_t::update(world_t& world) {
    TRACE_GENESIS;

    if (!is_run) {
      update_parameters();
      return;
    }

    int event_count = epoll_wait(fd_epoll, events, events_count, 0);

    for (int i = 0; i < event_count; i++) {
      epoll_event& event = events[i];
      LOG_GENESIS(EPOLL, "events: 0x%x", event.events);
      LOG_GENESIS(EPOLL, "data.fd: %d", event.data.fd);

      if ((event.events & EPOLLERR) ||
          (event.events & EPOLLHUP) ||
          (!(event.events & (EPOLLIN | EPOLLOUT))))
      {
        LOG_GENESIS(EPOLL, "epoll error");
        close(event.data.fd);
        continue;
      }

      if (event.data.fd == fd_listen) {
        LOG_GENESIS(EPOLL, "new connection");

        int fd = accept(fd_listen , 0, 0);
        LOG_GENESIS(EPOLL, "fd: %d", fd);
        if (fd < 0) {
          LOG_GENESIS(ERROR, "fd: %d", fd);
          close_all();
        }

        if (set_nonblock(fd) < 0) {
          close_all();
        }

        if (set_epoll_ctl(fd, EPOLLIN, EPOLL_CTL_ADD) < 0) {
          close_all();
        }

        continue;
      }

      if (event.events & EPOLLIN) {
        process_read(event.data.fd);
      } else {
        process_write(event.data.fd);
      }
    }
  }

  std::string system_epoll_t::get_response(const std::string ct, const std::string& body) {
    std::stringstream ss;
    ss << "HTTP/1.1 200 OK" << NL
      << "Content-Type: " << ct << NL
      << "Content-Length: " << body.size() << NL
      << "Connection: keep-alive" << NL << NL
      << body;

    return ss.str();
  }

  void system_epoll_t::close_all() {
    TRACE_GENESIS;
    close(fd_listen);
    close(fd_epoll);
    for (auto& kv : buffers) {
      close(kv.first);
    }
    buffers.clear();
    is_run = false;
  }

  int system_epoll_t::set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
      flags = 0;
    }

    int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (ret < 0) {
      LOG_GENESIS(ERROR, "fcntl(...): %d", ret);
    }
    return ret;
  }

  int system_epoll_t::set_epoll_ctl(int fd, int events, int op) {
    epoll_event event;
    event.data.fd = fd;
    event.events = events;
    int ret = epoll_ctl(fd_epoll, op, fd, &event);
    if (ret < 0) {
      LOG_GENESIS(ERROR, "epoll_ctl(...): %d", ret);
    }
    return ret;
  }

  void system_epoll_t::process_write(int fd) {
    TRACE_GENESIS;
    auto& buffer = buffers[fd];
    auto& buffer_resp = buffer.second;

    int bytes_write = std::min((int) sizeof(buffer_tmp),
        (int) std::distance(buffer_resp.begin(), buffer_resp.end()));

    std::copy(buffer_resp.begin(), buffer_resp.begin() + bytes_write, std::begin(buffer_tmp));

    bytes_write = write(fd, buffer_tmp, bytes_write);
    buffer_resp.erase(buffer_resp.begin(), buffer_resp.begin() + bytes_write);
    LOG_GENESIS(EPOLL, "write: %d", bytes_write);

    if (buffer_resp.empty()) {
      if (set_epoll_ctl(fd, EPOLLIN, EPOLL_CTL_MOD) < 0) {
        close_all();
      }
    }
  }

  void system_epoll_t::process_read(int fd) {
    TRACE_GENESIS;
    int bytes_read = read(fd, buffer_tmp, sizeof(buffer_tmp));
    LOG_GENESIS(EPOLL, "read: fd: %d, bytes_read: %d", fd, bytes_read);
    if (bytes_read == -1) {
      if (errno != EAGAIN) {
        LOG_GENESIS(EPOLL, "errno: !EAGAIN");
        close(fd);
      }
    } else if (bytes_read == 0) {
      LOG_GENESIS(EPOLL, "bytes_read == 0");
      shutdown(fd, SHUT_RDWR);
      close(fd);
      buffers.erase(fd);
    } else {
      auto& buffer = buffers[fd];
      auto& buffer_req = buffer.first;
      auto& buffer_resp = buffer.second;

      buffer_req.insert(buffer_req.end(), std::begin(buffer_tmp),
          std::begin(buffer_tmp) + bytes_read);

      // auto tmp = http_parser_t::method();
      // auto tmp = http_parser_t::params();

      auto it = std::search(buffer_req.begin(), buffer_req.end(), NL2.begin(), NL2.end());
      if (it != buffer_req.end()) {
        // GET found
        it += NL2.size();
        LOG_GENESIS(EPOLL, "msg: '%s'", std::string(buffer_req.begin(), it).c_str());
        buffer_req.erase(buffer_req.begin(), it);

        std::string response = "<h1>amyasnikov: genesis</h1>";

        response = get_response(ct_html, response);

        buffer_resp.insert(buffer_resp.end(), response.begin(), response.end());

        if (set_epoll_ctl(fd, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD) < 0) {
          close_all();
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  void system_time_t::update(world_t& world) {
    size_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    utils_t::parameters.time_ms = time_ms;
  }

  ////////////////////////////////////////////////////////////////////////////////
}



int main(int argc, char* argv[]) {
  std::string parameters_file = "parameters.json";

  if (argc > 1) {
    parameters_file = argv[1];
  }

  using namespace genesis_n;

  utils_t::update_parameters(parameters_file);

  world_t world;
  world.load();

  world.update_parameters();

  while (true) {
    world.update();
  }

  return 0;
}

