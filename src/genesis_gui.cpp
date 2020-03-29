
#include <SFML/Graphics.hpp>
#include <iostream>
#include <atomic>
#include <mutex>
#include <deque>
#include "genesis.h"

using namespace genesis_n;

struct sfml_config_t {
  std::string   font_path              = "./resources/fonts/Roboto-Regular.ttf";
  float         win_x                  = 600;
  float         win_y                  = 600;
  float         zoom                   = 1.05;
  uint64_t      fps                    = 30;
  uint32_t      text_size              = 18;
  uint32_t      color_background       = 0xE0E0E0FF;
  uint32_t      color_area_available   = 0xD0D0D0FF;
  uint32_t      color_area             = 0x90EE9050;
  uint32_t      color_microbe          = 0x000080FF;
};


struct genesis_sfml_t {
  enum mode_t : uint64_t {
    MODE_NORMAL,
    MODE_AGE,
    MODE_RESOURCE,
    MODE_FAMILY,
    MODE_COUNT,
  };

  struct microbe_t {
    using resources_t   = std::map<size_t/*ind*/, int64_t/*count*/>;
    using xy_pos_t      = utils_t::xy_pos_t;

    bool          alive;
    uint64_t      family;
    resources_t   resources;
    xy_pos_t      pos;
    uint64_t      age;
    uint64_t      direction;
  };

  struct area_pos_t {
    utils_t::xy_pos_t pos;
    uint64_t r;
  };

  using microbes_t = std::deque<microbe_t>;
  using areas_t    = std::deque<area_pos_t>;

  sfml_config_t       config           = {};

  std::atomic<bool>   stop             = false;
  std::atomic<bool>   pause            = false;
  std::timed_mutex    mutex_world      = {};
  std::thread         thread           = {};

  microbes_t          microbes         = {};
  areas_t             areas            = {};

  bool                showing_help     = false;
  std::string         stats_text       = {};

  world_t             world            = {};

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
    world.init();

    window.create(sf::VideoMode(config.win_x, config.win_y),
        "Genesis", sf::Style::Close | sf::Style::Resize);

    window.setFramerateLimit(config.fps);

    view_world.setCenter(config.win_x / 2, config.win_y / 2);

    if (!font.loadFromFile(config.font_path)) {
      throw std::runtime_error("can not load font");
    }

    thread = std::move(std::thread([this] { this->update(); }));
  }

  void deinit() {
    stop = true;
    thread.join();
    world.save_data();
  }

  void loop() {
    while (window.isOpen()) {
      check_events();
      get_data();
      draw_data();
    }
  }

  void update() {
    while (!stop) {
      if (!pause && mutex_world.try_lock_for(std::chrono::milliseconds(1))) {
        world.update();
        mutex_world.unlock();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

          float zoom = config.zoom;
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
              const std::lock_guard<std::timed_mutex> lock(mutex_world);
              world.save_data();
              world.load_config();
              world.load_data();
              break;

            } case sf::Keyboard::H: {
              showing_help = !showing_help;
              break;

            } case sf::Keyboard::Space: {
              pause = !pause;
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
    const std::lock_guard<std::timed_mutex> lock(mutex_world);

    areas = {};
    for (const auto& areas_v : world.config.areas) {
      for (const auto& area : areas_v) {
        areas.push_back({area.pos, area.radius});
      }
    }

    microbes = {};
    for (size_t i{}; i < world.microbes.size(); ++i) {
      if (world.microbes[i].alive) {
        microbes.push_back({});
        auto& microbe = microbes.back();
        microbe.alive       = world.microbes[i].alive;
        microbe.family      = world.microbes[i].family;
        microbe.resources   = world.microbes[i].resources;
        microbe.pos         = world.microbes[i].pos;
        microbe.age         = world.microbes[i].age;
        microbe.direction   = world.microbes[i].direction;
      }
    }

    static std::string help_text =
      "\n U - reload config"
      "\n M - change mode"
      "\n P - change mode_param"
      "\n Q - quit";

    std::string mode_text{};
    switch (mode) {
      case MODE_NORMAL:   mode_text = "normal"; break;
      case MODE_AGE:      mode_text = "age"; break;
      case MODE_RESOURCE: mode_text = "resource "
                              + world.config.resources[mode_param % world.config.resources.size()].name; break;
      case MODE_FAMILY:   mode_text = "family"; break;
      default:            mode_text = "unknown"; break;
    }

    const auto& stats = world.stats;
    stats_text = std::string{}
      + " H - show help"
      + (showing_help ? help_text : "") + "\n"
      + (pause ? "\n PAUSE" : "")
      + "\n mode: " + mode_text
      + "\n pos: " + std::to_string(int(pos_mouse.x)) + "\t" + std::to_string(int(pos_mouse.y))
      + "\n age: " + std::to_string(stats.age)
      + "\n microbes_count: " + std::to_string(stats.microbes_count)
      + "\n microbes_age_avg: " + std::to_string((uint64_t) stats.microbes_age_avg)
      + "\n time_update: " + std::to_string(stats.time_update)
      + "\n bpms: " + std::to_string(uint64_t (stats.microbes_count / std::max(1UL, stats.time_update)))
      + "";
  }

  void draw_data() {
    window.clear(sf::Color(config.color_background));
    window.setView(view_world);

    {
      sf::RectangleShape rectangle;
      rectangle.setSize(sf::Vector2f(world.config.x_max, world.config.y_max));
      rectangle.setPosition(sf::Vector2f(0, 0));
      rectangle.setFillColor(sf::Color(config.color_area_available));
      window.draw(rectangle);
    }

    for (const auto& [pos, r] : areas) {
      sf::CircleShape circle;
      circle.setRadius(r);
      circle.setPointCount(1000);
      circle.setPosition(pos.first + 0.5, pos.second + 0.5);
      circle.setOrigin(r, r);
      circle.setFillColor(sf::Color(config.color_area));
      window.draw(circle);
    }

    for (auto& microbe : microbes) {
      sf::RectangleShape rectangle;
      rectangle.setSize(sf::Vector2f(1, 1));
      rectangle.setPosition(sf::Vector2f(microbe.pos.first, microbe.pos.second));
      sf::Color color;
      switch (mode) {
        case MODE_AGE: {
          double ratio = 255. * microbe.age / world.config.age_max;
          color = sf::Color(255, ratio, 0, 255);
          break;
        } case MODE_RESOURCE: {
          mode_param %= world.config.resources.size();
          double ratio = 255. * microbe.resources[mode_param] / world.config.resources[mode_param].stack_size;
          color = sf::Color(255, ratio, 0, 255);
          break;
        } case MODE_FAMILY: {
          color = sf::Color(microbe.family, microbe.family >> 8, microbe.family >> 16, 255);
          break;
        } default:       color = sf::Color(config.color_microbe);
      }
      rectangle.setFillColor(color);
      window.draw(rectangle);
    }

    {
      window.setView(view_text);

      sf::Text text;
      text.setFont(font);
      text.setString(stats_text);
      text.setCharacterSize(config.text_size);
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

  context.world.config_file_name = config_file_name;
  context.world.world_file_name  = world_file_name;

  context.init();
  context.loop();
  context.deinit();

  return 0;
}
