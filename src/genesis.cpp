
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

#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

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

    inline static std::string TMP_SUFFIX       = ".tmp";
    inline static std::string TRACE            = "trace";
    inline static std::string ARGS             = "args ";
    inline static std::string EPOLL            = "epoll";
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
    inline static uint64_t npos                = std::string::npos;
    inline static std::string PRODUCER         = "producer";         // deprecated
    inline static std::string SPORE            = "spore";            // deprecated
    inline static std::string DEFENDER         = "defender";         // deprecated
    inline static std::string ENERGY           = "energy";           // deprecated
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
    inline static std::set<std::string> cell_types = { PRODUCER, SPORE, DEFENDER };

    static uint64_t n(const config_t& config);
    static uint64_t m(const config_t& config);
    static std::pair<uint64_t, uint64_t> position_to_xy(const config_t& config, uint64_t position);
    static uint64_t position_from_xy(const config_t& config, uint64_t x, uint64_t y);
    static uint64_t distance(const config_t& config, uint64_t position, uint64_t x, uint64_t y);
    static uint64_t position_next(const config_t& config, uint64_t position, const std::string& direction);
    static uint64_t position_next(const config_t& config, uint64_t position, uint64_t direction);

    static void rename(const std::string& name_old, const std::string& name_new);
    static void remove(const std::string& name);
    static bool load(nlohmann::json& json, const std::string& name);
    static bool save(const nlohmann::json& json, const std::string& name);
    static uint64_t rand_u64();
    static uint64_t hash_mix(uint64_t h);
    static uint64_t fasthash64(const void *buf, size_t len, uint64_t seed);

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

  struct resource_t {
    std::string   name    = {};
    uint64_t      count   = {};

    bool validation(const config_t& config);
  };

  struct bacteria_t {
    using resources_t   = std::map<std::string/*name*/, resource_t>;
    using code_t        = std::vector<uint8_t>;

    std::string   family             = {};
    code_t        code               = {};
    resources_t   resources          = {};
    uint64_t      pos                = utils_t::npos;
    uint64_t      age                = {};
    uint64_t      direction          = {};
    int64_t       energy_remaining   = {}; // not save

    bool validation(const config_t& config);
    void init(const config_t& config);
  };

  using bacteria_sptr_t = std::shared_ptr<bacteria_t>;

  struct resource_info_t {
    std::string   name          = {};
    uint64_t      stack_size    = utils_t::npos;
    bool          extractable   = false;

    bool validation(const config_t& config);
  };

  struct area_t {
    // Количество добытого ресурса вычистяется по формуле
    //    y = factor * max(0, 1 - |t / radius| ^ sigma)
    //    t = ((x - x1) ^ 2 + (y - y1) ^ 2) ^ 0.5 - расстояние до центра источника
    //    y *= out.count

    std::string   name              = {};
    std::string   resource          = {};
    uint64_t      x                 = utils_t::npos;
    uint64_t      y                 = utils_t::npos;
    uint64_t      radius            = utils_t::npos;
    double        factor            = 1;
    double        sigma             = 2;

    bool validation(const config_t& config);
  };

  struct recipe_item_t {
    std::string   name    = {};
    uint64_t      count   = utils_t::npos;

    bool validation(const config_t& config);
  };

  struct recipe_t {
    using in_t  = std::map<std::string/*name*/, recipe_item_t>;
    using out_t = std::map<std::string/*name*/, recipe_item_t>;

    std::string   name              = {};
    in_t          in                = {};
    out_t         out               = {};

    bool validation(const config_t& config);
  };

  struct cell_pipe_t {
    std::string   direction      = {};
    std::string   resource       = {};
    uint64_t      velocity       = utils_t::npos;

    bool validation(const config_t& config);
  };

  struct cell_t { // deprecated
    using resources_t        = std::map<std::string/*name*/, resource_t>;
    using pipes_t            = std::vector<cell_pipe_t>;
    using recipes_t          = std::set<std::string>;
    using spore_bacteria_t   = std::shared_ptr<bacteria_t>;

    uint64_t            pos              = utils_t::npos;
    uint64_t            age              = 0;
    uint64_t            health           = 0;
    std::string         family           = {};
    std::string         type             = {};
    resources_t         resources        = {};
    pipes_t             pipes            = {};
    recipes_t           recipes          = {};

    bool validation(const config_t& config);
  };

  struct config_t {
    using debug_t       = std::set<std::string>;
    using resources_t   = std::map<std::string/*name*/,     resource_info_t>;
    using areas_t       = std::map<std::string/*resource*/, std::vector<area_t>>;
    using recipes_t     = std::map<std::string/*name*/,     recipe_t>;

    uint64_t      revision                    = 0;
    uint64_t      position_n                  = 20;
    uint64_t      position_max                = 400;
    uint64_t      code_size                   = 32;
    uint64_t      health_max                  = 100;
    uint64_t      age_max                     = 10000;
    uint64_t      energy_remaining            = 10;
    std::string   ip                          = "127.0.0.1";
    uint64_t      port                        = 8000;
    uint64_t      interval_update_world_ms    = 100;
    uint64_t      interval_update_epoll_ms    = 10;
    uint64_t      interval_cache_world_ms     = 10;
    uint64_t      interval_save_world_ms      = 60 * 1000;
    uint64_t      sleep_timeout               = 1;
    debug_t       debug                       = { utils_t::ERROR };
    resources_t   resources                   = {};
    areas_t       areas                       = {};
    recipes_t     recipes                     = {};

    bool validation();
  };

  struct stats_t {
    uint64_t   age               = {};
    uint64_t   bacterias_count   = {};
  };

  struct context_t {
    using cells_t       = std::map<size_t/*pos*/, cell_t>;
    using bacterias_t   = std::map<size_t/*pos*/, bacteria_sptr_t>;

    config_t      config        = {};
    cells_t       cells         = {};
    bacterias_t   bacterias     = {};
    stats_t       stats         = {};
    uint64_t      revision      = {};

    bool validation();
  };

  struct http_parser_t {
    using params_t    = std::map<std::string, std::string>;
    using headers_t   = std::map<std::string, std::string>;
    using buffer_t    = std::deque<char>;

    buffer_t      buffer           = {};

    // req, resp
    headers_t     headers          = {};
    std::string   body             = {};

    // req
    std::string   path             = {};
    params_t      params           = {};
    uint64_t      content_length   = {};
    uint64_t      state            = {};
    bool          is_end           = false;

    // resp
    std::string   code             = {};

    inline static const std::string NL               = "\r\n";
    inline static const std::string CODE_200         = "200 OK";
    inline static const std::string CODE_304         = "304 Not Modified";
    inline static const std::string CODE_404         = "404 Not Found";
    inline static const std::string CONTENT_LENGTH   = "Content-Length";
    inline static const std::string CONTENT_TYPE     = "Content-Type";
    inline static const std::string CONNECTION       = "Connection";
    inline static const std::string KEEP_ALIVE       = "keep-alive";
    inline static const std::string APPLICATION_JSON = "application/json";

    void read() {
      TRACE_GENESIS;
      LOG_GENESIS(EPOLL, "read: buffer.size: %zd", buffer.size());

      switch (state) {
        case 0: {
          path           = {};
          params         = {};
          headers        = {};
          body           = {};
          content_length = {};
          is_end         = false;

          state = 1;
          [[fallthrough]];

        } case 1: {
          auto it = std::search(buffer.begin(), buffer.end(), NL.begin(), NL.end());
          if (it == buffer.end()) {
            return;
          }
          std::string req_line = std::string(buffer.begin(), it);
          it += NL.size();
          buffer.erase(buffer.begin(), it);
          LOG_GENESIS(EPOLL, "req_line: %zd %s", req_line.size(), req_line.c_str());

          const std::regex   base_regex("^(\\w+) (.+) HTTP/\\d.\\d$");
          std::smatch        base_match;
          if (std::regex_match(req_line, base_match, base_regex)) {
            if (base_match.size() == 3) {
              LOG_GENESIS(EPOLL, "method: %s", base_match[1].str().c_str());
              std::string url = base_match[2];
              size_t pos = url.find_first_of('?');
              if (pos != std::string::npos) {
                path = url.substr(0, pos);
                std::string query = url.substr(pos + 1);
                LOG_GENESIS(EPOLL, "query: %s", query.c_str());

                const std::regex   base_regex("(\\w+)=(\\w+)(?:&|$)");
                std::smatch        base_match;
                while (std::regex_search(query, base_match, base_regex)) {
                  if (base_match.size() == 3) {
                    params[base_match[1]] = base_match[2];
                    LOG_GENESIS(EPOLL, "param: %s: %s",
                        base_match[1].str().c_str(),
                        base_match[2].str().c_str());
                  }
                  query = base_match.suffix();
                }
              } else {
                path = url;
              }
              LOG_GENESIS(EPOLL, "path: %s",   path.c_str());
            }
          }

          state = 2;
          [[fallthrough]];

        } case 2: {
          while (true) {
            auto it = std::search(buffer.begin(), buffer.end(), NL.begin(), NL.end());
            if (it == buffer.end()) {
              return;
            }
            std::string header = std::string(buffer.begin(), it);
            it += NL.size();
            buffer.erase(buffer.begin(), it);

            if (header.empty()) {
              break;
            }

            LOG_GENESIS(EPOLL, "header: %zd '%s'", header.size(), header.c_str());
            const std::regex   base_regex("^(\\S+): (.+)$");
            std::smatch        base_match;
            if (std::regex_match(header, base_match, base_regex)) {
              if (base_match.size() == 3) {
                headers[base_match[1]] = base_match[2];
              }
            }
          }

          for (const auto& [key, val] : headers) {
            LOG_GENESIS(EPOLL, "header: %s: %s", key.c_str(), val.c_str());
            if (key == CONTENT_LENGTH) {
              content_length = std::stol(val); // XXX exception
            }
          }

          state = 3;
          [[fallthrough]];

        } case 3: {
          LOG_GENESIS(EPOLL, "body size: %zd / %zd", buffer.size(), content_length);

          if (buffer.size() >= content_length) {
            auto it = buffer.begin() + content_length;
            body = std::string(buffer.begin(), it);
            buffer.erase(buffer.begin(), it);
            LOG_GENESIS(EPOLL, "body %s", body.c_str());

            is_end = 1;
            state = {};
            break;
          }

        }
      }
    }

    void write() {
      TRACE_GENESIS;

      std::stringstream ss;
      ss << "HTTP/1.1 " << code << NL;
      for (const auto& [key, val] : headers) {
        ss << key << ": " << val << NL;
      }
      ss << NL << body;

      std::string resp = ss.str();
      buffer.insert(buffer.end(), resp.begin(), resp.end());
      LOG_GENESIS(EPOLL, "response:\n%s", resp.c_str());
    }
  };

  struct client_manager_t {
    using buffers_t = std::map<int/*fd*/, std::pair<http_parser_t/*req*/, http_parser_t/*resp*/>>;

    world_t&              world;
    static const size_t   events_count             = 16;
    int                   fd_listen                = -1;
    int                   fd_epoll                 = -1;
    epoll_event           events[events_count]     = {};
    buffers_t             buffers                  = {};
    char                  buffer_tmp[32 * 1024]    = {};
    bool                  is_run                   = false;

    client_manager_t(world_t& world) : world(world) { }
    void init();
    void update();
    void close_all();
    int set_nonblock(int fd);
    int set_epoll_ctl(int fd, int events, int op);
    void process_write(int fd);
    void process_read(int fd);
  };

  struct world_t {
    context_t          ctx              = {};
    std::string        file_name        = {};
    client_manager_t   client_manager   = client_manager_t(*this);

    nlohmann::json     ctx_json         = {}; // TODO
    std::string        ctx_str          = {};
    uint64_t           time_ms          = {};
    uint64_t           update_world_ms  = {};
    uint64_t           update_epoll_ms  = {};
    uint64_t           cache_world_ms   = {};
    uint64_t           save_world_ms    = {};

    void update();
    void update_ctx();
    void update_cell_total(cell_t& cell);
    void update_cell_producer(cell_t& cell);
    void update_cell_spore(cell_t& cell);
    void update_bacteria(bacteria_sptr_t bacteria);
    bool update_cell_recipe(cell_t& cell, const std::string& recipe_name);
    void init();
    void load();
    void save();
  };

  ////////////////////////////////////////////////////////////////////////////////

  inline void to_json(nlohmann::json& json, const resource_info_t& resource_info) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource_info, name);
    JSON_SAVE2(json, resource_info, stack_size);
    JSON_SAVE2(json, resource_info, extractable);
  }

  inline void from_json(const nlohmann::json& json, resource_info_t& resource_info) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource_info, name);
    JSON_LOAD2(json, resource_info, stack_size);
    JSON_LOAD2(json, resource_info, extractable);
  }

  inline void to_json(nlohmann::json& json, const area_t& area) {
    TRACE_GENESIS;
    JSON_SAVE2(json, area, name);
    JSON_SAVE2(json, area, resource);
    JSON_SAVE2(json, area, x);
    JSON_SAVE2(json, area, y);
    JSON_SAVE2(json, area, radius);
    JSON_SAVE2(json, area, factor);
    JSON_SAVE2(json, area, sigma);
  }

  inline void from_json(const nlohmann::json& json, area_t& area) {
    TRACE_GENESIS;
    JSON_LOAD2(json, area, name);
    JSON_LOAD2(json, area, resource);
    JSON_LOAD2(json, area, x);
    JSON_LOAD2(json, area, y);
    JSON_LOAD2(json, area, radius);
    JSON_LOAD2(json, area, factor);
    JSON_LOAD2(json, area, sigma);
  }

  inline void to_json(nlohmann::json& json, const recipe_item_t& recipe_item) {
    TRACE_GENESIS;
    JSON_SAVE2(json, recipe_item, name);
    JSON_SAVE2(json, recipe_item, count);
  }

  inline void from_json(const nlohmann::json& json, recipe_item_t& recipe_item) {
    TRACE_GENESIS;
    JSON_LOAD2(json, recipe_item, name);
    JSON_LOAD2(json, recipe_item, count);
  }

  inline void to_json(nlohmann::json& json, const recipe_t& recipe) {
    TRACE_GENESIS;
    JSON_SAVE2(json, recipe, name);
    JSON_SAVE2(json, recipe, in);
    JSON_SAVE2(json, recipe, out);
  }

  inline void from_json(const nlohmann::json& json, recipe_t& recipe) {
    TRACE_GENESIS;
    JSON_LOAD2(json, recipe, name);
    JSON_LOAD2(json, recipe, in);
    JSON_LOAD2(json, recipe, out);
  }

  inline void to_json(nlohmann::json& json, const resource_t& resource) {
    TRACE_GENESIS;
    JSON_SAVE2(json, resource, name);
    JSON_SAVE2(json, resource, count);
  }

  inline void from_json(const nlohmann::json& json, resource_t& resource) {
    TRACE_GENESIS;
    JSON_LOAD2(json, resource, name);
    JSON_LOAD2(json, resource, count);
  }

  inline void to_json(nlohmann::json& json, const cell_pipe_t& cell_pipe) {
    TRACE_GENESIS;
    JSON_SAVE2(json, cell_pipe, direction);
    JSON_SAVE2(json, cell_pipe, resource);
    JSON_SAVE2(json, cell_pipe, velocity);
  }

  inline void from_json(const nlohmann::json& json, cell_pipe_t& cell_pipe) {
    TRACE_GENESIS;
    JSON_LOAD2(json, cell_pipe, direction);
    JSON_LOAD2(json, cell_pipe, resource);
    JSON_LOAD2(json, cell_pipe, velocity);
  }

  inline void to_json(nlohmann::json& json, const config_t& config) {
    TRACE_GENESIS;
    JSON_SAVE2(json, config, revision);
    JSON_SAVE2(json, config, position_n);
    JSON_SAVE2(json, config, position_max);
    JSON_SAVE2(json, config, code_size);
    JSON_SAVE2(json, config, health_max);
    JSON_SAVE2(json, config, age_max);
    JSON_SAVE2(json, config, energy_remaining);
    JSON_SAVE2(json, config, ip);
    JSON_SAVE2(json, config, port);
    JSON_SAVE2(json, config, interval_update_world_ms);
    JSON_SAVE2(json, config, interval_update_epoll_ms);
    JSON_SAVE2(json, config, interval_cache_world_ms);
    JSON_SAVE2(json, config, interval_save_world_ms);
    JSON_SAVE2(json, config, sleep_timeout);
    JSON_SAVE2(json, config, debug);
    JSON_SAVE2(json, config, resources);
    JSON_SAVE2(json, config, areas);
    JSON_SAVE2(json, config, recipes);
  }

  inline void from_json(const nlohmann::json& json, config_t& config) {
    TRACE_GENESIS;
    JSON_LOAD2(json, config, revision);
    JSON_LOAD2(json, config, position_n);
    JSON_LOAD2(json, config, position_max);
    JSON_LOAD2(json, config, code_size);
    JSON_LOAD2(json, config, health_max);
    JSON_LOAD2(json, config, age_max);
    JSON_LOAD2(json, config, energy_remaining);
    JSON_LOAD2(json, config, ip);
    JSON_LOAD2(json, config, port);
    JSON_LOAD2(json, config, interval_update_world_ms);
    JSON_LOAD2(json, config, interval_update_epoll_ms);
    JSON_LOAD2(json, config, interval_cache_world_ms);
    JSON_LOAD2(json, config, interval_save_world_ms);
    JSON_LOAD2(json, config, sleep_timeout);
    JSON_LOAD2(json, config, debug);
    JSON_LOAD2(json, config, resources);
    JSON_LOAD2(json, config, areas);
    JSON_LOAD2(json, config, recipes);
  }

  inline void to_json(nlohmann::json& json, const bacteria_t& bacteria) {
    TRACE_GENESIS;
    JSON_SAVE2(json, bacteria, family);
    JSON_SAVE2(json, bacteria, code);
    JSON_SAVE2(json, bacteria, resources);
    JSON_SAVE2(json, bacteria, pos);
    JSON_SAVE2(json, bacteria, age);
    JSON_SAVE2(json, bacteria, direction);
  }

  inline void from_json(const nlohmann::json& json, bacteria_t& bacteria) {
    TRACE_GENESIS;
    JSON_LOAD2(json, bacteria, family);
    JSON_LOAD2(json, bacteria, code);
    JSON_LOAD2(json, bacteria, resources);
    JSON_LOAD2(json, bacteria, pos);
    JSON_LOAD2(json, bacteria, age);
    JSON_LOAD2(json, bacteria, direction);
  }

  inline void to_json(nlohmann::json& json, const cell_t& cell) {
    TRACE_GENESIS;
    JSON_SAVE2(json, cell, pos);
    JSON_SAVE2(json, cell, age);
    JSON_SAVE2(json, cell, health);
    JSON_SAVE2(json, cell, family);
    JSON_SAVE2(json, cell, type);
    JSON_SAVE2(json, cell, resources);
    JSON_SAVE2(json, cell, pipes);
    JSON_SAVE2(json, cell, recipes);
  }

  inline void from_json(const nlohmann::json& json, cell_t& cell) {
    TRACE_GENESIS;
    JSON_LOAD2(json, cell, pos);
    JSON_LOAD2(json, cell, age);
    JSON_LOAD2(json, cell, health);
    JSON_LOAD2(json, cell, family);
    JSON_LOAD2(json, cell, type);
    JSON_LOAD2(json, cell, resources);
    JSON_LOAD2(json, cell, pipes);
    JSON_LOAD2(json, cell, recipes);
  }

  inline void to_json(nlohmann::json& json, const stats_t& stats) {
    TRACE_GENESIS;
    JSON_SAVE2(json, stats, age);
    JSON_SAVE2(json, stats, bacterias_count);
  }

  inline void from_json(const nlohmann::json& json, stats_t& stats) {
    TRACE_GENESIS;
    JSON_LOAD2(json, stats, age);
    JSON_LOAD2(json, stats, bacterias_count);
  }

  inline void to_json(nlohmann::json& json, const context_t& context) {
    TRACE_GENESIS;
    JSON_SAVE2(json, context, config);
    {
      auto& cells = json["cells"];
      for (const auto& [key, cell] : context.cells) {
        cells[std::to_string(key)] = cell;
      }
    }
    {
      auto& bacterias = json["bacterias"];
      for (const auto& [key, bacteria] : context.bacterias) {
        if (bacteria) {
          bacterias[std::to_string(key)] = *bacteria;
        }
      }
    }
    JSON_SAVE2(json, context, stats);
    JSON_SAVE2(json, context, revision);
  }

  inline void from_json(const nlohmann::json& json, context_t& context) {
    TRACE_GENESIS;
    JSON_LOAD2(json, context, config);
    {
      auto& cells = json["cells"];
      for (auto& cell_json : cells) {
        cell_t cell = cell_json;
        context.cells[cell.pos] = std::move(cell);
      }
    }
    {
      auto& bacterias = json["bacterias"];
      for (auto& bacteria_json : bacterias) {
        bacteria_t bacteria = bacteria_json;
        context.bacterias[bacteria.pos] = std::make_shared<bacteria_t>(std::move(bacteria));
      }
    }
    JSON_LOAD2(json, context, stats);
    JSON_LOAD2(json, context, revision);
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool resource_info_t::validation(const config_t&) {
    TRACE_GENESIS;

    if (name.empty()) {
      LOG_GENESIS(ERROR, "invalid resource_info_t::name %s", name.c_str());
      return false;
    }

    if (stack_size == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid resource_info_t::stack_size %zd", stack_size);
      return false;
    }

    // extractable

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool bacteria_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (family.empty()) {
      LOG_GENESIS(ERROR, "invalid bacteria_t::family %s", family.c_str());
      return false;
    }

    while (code.size() < config.code_size) {
      code.push_back(utils_t::rand_u64() % 0xFF);
    }
    code.resize(config.code_size);

    // resources

    if (pos >= config.position_max) {
      LOG_GENESIS(ERROR, "invalid bacteria_t::pos %zd", pos);
      return false;
    }

    // age

    direction %= 8;

    return true;
  }

  void bacteria_t::init(const config_t& config) {
    TRACE_GENESIS;

    family             = "r" + std::to_string(utils_t::rand_u64());
    // code
    resources          = {};
    pos                = utils_t::rand_u64() % config.position_max;
    age                = {};
    direction          = utils_t::rand_u64() % 8;
    energy_remaining   = {};

    resources[utils_t::ENERGY] = {utils_t::ENERGY, config.resources.at(utils_t::ENERGY).stack_size / 2 };
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool cell_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (pos >= config.position_max) {
      LOG_GENESIS(ERROR, "invalid cell_t::pos %zd", pos);
      return false;
    }

    // age

    health = std::min(health, config.health_max);

    if (family.empty()) {
      LOG_GENESIS(ERROR, "invalid cell_t::family %s", family.c_str());
      return false;
    }

    if (!utils_t::cell_types.contains(type)) {
      LOG_GENESIS(ERROR, "invalid cell_t::type %s", type.c_str());
      return false;
    }

    for (auto& [key, resource] : resources) {
      if (key != resource.name || !resource.validation(config)) {
        LOG_GENESIS(ERROR, "invalid cell_t::resources");
        return false;
      }
    }

    for (auto& pipe : pipes) {
      if (!pipe.validation(config)) {
        LOG_GENESIS(ERROR, "invalid cell_t::pipes");
        return false;
      }
    }

    for (auto& recipe_name : recipes) {
      if (!config.recipes.contains(recipe_name)) {
        LOG_GENESIS(ERROR, "invalid cell_t::recipes %s", recipe_name.c_str());
        return false;
      }
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool area_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (name.empty()) {
      LOG_GENESIS(ERROR, "invalid area_t::name %s", name.c_str());
      return false;
    }

    if (!config.resources.contains(resource)) {
      LOG_GENESIS(ERROR, "invalid area_t::resource %s", resource.c_str());
      return false;
    }

    if (x == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid area_t::x %zd", x);
      return false;
    }

    if (y == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid area_t::y %zd", y);
      return false;
    }

    if (!radius || radius == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid area_t::radius %zd", radius);
      return false;
    }

    if (factor <= 0) {
      LOG_GENESIS(ERROR, "invalid area_t::factor %f", factor);
      return false;
    }

    if (sigma <= 0) {
      LOG_GENESIS(ERROR, "invalid area_t::sigma %f", sigma);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool recipe_item_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (!config.resources.contains(name)) {
      LOG_GENESIS(ERROR, "invalid recipe_item_t::name %s", name.c_str());
      return false;
    }

    if (!count || count == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid recipe_item_t::count %s", count);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool recipe_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (name.empty()) {
      LOG_GENESIS(ERROR, "invalid recipe_t::name %s", name.c_str());
      return false;
    }

    for (auto& [key, recipe_item] : in) {
      if (key != recipe_item.name || !recipe_item.validation(config)) {
        LOG_GENESIS(ERROR, "invalid recipe_t::in");
        return false;
      }
    }

    for (auto& [key, recipe_item] : out) {
      if (key != recipe_item.name || !recipe_item.validation(config)) {
        LOG_GENESIS(ERROR, "invalid recipe_t::out");
        return false;
      }
    }

    if (in.empty() && out.empty()) {
      LOG_GENESIS(ERROR, "invalid recipe_t::[in, out]");
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool resource_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (!config.resources.contains(name)) {
      LOG_GENESIS(ERROR, "invalid resource_t::name %s", name.c_str());
      return false;
    }

    if (count == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid resource_t::count %zd", count);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool cell_pipe_t::validation(const config_t& config) {
    TRACE_GENESIS;

    if (std::find(utils_t::directions.begin(), utils_t::directions.end(), direction)
        == utils_t::directions.end())
    {
      LOG_GENESIS(ERROR, "invalid cell_pipe_t::direction %s", direction.c_str());
      return false;
    }

    if (!config.resources.contains(resource)) {
      LOG_GENESIS(ERROR, "invalid cell_pipe_t::resource %s", resource.c_str());
      return false;
    }

    if (!velocity || velocity == utils_t::npos) {
      LOG_GENESIS(ERROR, "invalid cell_pipe_t::velocity %zd", velocity);
      return false;
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool config_t::validation() {
    TRACE_GENESIS;
    position_max = (position_max / position_n) * position_n;

    if (position_n < 5 || position_n > 1000) {
      LOG_GENESIS(ERROR, "invalid config_t::position_n %zd", position_n);
      return false;
    }

    if (position_max < 5 * 5 || position_max > 1000 * 1000) {
      LOG_GENESIS(ERROR, "invalid config_t::position_max %zd", position_max);
      return false;
    }

    if (code_size < utils_t::CODE_SIZE_MIN || code_size > 1000) {
      LOG_GENESIS(ERROR, "invalid config_t::code_size %zd", code_size);
      return false;
    }

    if (!health_max) {
      LOG_GENESIS(ERROR, "invalid config_t::health_max %zd", health_max);
      return false;
    }

    // age_max
    // energy_remaining

    if (ip.size() > 46) { // IPv6
      LOG_GENESIS(ERROR, "invalid config_t::ip %s", ip.c_str());
      return false;
    }

    if (!port || port > 0xFFFF) {
      LOG_GENESIS(ERROR, "invalid config_t::port %zd", port);
      return false;
    }

    if (debug.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::debug %zd", debug.size());
      return false;
    }

    if (resources.empty() || resources.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::resources %zd", resources.size());
      return false;
    }

    if (!resources.contains(utils_t::ENERGY)) {
      resources[utils_t::ENERGY] = {utils_t::ENERGY, 1000, true};
    }

    for (auto& [key, resource] : resources) {
      if (key != resource.name || !resource.validation(*this)) {
        LOG_GENESIS(ERROR, "invalid config_t::resources");
        return false;
      }
    }

    if (areas.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::areas %zd", areas.size());
      return false;
    }

    for (auto& [key, area_v] : areas) {
      for (auto& area : area_v) {
        if (key != area.resource || !area.validation(*this)) {
          LOG_GENESIS(ERROR, "invalid config_t::area");
          return false;
        }
      }
    }

    if (recipes.size() < 1 || recipes.size() > 100) {
      LOG_GENESIS(ERROR, "invalid config_t::recipes %zd", recipes.size());
      return false;
    }

    for (auto& [key, recipe] : recipes) {
      if (key != recipe.name || !recipe.validation(*this)) {
        LOG_GENESIS(ERROR, "invalid config_t::recipe");
        return false;
      }
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  bool context_t::validation() {
    TRACE_GENESIS;

    if (!config.validation()) {
      LOG_GENESIS(ERROR, "invalid config_json_t::config");
      return false;
    }

    for (auto& [key, cell] : cells) {
      if (key != cell.pos || !cell.validation(config)) {
        LOG_GENESIS(ERROR, "invalid config_json_t::cells");
        return false;
      }
    }

    for (auto& [key, bacteria] : bacterias) {
      if (!bacteria || key != bacteria->pos || !bacteria->validation(config)) {
        LOG_GENESIS(ERROR, "invalid config_json_t::bacterias");
        return false;
      }
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////

  uint64_t utils_t::n(const config_t& config) {
    return config.position_n;
  }

  uint64_t utils_t::m(const config_t& config) {
    return config.position_max / config.position_n;
  }

  std::pair<uint64_t, uint64_t> utils_t::position_to_xy(const config_t& config, uint64_t position) {
    TRACE_GENESIS;
    if (position > config.position_max) {
      return {npos, npos};
    }
    uint64_t x = position % config.position_n;
    uint64_t y = position / config.position_n;
    return {x, y};
  }

  uint64_t utils_t::position_from_xy(const config_t& config, uint64_t x, uint64_t y) {
    TRACE_GENESIS;
    if (x >= n(config) || y >= m(config)) {
      return npos;
    }
    return y * n(config) + x;
  }

  uint64_t utils_t::distance(const config_t& config, uint64_t position, uint64_t x, uint64_t y) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "position: %zd", position);
    LOG_GENESIS(ARGS, "x, y: %zd %zd", x, y);
    auto [x1, y1] = position_to_xy(config, position);
    uint64_t ret = npos;
    if (x1 != npos && y1 != npos) {
      uint64_t dx = (x >= x1) ? (x - x1) : (x1 - x);
      uint64_t dy = (y >= y1) ? (y - y1) : (y1 - y);
      ret = std::hypot(dx, dy);
    }
    LOG_GENESIS(ARGS, "ret: %zd", ret);
    return ret;
  }

  uint64_t utils_t::position_next(const config_t& config, uint64_t position, uint64_t direction) {
    TRACE_GENESIS;
    direction %= 8;
    LOG_GENESIS(ARGS, "position:  %zd", position);
    LOG_GENESIS(ARGS, "direction: %zd", direction);
    uint64_t ret = npos;
    auto [x, y] = position_to_xy(config, position);

    if (x == npos || y == npos) {
      return npos;
    }

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
      default: return npos;
    }

    ret = position_from_xy(config, x + dx, y + dy);
    LOG_GENESIS(ARGS, "ret: %zd", ret);
    return ret;
  }

  uint64_t utils_t::position_next(const config_t& config, uint64_t position, const std::string& direction) {
    TRACE_GENESIS;
    LOG_GENESIS(ARGS, "position: %zd", position);
    LOG_GENESIS(ARGS, "direction: %s", direction.c_str());
    uint64_t ret = npos;
    auto [x, y] = position_to_xy(config, position);

    if (x == npos || y == npos) {
      return npos;
    }

    int dx = 0;
    int dy = 0;
    if (direction == DIR_L) {
      dx = -1;
    } else if (direction == DIR_R) {
      dx = 1;
    } else if (direction == DIR_U) {
      dy = -1;
    } else if (direction == DIR_D) {
      dy = 1;
    } else if (direction == DIR_LU) {
      dx = -1;
      dy = -1;
    } else if (direction == DIR_LD) {
      dx = -1;
      dy = 1;
    } else if (direction == DIR_RU) {
      dx = 1;
      dy = -1;
    } else if (direction == DIR_RD) {
      dx = 1;
      dy = 1;
    } else {
      return npos;
    }

    ret = position_from_xy(config, x + dx, y + dy);
    LOG_GENESIS(ARGS, "ret: %zd", ret);
    return ret;
  }

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
    TRACE_GENESIS;
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

  void world_t::update() {
    TRACE_GENESIS;

    bool need_sleep = true;

    time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    LOG_GENESIS(TIME, "time %zd", time_ms);

    if (update_world_ms < time_ms) {
      LOG_GENESIS(TIME, "update_world_ms %zd   %zd", time_ms, time_ms - update_world_ms);
      update_world_ms = time_ms + ctx.config.interval_update_world_ms;
      update_ctx();
      need_sleep = false;
    }

    if (update_epoll_ms < time_ms) {
      LOG_GENESIS(TIME, "update_epoll_ms %zd   %zd", time_ms, time_ms - update_epoll_ms);
      update_epoll_ms = time_ms + ctx.config.interval_update_epoll_ms;
      client_manager.update();
      need_sleep = false;
    }

    if (cache_world_ms < time_ms) {
      LOG_GENESIS(TIME, "cache_world_ms %zd   %zd", time_ms, time_ms - cache_world_ms);
      cache_world_ms = time_ms + ctx.config.interval_cache_world_ms;
      ctx_json = ctx;
      ctx_str = ctx_json.dump();
      need_sleep = false;
    }

    if (save_world_ms < time_ms) {
      LOG_GENESIS(TIME, "save_world_ms %zd   %zd", time_ms, time_ms - save_world_ms);
      save_world_ms = time_ms + ctx.config.interval_save_world_ms;
      utils_t::save(ctx_json, file_name);
      need_sleep = false;
    }

    if (need_sleep && ctx.config.sleep_timeout) {
      LOG_GENESIS(TIME, "sleep %zd", time_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(ctx.config.sleep_timeout));
    }
  }

  void world_t::update_ctx() {
    TRACE_GENESIS;

    for (auto& [_, cell] : ctx.cells) {
      LOG_GENESIS(DEBUG, "cell.pos %zd", cell.pos);
      try {
        if (cell.type == utils_t::PRODUCER) {
          update_cell_producer(cell);
        } else if (cell.type == utils_t::SPORE) {
          update_cell_spore(cell);
        } else {
          LOG_GENESIS(ERROR, "unknown cell.type %s", cell.type.c_str());
        }
        update_cell_total(cell);
      } catch (const std::exception& e) {
        LOG_GENESIS(ERROR, "exception %s", e.what());
      }
    }

    std::erase_if(ctx.cells, [this](const auto& kv) {
        const auto& cell = kv.second;
        return !cell.health
          || cell.age > ctx.config.age_max
          || (cell.resources.contains(utils_t::ENERGY)
              && !cell.resources.at(utils_t::ENERGY).count); });

    for (auto [_, bacteria] : ctx.bacterias) {
      if (!bacteria) {
        continue;
      }

      bacteria->energy_remaining = ctx.config.energy_remaining;
    }

    for (auto [_, bacteria] : ctx.bacterias) {
      if (!bacteria) {
        continue;
      }

      while (bacteria->energy_remaining > 0) {
        update_bacteria(bacteria);
        bacteria->energy_remaining--;
      }

      bacteria->age++;
      auto& count = bacteria->resources[utils_t::ENERGY].count;
      if (count)
        count--;
    }

    std::erase_if(ctx.bacterias, [this](const auto& kv) {
        const auto& bacteria = kv.second;
        return !bacteria
          || bacteria->resources[utils_t::ENERGY].count <= 0
          || bacteria->age > ctx.config.age_max; });

    {
      uint16_t count_min  = 3; // 0.1 * ctx.config.position_n;
      uint16_t count_need = 1.0 * ctx.config.position_n;
      if (ctx.bacterias.size() < count_min) {
        while (ctx.bacterias.size() < count_need) {
          auto bacteria = std::make_shared<bacteria_t>();
          bacteria->init(ctx.config);
          if (bacteria->validation(ctx.config) && !ctx.bacterias.contains(bacteria->pos)) {
            ctx.bacterias[bacteria->pos] = std::move(bacteria);
          }
        }
      }
    }

    // stats
    {
      ctx.stats.age++;
      ctx.stats.bacterias_count = ctx.bacterias.size();
    }

    ctx.revision++;
  }

  void world_t::update_cell_total(cell_t& cell) {
    TRACE_GENESIS;

    for (const auto& pipe : cell.pipes) {
      if (!cell.resources.contains(pipe.resource)) {
        continue;
      }
      auto& resource = cell.resources.at(pipe.resource);

      uint64_t pos_next = utils_t::position_next(ctx.config, cell.pos, pipe.direction);
      if (pos_next == utils_t::npos) {
        continue;
      }

      if (!ctx.cells.contains(pos_next)) {
        continue;
      }
      auto& cell_next = ctx.cells.at(pos_next);

      if (!cell_next.resources.contains(pipe.resource)) {
        continue;
      }
      auto& resource_next = cell_next.resources.at(pipe.resource);

      uint64_t stack_size = ctx.config.resources.at(pipe.resource).stack_size;
      uint64_t count = std::min(pipe.velocity, resource.count);
      count = std::min(count, stack_size - resource_next.count);

      resource.count -= count;
      resource_next.count += count;
      LOG_GENESIS(DEBUG, "count %zd", count);
    }

    cell.age++;
  }

  void world_t::update_cell_producer(cell_t& cell) {
    TRACE_GENESIS;

    for (const auto& recipe_name : cell.recipes) {
      update_cell_recipe(cell, recipe_name);
    }
  }

  void world_t::update_cell_spore(cell_t& cell) {
    TRACE_GENESIS;

    if (ctx.bacterias.contains(cell.pos)) {
      return;
    }

    for (const auto& recipe_name : cell.recipes) {
      if (!update_cell_recipe(cell, recipe_name)) {
        return;
      }
    }

    auto bacteria = std::make_shared<bacteria_t>();
    // TODO mutation

    if (bacteria->validation(ctx.config)) {
        ctx.bacterias[cell.pos] = bacteria;
    }
  }

  void world_t::update_bacteria(bacteria_sptr_t bacteria) {
    TRACE_GENESIS;

    auto& code = bacteria->code;
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

        bacteria->direction = (bacteria->direction + dir) % 8;
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

        uint64_t pos_next = utils_t::position_next(ctx.config, bacteria->pos, bacteria->direction);
        if (pos_next != utils_t::npos && !ctx.bacterias.contains(pos_next)) {
          ctx.bacterias[pos_next] = bacteria;
          ctx.bacterias[bacteria->pos].reset();
          bacteria->pos = pos_next;
        }
        break;

      } case 19: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        hd.arg++;
        uint64_t opnd2 = {};
        uint64_t opnd2_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd2_addr, opnd2);

        LOG_GENESIS(MIND, "CLONE <%zd> <%zd>", opnd1, opnd2);

        uint64_t dir = opnd1 % 8;
        double probability = 0.1 * opnd2 / 0xFFFF;
        uint64_t pos_next = utils_t::position_next(ctx.config, bacteria->pos, dir);
        LOG_GENESIS(MIND, "pos_next: %zd", pos_next);
        LOG_GENESIS(MIND, "energy: %zd", bacteria->resources[utils_t::ENERGY].count);
        if (pos_next != utils_t::npos
            && bacteria->resources[utils_t::ENERGY].count > 600
            && !ctx.bacterias.contains(pos_next))
        {
          for (auto [_, resource]  : bacteria->resources) {
            resource.count /= 2;
          }
          LOG_GENESIS(MIND, "energy ok: %zd", bacteria->resources[utils_t::ENERGY].count);
          auto bacteria_child = std::make_shared<bacteria_t>(*bacteria);
          for (auto& byte : code) {
            if (utils_t::rand_u64() % 0xFFFF > probability) {
              byte = utils_t::rand_u64();
            }
          }
          if (bacteria_child->validation(ctx.config)) {
            ctx.bacterias[pos_next] = std::move(bacteria_child);
          }
        }
        break;

      } case 20: {
        hd.arg++;
        uint64_t opnd1 = {};
        uint64_t opnd1_addr = utils_t::fasthash64(&hd, sizeof(hd), seed);
        utils_t::load_by_hash<uint16_t>(code, opnd1_addr, opnd1);

        LOG_GENESIS(MIND, "RECIPE <%zd>", opnd1);

        [this, bacteria, opnd1]() {
          const auto& recipes = ctx.config.recipes;
          auto it = recipes.begin();
          std::advance(it, opnd1 % recipes.size());
          const auto& recipe = it->second;
          LOG_GENESIS(MIND, "name %s", recipe.name.c_str());

          auto& resources = bacteria->resources;
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
              const auto& areas = ctx.config.areas.at(recipe_item.name);

              for (const auto area : areas) {
                uint64_t dist = utils_t::distance(ctx.config, bacteria->pos, area.x, area.y);
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

  bool world_t::update_cell_recipe(cell_t& cell, const std::string& recipe_name) {
    TRACE_GENESIS;

    const auto& recipes = ctx.config.recipes;
    if (!recipes.contains(recipe_name)) {
      LOG_GENESIS(ERROR, "recipe_name not found: %s", recipe_name.c_str());
      return false;
    }

    const auto& recipe = recipes.at(recipe_name);
    LOG_GENESIS(DEBUG, "recipe_name %s", recipe.name.c_str());

    bool need_produce = false;

    for (const auto& [_, recipe_item] : recipe.out) {
      if (!cell.resources.contains(recipe_item.name)) {
        continue;
      }

      auto& cell_resource = cell.resources.at(recipe_item.name);
      const auto& resource_info = ctx.config.resources.at(cell_resource.name);
      if (cell_resource.count < resource_info.stack_size) {
        need_produce = true;
      }
    }

    need_produce = true;

    for (const auto& [_, recipe_item] : recipe.in) {
      if (!cell.resources.contains(recipe_item.name)) {
        need_produce = false;
        break;
      }

      const auto& cell_resource = cell.resources.at(recipe_item.name);
      LOG_GENESIS(DEBUG, "resource.count %zd", cell_resource.count);
      LOG_GENESIS(DEBUG, "recipe_item.count %zd", recipe_item.count);
      if (cell_resource.count < recipe_item.count) {
        need_produce = false;
        break;
      }
    }

    if (!need_produce) {
      return false;
    }

    for (const auto& [_, recipe_item] : recipe.in) {
      auto& cell_resource = cell.resources.at(recipe_item.name);
      cell_resource.count -= recipe_item.count;
    }

    for (const auto& [_, recipe_item] : recipe.out) {
      if (!cell.resources.contains(recipe_item.name)) {
        continue;
      }

      auto& cell_resource = cell.resources.at(recipe_item.name);
      const auto& resource_info = ctx.config.resources.at(cell_resource.name);
      uint64_t count = recipe_item.count;

      if (resource_info.extractable) {
        double multiplier = {};
        const auto& areas = ctx.config.areas.at(recipe_item.name);
        for (const auto area : areas) {
          uint64_t dist = utils_t::distance(ctx.config, cell.pos, area.x, area.y);
          multiplier += area.factor * std::max(0., 1. - std::pow(std::abs((double) dist / area.radius), area.sigma));
          LOG_GENESIS(DEBUG, "multiplier %f", multiplier);
        }
        count *= multiplier;
        LOG_GENESIS(DEBUG, "count %zd", count);
      }

      count = std::min(count, resource_info.stack_size - cell_resource.count);
      LOG_GENESIS(DEBUG, "count new %zd %zd", cell_resource.count, count);
      cell_resource.count += count;
    }

    return true;
  }

  void world_t::init() {
    TRACE_GENESIS;
    load();
    utils_t::debug = ctx.config.debug;
    save();

    client_manager.init();
  }

  void world_t::load() {
    TRACE_GENESIS;
    nlohmann::json json;
    if (!utils_t::load(json, file_name)) {
      LOG_GENESIS(ERROR, "can not load file %s", file_name.c_str());
      throw std::runtime_error("invalid json");
    }

    ctx = json;

    if (!ctx.validation()) {
      LOG_GENESIS(ERROR, "invalid context");
      throw std::runtime_error("invalid context");
    }
  }

  void world_t::save() {
    TRACE_GENESIS;

    nlohmann::json json;
    json = ctx;
    utils_t::save(json, file_name);
  }

  ////////////////////////////////////////////////////////////////////////////////

  void client_manager_t::init() {
    TRACE_GENESIS;

    if (is_run) {
      return;
    }

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
    if (int ret = inet_pton(AF_INET, world.ctx.config.ip.c_str(), &ip_addr); ret < 0) {
      LOG_GENESIS(ERROR, "inet_pton(...): %d", ret);
      close_all();
      return;
    }

    sockaddr_in addr;
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ip_addr.s_addr;
    addr.sin_port        = htons((uint16_t) world.ctx.config.port);

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

  void client_manager_t::update() {
    TRACE_GENESIS;

    if (!is_run) {
      // init();
      return;
    }

    int event_count = epoll_wait(fd_epoll, events, events_count, 0);

    for (int i = 0; i < event_count; i++) {
      epoll_event& event = events[i];
      LOG_GENESIS(EPOLL, "events: 0x%x", event.events);
      LOG_GENESIS(EPOLL, "data.fd: %d", event.data.fd);
      LOG_GENESIS(EPOLL, "buffers.size(): %zd", buffers.size());

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

  void client_manager_t::close_all() {
    TRACE_GENESIS;

    close(fd_listen);
    close(fd_epoll);
    for (auto& [fd, _] : buffers) {
      close(fd);
    }
    buffers.clear();
    is_run = false;
  }

  int client_manager_t::set_nonblock(int fd) {
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

  int client_manager_t::set_epoll_ctl(int fd, int events, int op) {
    epoll_event event;
    event.data.fd = fd;
    event.events = events;
    int ret = epoll_ctl(fd_epoll, op, fd, &event);
    if (ret < 0) {
      LOG_GENESIS(ERROR, "epoll_ctl(...): %d", ret);
    }
    return ret;
  }

  void client_manager_t::process_write(int fd) {
    TRACE_GENESIS;
    auto& http_parser = buffers[fd].second;
    auto& buffer      = http_parser.buffer;

    int bytes_write = std::min(sizeof(buffer_tmp), buffer.size());

    std::copy(buffer.begin(), buffer.begin() + bytes_write, std::begin(buffer_tmp));

    bytes_write = write(fd, buffer_tmp, bytes_write);

    if (bytes_write <= 0) {
      LOG_GENESIS(ERROR, "write(...): %d", bytes_write);
      close(fd);
      return;
    }

    buffer.erase(buffer.begin(), buffer.begin() + bytes_write);
    LOG_GENESIS(EPOLL, "write: %d", bytes_write);

    if (http_parser.buffer.empty()) {
      if (set_epoll_ctl(fd, EPOLLIN, EPOLL_CTL_MOD) < 0) {
        close(fd);
      }
    }
  }

  void client_manager_t::process_read(int fd) {
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
      auto& http_parser = buffers[fd].first;
      auto& buffer      = http_parser.buffer;

      buffer.insert(buffer.end(), std::begin(buffer_tmp),
          std::begin(buffer_tmp) + bytes_read);

      http_parser.read();

      if (http_parser.is_end) {
        auto& http_parser_resp = buffers[fd].second;

        if (http_parser.path == "/genesis/world") {
          uint64_t revision = {};
          auto it = http_parser.params.find("revision");
          if (it != http_parser.params.end()) {
            revision = stol(it->second); // XXX exception
          }
          uint64_t revision_ctx = world.ctx_json.value("revision", uint64_t{});

          LOG_GENESIS(EPOLL, "revision: %zd   %zd", revision, revision_ctx);
          if (revision != revision_ctx) {
            http_parser_resp.body = world.ctx_str;
            http_parser_resp.code = http_parser_t::CODE_200;
          } else {
            http_parser_resp.body = {};
            http_parser_resp.code = http_parser_t::CODE_304;
          }
        } else {
          http_parser_resp.body = {};
          http_parser_resp.code = http_parser_t::CODE_404;
        }

        http_parser_resp.headers[http_parser_t::CONTENT_LENGTH] = std::to_string(http_parser_resp.body.size());
        http_parser_resp.headers[http_parser_t::CONTENT_TYPE]   = http_parser_t::APPLICATION_JSON;
        http_parser_resp.headers[http_parser_t::CONNECTION]     = http_parser_t::KEEP_ALIVE;
        http_parser_resp.headers["Access-Control-Allow-Origin"] =  "*";

        http_parser_resp.write();

        if (set_epoll_ctl(fd, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD) < 0) {
          close(fd);
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
}



int main(int argc, char* argv[]) {

  if (argc <= 1) {
    std::cerr << "usage: " << (argc > 0 ? argv[0] : "<program>")
        << " <world_name.json>" << std::endl;
    return -1;
  }

  std::error_code ec;
  std::string file_name = std::filesystem::absolute(argv[1], ec);

  if (ec) {
    std::cerr << "invalid path: " << ec.message().c_str() << std::endl;
    return -1;
  }

  using namespace genesis_n;

  world_t world;
  world.file_name = file_name;
  world.init();

  while (true) {
    world.update();
  }

  std::cout << "end" << std::endl;

  return 0;
}

