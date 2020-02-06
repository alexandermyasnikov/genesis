
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

#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>


#include "json.hpp"
#include "debug_logger.h"

#define JSON_SAVE(json, name) json[#name] = name
#define JSON_LOAD(json, name) name = json.value(#name, name)

#define TRACE_TEST                 // DEBUG_LOGGER("test ", logger_indent_test_t::indent)
#define LOG_TEST(...)              // DEBUG_LOG("test ", logger_indent_test_t::indent, __VA_ARGS__)
#define LOG_EPOLL(...)             DEBUG_LOG("epoll", logger_indent_test_t::indent, __VA_ARGS__)

using namespace std::chrono_literals;

struct logger_indent_test_t   : logger_indent_t<logger_indent_test_t> { };



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
    size_t   system_epoll_port        = 8282;
    std::string   system_epoll_ip     = "127.0.0.1";
    std::string   world_file          = "world.json";
    std::string   parameters_file     = "parameters.json";
    size_t   time_ms                  = 0;
    size_t   interval_save_world_ms   = 60 * 1000;
    size_t   interval_load_parameters_ms   = 60 * 1000;

    void load(nlohmann::json& json) {
      TRACE_TEST;
      if (!json.is_object())
        return;

      JSON_LOAD(json, position_n);
      JSON_LOAD(json, position_max);
      JSON_LOAD(json, bot_code_size);
      JSON_LOAD(json, bot_regs_size);
      JSON_LOAD(json, bot_interrupts_size);
      JSON_LOAD(json, bot_energy_max);
      JSON_LOAD(json, bot_energy_daily);
      JSON_LOAD(json, system_min_bot_count);
      JSON_LOAD(json, system_epoll_port);
      JSON_LOAD(json, system_epoll_ip);
      JSON_LOAD(json, world_file);
      JSON_LOAD(json, parameters_file);
      // JSON_LOAD(json, time_ms);
      JSON_LOAD(json, interval_save_world_ms);
      JSON_LOAD(json, interval_load_parameters_ms);

      // correct: position_max % position_n == 0
      position_max = (position_max / position_n) * position_n;
    }

    void save(nlohmann::json& json) {
      TRACE_TEST;
      if (!json.is_object())
        json = nlohmann::json::object();

      JSON_SAVE(json, position_n);
      JSON_SAVE(json, position_max);
      JSON_SAVE(json, bot_code_size);
      JSON_SAVE(json, bot_regs_size);
      JSON_SAVE(json, bot_interrupts_size);
      JSON_SAVE(json, bot_energy_max);
      JSON_SAVE(json, bot_energy_daily);
      JSON_SAVE(json, system_min_bot_count);
      JSON_SAVE(json, system_epoll_port);
      JSON_SAVE(json, system_epoll_ip);
      JSON_SAVE(json, world_file);
      JSON_SAVE(json, parameters_file);
      JSON_SAVE(json, time_ms);
      JSON_SAVE(json, interval_save_world_ms);
      JSON_SAVE(json, interval_load_parameters_ms);
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

    static void rename(const std::string& name_old, const std::string& name_new) {
      TRACE_TEST;
      std::error_code ec;
      std::filesystem::rename(name_old, name_new, ec);
      if (ec) {
        std::cerr << "WARN: " << ec.message() << std::endl;
      }
    }

    static void remove(const std::string& name) {
      TRACE_TEST;
      std::error_code ec;
      std::filesystem::remove(name, ec);
      if (ec) {
        std::cerr << "WARN: " << ec.message() << std::endl;
      }
    }

    static bool load(nlohmann::json& json, const std::string& name) {
      TRACE_TEST;
      try {
        std::ifstream file(name);
        file >> json;
        return true;
      } catch (const std::exception& e) {
        std::cerr << "WARN: " << e.what() << std::endl;
      }

      std::string name_tmp = name + ".tmp";
      try {
        std::ifstream file(name_tmp);
        file >> json;
        return true;
      } catch (const std::exception& e) {
        std::cerr << "WARN: " << e.what() << std::endl;
      }
      utils_t::remove(name_tmp);
      return false;
    }

    static bool save(const nlohmann::json& json, const std::string& name) {
      TRACE_TEST;
      std::string name_tmp = name + ".tmp";
      try {
        std::ofstream file(name_tmp);
        file << std::setw(2) << json;

        utils_t::rename(name_tmp, name);
        utils_t::remove(name_tmp);
        return true;
      } catch (const std::exception& e) {
        std::cerr << "WARN: " << e.what() << std::endl;
        return false;
      }
    }
  };

  struct http_parser_t {
    ;
  };

  struct stats_t {
    size_t   bots_alive;

    stats_t() :
      bots_alive(0)
    { }
  };

  struct bot_t {
    std::vector<uint8_t>   code;
    std::vector<uint8_t>   regs;            // TODO uint32_t
    std::vector<uint8_t>   interrupts;
    size_t                 rip              = 0;
    size_t                 mineral          = utils_t::parameters.bot_energy_max;
    size_t                 sunlight         = utils_t::parameters.bot_energy_max;
    size_t                 energy           = utils_t::parameters.bot_energy_max;
    size_t                 energy_daily     = utils_t::parameters.bot_energy_daily;
    size_t                 position         = utils_t::rand_u64() % (utils_t::parameters.position_max);
    size_t                 age              = 0;
    std::string            name             = "r" + std::to_string(utils_t::rand_u64());

    void update_parameters() {
      TRACE_TEST;
      while (code.size() < utils_t::parameters.bot_code_size) {
        code.push_back(utils_t::rand_u64() % 0xFF);
      }
      code.resize(utils_t::parameters.bot_code_size);

      while (regs.size() < utils_t::parameters.bot_code_size) {
        regs.push_back(utils_t::rand_u64() % 0xFF);
      }
      regs.resize(utils_t::parameters.bot_code_size);

      while (interrupts.size() < utils_t::parameters.bot_code_size) {
        interrupts.push_back(utils_t::rand_u64() % 0xFF);
      }
      interrupts.resize(utils_t::parameters.bot_code_size);

      mineral        = std::min(mineral, utils_t::parameters.bot_energy_max);
      sunlight       = std::min(mineral, utils_t::parameters.bot_energy_max);
      energy         = std::min(mineral, utils_t::parameters.bot_energy_max);
      energy_daily   = std::min(mineral, utils_t::parameters.bot_energy_daily);
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
      JSON_LOAD(json, age);
      JSON_LOAD(json, name);

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
      JSON_SAVE(json, age);
      JSON_SAVE(json, name);
    }
  };

  struct world_t {
    stats_t                      stats;
    std::vector<bot_sptr_t>      bots;
    std::vector<system_sptr_t>   systems;
    size_t                       last_time_save_words_ms = 0;
    size_t                       last_time_load_parameters_ms = 0;

    world_t();

    void update();

    void update_parameters();

    void load() {
      TRACE_TEST;
      nlohmann::json json;
      utils_t::load(json, utils_t::parameters.world_file);

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

      last_time_save_words_ms = utils_t::parameters.time_ms;
    }

    void save() {
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

      utils_t::save(json, utils_t::parameters.world_file);
    }
  };

  struct system_t {
    virtual void update_parameters() { }
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

      for (auto& bot : world.bots) {
        if (!bot)
          continue;

        LOG_TEST("");
        LOG_TEST("bot:      %p", bot.get());
        LOG_TEST("mineral:  %zd", bot->mineral);
        LOG_TEST("sunlight: %zd", bot->sunlight);
        LOG_TEST("energy:   %zd", bot->energy);
        LOG_TEST("position: %zd", bot->position);
        LOG_TEST("rip:      %zd", bot->rip);

        // TODO for (x : bot->energy_daily)
        if (bot->energy < bot->energy_daily) {
          bot.reset();
          LOG_TEST("DEAD");
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
        LOG_TEST("cmd: %zd", (size_t) cmd);
        switch (cmd) {
          // RISC instructions:
          case 0: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()];
            LOG_TEST("BR <%d>", arg1);
            bot->rip += arg1;
            break;
          } case 1: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()];
            LOG_TEST("SET <%%%d> <%d>", arg1, arg2);
            bot->regs[arg1] = arg2;
            break;
          } case 2: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            LOG_TEST("MOV <%%%d> = <%%%d>", arg1, arg2);
            bot->regs[arg1] = bot->regs[arg2];
            break;
          } case 3: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()];
            uint8_t arg4 = bot->code[(bot->rip++) % bot->code.size()];
            LOG_TEST("IF > <%%%d> <%%%d> <%d> <%d>", arg1, arg2, arg3, arg4);
            bot->rip += (bot->regs[arg1] > bot->regs[arg2]) ? arg3 : arg4;
            break;
          } case 4: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()];
            uint8_t arg4 = bot->code[(bot->rip++) % bot->code.size()];
            LOG_TEST("IF < <%%%d> <%%%d> <%d> <%d>", arg1, arg2, arg3, arg4);
            bot->rip += (bot->regs[arg1] < bot->regs[arg2]) ? arg3 : arg4;
            break;
          } case 5: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            uint8_t arg2 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            uint8_t arg3 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            LOG_TEST("ADD <%%%d> <%%%d> <%%%d>", arg1, arg2, arg3);
            bot->regs[arg1] = bot->regs[arg2] + bot->regs[arg3];
            break;

            // Bot instructions:
          } case 32: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % 9;
            LOG_TEST("TODO MOVE <%d>", arg1);
            break;
          } case 33: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % 9;
            LOG_TEST("TODO ATTACK <%d>", arg1);
            break;
          } case 34: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            LOG_TEST("GET MINERAL <%%%d>", arg1);
            bot->regs[arg1] = bot->mineral % 0xFF;
            break;
          } case 35: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            LOG_TEST("GET SUNLIGHT <%%%d>", arg1);
            bot->regs[arg1] = bot->sunlight % 0xFF;
            break;
          } case 36: {
            uint8_t arg1 = bot->code[(bot->rip++) % bot->code.size()] % bot->regs.size();
            LOG_TEST("GET ENERGY <%%%d>", arg1);
            bot->regs[arg1] = bot->energy % 0xFF;
            break;
          } case 37: {
            LOG_TEST("EXTRACT MINERAL");
            bot->mineral = utils_t::parameters.bot_energy_max;
            break;
          } case 38: {
            LOG_TEST("EXTRACT SUNLIGHT");
            bot->sunlight = utils_t::parameters.bot_energy_max;
            break;
          } case 39: {
            LOG_TEST("CONVERT MINERAL");
            bot->energy = (bot->energy + bot->mineral) % utils_t::parameters.bot_energy_max;
            bot->mineral = 0;
            break;
          } case 40: {
            LOG_TEST("CONVERT SUNLIGHT");
            bot->energy = (bot->energy + bot->sunlight) % utils_t::parameters.bot_energy_max;
            bot->sunlight = 0;
            break;
          } case 41: {
            LOG_TEST("TODO CLONE");
            if (bot->energy > utils_t::parameters.bot_energy_max / 2) {
              ;
              // bot->energy -= utils_t::parameters.bot_energy_max / 2;
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

    void update_parameters() override {
      TRACE_TEST;
      is_run = false;

      if (fd_epoll = epoll_create1(0); fd_epoll < 0) {
        std::cerr << "WARN: fd_epoll: " << fd_epoll << std::endl;
        close_all();
      }

      if (fd_listen = socket(AF_INET, SOCK_STREAM, 0); fd_listen < 0) {
        std::cerr << "WARN: fd_listen: " << fd_listen << std::endl;
        close_all();
      }

      int on = 1;
      if (int ret = setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); ret < 0) {
        std::cerr << "WARN: setsockopt(...): " << ret << std::endl;
        close_all();
      }

      in_addr ip_addr = {0};
      if (int ret = inet_pton(AF_INET, utils_t::parameters.system_epoll_ip.c_str(), &ip_addr); ret < 0) {
        std::cerr << "WARN: inet_pton(...): " << ret << std::endl;
        close_all();
      }

      sockaddr_in addr;
      memset((char *) &addr, 0, sizeof(addr));
      addr.sin_family      = AF_INET;
      addr.sin_addr.s_addr = ip_addr.s_addr;
      addr.sin_port        = htons((uint16_t) utils_t::parameters.system_epoll_port);

      if (int ret = bind(fd_listen, (sockaddr *) &addr, sizeof(addr)); ret < 0) {
        std::cerr << "WARN: bind(...): " << ret << std::endl;
        close_all();
      }

      if (set_nonblock(fd_listen) < 0) {
        close_all();
      }

      if (int ret = listen(fd_listen, SOMAXCONN); ret < 0) {
        std::cerr << "WARN: listen(...): " << ret << std::endl;
        close_all();
      }

      if (set_epoll_ctl(fd_listen, EPOLLIN, EPOLL_CTL_ADD) < 0) {
        close_all();
      }

      is_run = true;
    }

    void update(world_t& world) override {
      TRACE_TEST;

      if (!is_run) {
        update_parameters();
        return;
      }

      int event_count = epoll_wait(fd_epoll, events, events_count, 0);

      for (int i = 0; i < event_count; i++) {
        epoll_event& event = events[i];
        LOG_EPOLL("events: 0x%x", event.events);
        LOG_EPOLL("data.fd: %d", event.data.fd);

        if ((event.events & EPOLLERR) ||
            (event.events & EPOLLHUP) ||
            (!(event.events & (EPOLLIN | EPOLLOUT))))
        {
          LOG_EPOLL("epoll error");
          close(event.data.fd);
          continue;
        }

        if (event.data.fd == fd_listen) {
          LOG_EPOLL("new connection");

          int fd = accept(fd_listen , 0, 0);
          LOG_EPOLL("fd: %d", fd);
          if (fd < 0) {
            std::cerr << "WARN: fd: " << fd << std::endl;
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

    std::string get_response(const std::string ct, const std::string& body) {
      std::stringstream ss;
      ss << "HTTP/1.1 200 OK" << NL
        << "Content-Type: " << ct << NL
        << "Content-Length: " << body.size() << NL
        << "Connection: keep-alive" << NL << NL
        << body;

      return ss.str();
    }

    void close_all() {
      std::cerr << "WARN: epoll: close_all()" << std::endl;
      for (auto& kv : buffers) {
        close(kv.first);
      }
      buffers.clear();
      is_run = false;
    }

    int set_nonblock(int fd) {
      int flags = fcntl(fd, F_GETFL, 0);
      if (flags < 0) {
        flags = 0;
      }

      int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
      if (ret < 0) {
        std::cerr << "WARN: fcntl(...): " << ret << std::endl;
      }
      return ret;
    }

    int set_epoll_ctl(int fd, int events, int op) {
      epoll_event event;
      event.data.fd = fd;
      event.events = events;
      int ret = epoll_ctl(fd_epoll, op, fd, &event);
      if (ret < 0) {
        std::cerr << "WARN: epoll_ctl(...): " << ret << std::endl;
      }
      return ret;
    }

    void process_write(int fd) {
      auto& buffer = buffers[fd];
      auto& buffer_resp = buffer.second;

      int bytes_write = std::min((int) sizeof(buffer_tmp),
          (int) std::distance(buffer_resp.begin(), buffer_resp.end()));

      std::copy(buffer_resp.begin(), buffer_resp.begin() + bytes_write, std::begin(buffer_tmp));

      bytes_write = write(fd, buffer_tmp, bytes_write);
      buffer_resp.erase(buffer_resp.begin(), buffer_resp.begin() + bytes_write);
      LOG_EPOLL("write: %d", bytes_write);

      if (buffer_resp.empty()) {
        if (set_epoll_ctl(fd, EPOLLIN, EPOLL_CTL_MOD) < 0) {
          close_all();
        }
      }
    }

    void process_read(int fd) {
      int bytes_read = read(fd, buffer_tmp, sizeof(buffer_tmp));
      LOG_EPOLL("read: fd: %d, bytes_read: %d", fd, bytes_read);
      if (bytes_read == -1) {
        if (errno != EAGAIN) {
          LOG_EPOLL("errno: !EAGAIN");
          close(fd);
        }
      } else if (bytes_read == 0) {
        LOG_EPOLL("bytes_read == 0");
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
          LOG_EPOLL("msg: '%s'", std::string(buffer_req.begin(), it).c_str());
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
  };

  struct system_time_t : system_t {
    void update_parameters() override {
      size_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      utils_t::parameters.time_ms = time_ms;
    }

    void update(world_t& world) override {
      size_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      utils_t::parameters.time_ms = time_ms;
    }
  };



  world_t::world_t() {
    systems.push_back(std::make_shared<system_time_t>());
    systems.push_back(std::make_shared<system_bot_stats_t>());
    systems.push_back(std::make_shared<system_bot_generator_t>());
    systems.push_back(std::make_shared<system_bot_updater_t>());
    systems.push_back(std::make_shared<system_epoll_t>());
  }

  void world_t::update() {
    TRACE_TEST;
    for (auto& system : systems) {
      system->update(*this);
    }

    if (utils_t::parameters.interval_save_world_ms
        + last_time_save_words_ms < utils_t::parameters.time_ms)
    {
      save();
      last_time_save_words_ms = utils_t::parameters.time_ms;
    }

    if (utils_t::parameters.interval_load_parameters_ms
        + last_time_load_parameters_ms < utils_t::parameters.time_ms)
    {
      utils_t::update_parameters(utils_t::parameters.parameters_file);
      last_time_load_parameters_ms = utils_t::parameters.time_ms;
    }
  }


  void world_t::update_parameters() {
    TRACE_TEST;
    for (auto& system : systems) {
      system->update_parameters();
    }
  }
}



int main(int argc, char* argv[]) {
  TRACE_TEST;

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

