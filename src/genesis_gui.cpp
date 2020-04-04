
#include <SFML/Graphics.hpp>
#include <iostream>
#include <atomic>
#include <mutex>
#include <deque>
#include "genesis.h"

using namespace genesis_n;



struct genesis_sfml_t {
  struct sfml_config_t {
    std::string   font_path              = "../resources/fonts/Roboto-Regular.ttf";
    float         win_x                  = 600;
    float         win_y                  = 600;
    float         zoom                   = 1.05;
    uint32_t      fps                    = 10;
    uint32_t      text_size              = 14;
    uint32_t      color_background       = 0xE0E0E0FF;
    uint32_t      color_area_available   = 0xD0D0D0FF;
    uint32_t      color_min              = 0x0000FFFF;
    uint32_t      color_max              = 0xFF0000FF;
  };

  enum mode_t : uint64_t {
    MODE_NORMAL,
    MODE_AGE,
    MODE_AREA,
    MODE_RESOURCE,
    MODE_FAMILY,
    MODE_COUNT,
  };

  struct cell_t {
    using resources_t   = std::vector<res_val_t>;

    bool          alive;
    uint64_t      family;
    xy_pos_t      pos;
    uint64_t      age;
    resources_t   resources_microbe;
    resources_t   resources_world;
  };

  struct world_ctx_t {
    using cells_t = std::vector<cell_t>;

    bool       valid = false;
    config_t   config;
    cells_t    cells;
    stats_t    stats;
  };

  class world_safe_t {
    world_t             _world;
    world_ctx_t         _ctx;
    std::timed_mutex    _mutex;
    std::atomic<bool>   _need_data = false;
    std::atomic<bool>   _need_update = false;
    std::atomic<bool>   _pause = false;
    std::atomic<bool>   _stop = false;

   public:
    void init(const std::string& config_file_name, const std::string& world_file_name) {
      _world.config_file_name = config_file_name;
      _world.world_file_name  = world_file_name;
    }

    void stop() {
      _stop = true;
    }

    void pause() {
      _pause = !_pause;
    }

    void need_update() {
      _need_update = true;
    }

    void update() {
      _world.init();
      while (!_stop) {
        if (_pause) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }
        _world.update();

        if (_need_update) {
          _world.save_data();
          _world.load_config();
          _world.load_data();
          _need_update = false;
        }

        if (_need_data && _mutex.try_lock_for(std::chrono::milliseconds(1))) {
          _ctx.config = _world.config;
          _ctx.stats = _world.stats;
          _ctx.cells.resize(_world.config.x_max * _world.config.y_max);
          for (size_t ind{}; ind < _world.cells.size(); ++ind) {
            const auto& cell    = _world.cells[ind];
            const auto& microbe = cell.microbe;
            auto&       cell_n  = _ctx.cells[ind];
            cell_n.alive        = microbe.alive;
            cell_n.family       = microbe.family;
            cell_n.pos          = { ind % _world.config.x_max, ind / _world.config.y_max };
            cell_n.age          = microbe.age;
            cell_n.resources_microbe   = microbe.resources;
            cell_n.resources_world     = cell.resources;
          }
          _ctx.valid = true;
          _need_data = false;
          _mutex.unlock();
        }
      }
      _world.save_data();
    }

    void get_cells(world_ctx_t& ctx) {
      if (!_need_data && _mutex.try_lock_for(std::chrono::milliseconds(1))) {
        std::swap(_ctx, ctx);
        _mutex.unlock();
      }
      _need_data = true;
    }
  };



  sfml_config_t       sfml_config      = {};

  std::thread         thread_world     = {};

  std::string         config_file_name;
  std::string         world_file_name;
  world_ctx_t         ctx;
  config_t            config;
  world_safe_t        world;

  bool                showing_help     = false;
  std::string         stats_text       = {};
  std::string         debug_text       = {};

  sf::RenderWindow    window;
  sf::View            view_world;
  sf::View            view_text;
  sf::Vector2f        pos_old;
  sf::Vector2f        pos_mouse;
  bool                moving           = false;
  sf::Font            font;

  uint64_t            mode             = MODE_NORMAL;
  uint64_t            mode_param       = {};

  void init() {
    window.create(sf::VideoMode(sfml_config.win_x, sfml_config.win_y),
        "Genesis   amyasnikov.pro", sf::Style::Close | sf::Style::Resize);

    window.setFramerateLimit(sfml_config.fps);

    view_world.setCenter(sfml_config.win_x / 2, sfml_config.win_y / 2);

    if (!font.loadFromFile(sfml_config.font_path)) {
      throw std::runtime_error("can not load font");
    }

    world.init(config_file_name, world_file_name);

    thread_world = std::move(std::thread([this] { world.update(); }));
  }

  void deinit() {
    world.stop();
    thread_world.join();
  }

  void loop() {
    while (window.isOpen()) {
      check_events();
      get_data();
      draw_data();
    }
  }

  void check_events() {
    sf::Event event;
    while (window.pollEvent(event)) {
      switch (event.type) {
        case sf::Event::Closed: {
          window.close();
          break;

        } case sf::Event::MouseButtonPressed: {
          if (event.mouseButton.button == sf::Mouse::Left) {
            moving = true;
            pos_old = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
          }
          break;

        } case sf::Event::MouseButtonReleased: {
          if (event.mouseButton.button == sf::Mouse::Left) {
            moving = false;
          }
          break;

        } case sf::Event::MouseMoved: {
          pos_mouse = window.mapPixelToCoords(sf::Vector2i(event.mouseMove.x, event.mouseMove.y));

          if (!moving)
            break;

          const sf::Vector2f pos_new = window.mapPixelToCoords(sf::Vector2i(event.mouseMove.x, event.mouseMove.y));
          const sf::Vector2f pos_delta = pos_old - pos_new;

          view_world.setCenter(view_world.getCenter() + pos_delta);
          window.setView(view_world);
          pos_old = window.mapPixelToCoords(sf::Vector2i(event.mouseMove.x, event.mouseMove.y));
          break;

        } case sf::Event::MouseWheelScrolled: {
          if (moving)
            break;

          float zoom = sfml_config.zoom;
          if (event.mouseWheelScroll.delta > 0) {
            zoom = 1 / zoom;
          }

          view_world.zoom(zoom);
          window.setView(view_world);
          break;

        } case sf::Event::Resized: {
          double aspect_ratio = double(window.getSize().x) / window.getSize().y;

          view_world.setSize(window.getSize().x, window.getSize().x / aspect_ratio);

          view_text.setCenter(window.getSize().x / 2, window.getSize().y / 2);
          view_text.setSize(window.getSize().x, window.getSize().x / aspect_ratio);
          break;

        } case sf::Event::KeyReleased: {
          switch (event.key.code) {
            case sf::Keyboard::Q: {
              window.close();
              break;

            } case sf::Keyboard::U: {
              world.need_update();
              break;

            } case sf::Keyboard::Space: {
              world.pause();
              break;

            } case sf::Keyboard::H: {
              showing_help = !showing_help;
              break;

            } case sf::Keyboard::P: {
              mode_param++;
              break;

            } case sf::Keyboard::M: {
              mode++;
              if (mode == MODE_COUNT) {
                mode = {};
              }
              break;

            }
            default: break;
          }
          break;
        }

        default: break;
      }
    }
  }

  void get_data() {
    world.get_cells(ctx);

    if (!ctx.valid) {
      return;
    }

    static std::string help_text =
      "\n Space - pause"
      "\n U - reload config"
      "\n M - change mode"
      "\n P - change mode_param"
      "\n Q - quit";

    const auto& config = ctx.config;

    std::string mode_text{};
    switch (mode) {
      case MODE_NORMAL:   mode_text = "normal"; break;
      case MODE_AGE:      mode_text = "age"; break;
      case MODE_AREA:     mode_text = "area "
                              + config.resources[mode_param % config.resources.size()].name; break;
      case MODE_RESOURCE: mode_text = "resource "
                              + config.resources[mode_param % config.resources.size()].name; break;
      case MODE_FAMILY:   mode_text = "family"; break;
      default:            mode_text = "unknown"; break;
    }

    const auto& stats = ctx.stats;
    stats_text = std::string{}
      + " H - show help"
      + (showing_help ? help_text : "") + "\n"
      + "\n mode: " + mode_text
      + "\n age: " + std::to_string(stats.age)
      + "\n microbes_count: " + std::to_string(stats.microbes_count)
      + "\n microbes_age_avg: " + std::to_string((uint64_t) stats.microbes_age_avg)
      + "\n time_update: " + std::to_string(stats.time_update)
      + "\n bpms: " + std::to_string(uint64_t (stats.microbes_count / std::max(1UL, stats.time_update)))
      + "\n "
      + "\n pos: " + std::to_string(int(pos_mouse.x)) + "\t" + std::to_string(int(pos_mouse.y))
      + "";

    int x = pos_mouse.x;
    int y = pos_mouse.y;

    if (x >= 0 && x < (int) config.x_max && y >= 0 && y < (int) config.y_max) {
      uint64_t ind = x + config.x_max * y;
      const auto& cell = ctx.cells[ind];
      if (cell.alive) {
        stats_text += "\n family " + std::to_string(cell.family);
        stats_text += "\n age " + std::to_string(cell.age);
        for (size_t ind{}; ind < config.resources.size(); ++ind) {
          if (cell.resources_microbe[ind]) {
            stats_text += "\n resource " + config.resources[ind].name;
            stats_text += " " + std::to_string(cell.resources_microbe[ind]);
            stats_text += " / " + std::to_string(config.resources[ind].stack_size);
          }
        }
      }
      for (size_t ind{}; ind < config.resources.size(); ++ind) {
        if (cell.resources_world[ind]) {
          stats_text += "\n area " + config.resources[ind].name;
          stats_text += " " + std::to_string(cell.resources_world[ind]);
          stats_text += " / " + std::to_string(config.resources[ind].stack_size);
        }
      }
    }

    if (!debug_text.empty()) {
      stats_text += "\n debug: " + debug_text;
    }
  }

  void draw_data() {
    if (!ctx.valid) {
      return;
    }

    window.clear(sf::Color(sfml_config.color_background));
    window.setView(view_world);

    const auto& config = ctx.config;

    {
      sf::RectangleShape rectangle;
      rectangle.setSize(sf::Vector2f(config.x_max, config.y_max));
      rectangle.setPosition(sf::Vector2f(0, 0));
      rectangle.setFillColor(sf::Color(sfml_config.color_area_available));
      window.draw(rectangle);
    }

    for (const auto& cell : ctx.cells) {
      sf::Color color;
      auto c1 = sf::Color(sfml_config.color_min);
      auto c2 = sf::Color(sfml_config.color_max);
      switch (mode) {
        case MODE_AGE: {
          if (!cell.alive) continue;
          double r = 1. * cell.age / (config.age_max + config.age_max_delta);
          color = sf::Color(
              c1.r * (1 - r) + c2.r * r,
              c1.g * (1 - r) + c2.g * r,
              c1.b * (1 - r) + c2.b * r,
              255);
          break;

        } case MODE_AREA: {
          mode_param %= config.resources.size();
          if (!cell.resources_world[mode_param]) continue;
          double r = 1. * cell.resources_world[mode_param] / config.resources[mode_param].stack_size;
          color = sf::Color(
              c1.r * (1 - r) + c2.r * r,
              c1.g * (1 - r) + c2.g * r,
              c1.b * (1 - r) + c2.b * r,
              255);
          break;

        } case MODE_RESOURCE: {
          if (!cell.alive) continue;
          mode_param %= config.resources.size();
          if (!cell.resources_microbe[mode_param]) continue;
          double r = 1. * cell.resources_microbe[mode_param] / config.resources[mode_param].stack_size;
          color = sf::Color(
              c1.r * (1 - r) + c2.r * r,
              c1.g * (1 - r) + c2.g * r,
              c1.b * (1 - r) + c2.b * r,
              255);
          break;

        } case MODE_FAMILY: {
          if (!cell.alive) continue;
          color = sf::Color(cell.family, cell.family >> 8, cell.family >> 16, 255);
          break;

        } default: {
          if (!cell.alive) continue;
          color = sf::Color(sfml_config.color_min);
        }
      }
      sf::RectangleShape rectangle;
      rectangle.setSize(sf::Vector2f(1, 1));
      rectangle.setPosition(sf::Vector2f(cell.pos.first, cell.pos.second));
      rectangle.setFillColor(color);
      window.draw(rectangle);
    }

    {
      window.setView(view_text);

      sf::Text text;
      text.setFont(font);
      text.setString(stats_text);
      text.setCharacterSize(sfml_config.text_size);
      text.setFillColor(sf::Color::Black);
      window.draw(text);

      window.setView(view_world);
    }

    window.display();
  }

};

int main(int argc, char* argv[]) {

  if (argc <= 2) {
    std::cerr << "usage: " << (argc > 0 ? argv[0] : "<program>")
        << " <config.json> <world.json>" << std::endl;
    return -1;
  }

  std::error_code ec;
  std::string config_file_name = std::filesystem::absolute(argv[1], ec);

  if (ec) {
    std::cerr << "invalid path: " << ec.message().c_str() << std::endl;
    return -1;
  }

  std::string world_file_name = std::filesystem::absolute(argv[2], ec);

  if (ec) {
    std::cerr << "invalid path: " << ec.message().c_str() << std::endl;
    return -1;
  }

  genesis_sfml_t context;

  context.config_file_name = config_file_name;
  context.world_file_name  = world_file_name;

  context.init();
  context.loop();
  context.deinit();

  return 0;
}
