
#include <SFML/Graphics.hpp>
#include <iostream>
#include <atomic>
#include <mutex>
#include "genesis.h"

struct sfml_window_t {
};

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

  std::atomic<bool> stop = false;
  std::timed_mutex mutex_world;

  struct area_pos_t {
    utils_t::xy_pos_t pos;
    uint64_t r;
  };

  std::list<utils_t::xy_pos_t> microbes;
  std::list<area_pos_t> areas;
  std::string stats_text = {};

  microbes = {};
  for (const auto& microbe : world.ctx.microbes) {
    microbes.push_back(microbe.pos);
  }

  areas = {};
  for (const auto& [_, area] : world.ctx.config.areas) {
    areas.push_back({area.pos, area.radius});
  }

  std::thread thread_update([&stop, &world, &mutex_world](){
    while (!stop) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      const std::lock_guard<std::timed_mutex> lock(mutex_world);
      world.update();
    }
  });

  const auto& config = world.ctx.config;

  const float size_x = config.x_max;
  const float size_y = config.y_max;

  sf::RenderWindow window(sf::VideoMode(size_x, size_y), "Genesis", sf::Style::Close | sf::Style::Resize);
  window.setFramerateLimit(60);

  sf::View view_world;
  sf::View view_text;

  sf::Font font;
  std::string font_path = "./resources/fonts/Roboto-Regular.ttf";
  if (!font.loadFromFile(font_path)) {
    std::cerr << "ERROR: can not load font" << std::endl;
    return -1;
  }

  while (window.isOpen()) {
    sf::Event event;
    while (window.pollEvent(event)) {

      switch (event.type) {
        case sf::Event::Closed: {
          window.close();
          break;

        } case sf::Event::Resized: {
          double aspect_ratio = double(window.getSize().x) / window.getSize().y;

          view_world.setCenter(window.getSize().x/2, window.getSize().y/2);
          view_world.setSize(window.getSize().x, window.getSize().x / aspect_ratio);

          view_text.setCenter(window.getSize().x/2, window.getSize().y/2);
          view_text.setSize(window.getSize().x, window.getSize().x / aspect_ratio);
          break;
        }
        default: break;
      }
    }

    if (window.hasFocus()) {
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) {
        window.close();
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Add)) {
        view_world.zoom(0.9);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Subtract)) {
        view_world.zoom(1.1);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
        view_world.move(-10, 0);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
        view_world.move(10, 0);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
        view_world.move(0, -10);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
        view_world.move(0, 10);
      }
    }

    if (mutex_world.try_lock_for(std::chrono::milliseconds(1))) {
      // std::cout << "try_lock ok" << std::endl;

      areas = {};
      for (const auto& [_, area] : world.ctx.config.areas) {
        areas.push_back({area.pos, area.radius});
      }

      microbes = {};
      for (const auto& microbe : world.ctx.microbes) {
        microbes.push_back(microbe.pos);
      }

      const auto& stats = world.ctx.stats;
      stats_text = std::string{} + "stats:"
        + "\n\tage: " + std::to_string(stats.age)
        + "\n\tmicrobes_count: " + std::to_string(stats.microbes_count)
        + "";

      mutex_world.unlock();
    } else {
      // std::cout << "try_lock failed" << std::endl;
    }

    window.clear(sf::Color(240, 240, 240));
    window.setView(view_world);

    {
      sf::RectangleShape rectangle;
      rectangle.setSize(sf::Vector2f(world.ctx.config.x_max, world.ctx.config.y_max));
      rectangle.setPosition(sf::Vector2f(0, 0));
      rectangle.setFillColor(sf::Color(200, 200, 200));
      window.draw(rectangle);
    }

    for (const auto& [pos, r] : areas) {
      sf::CircleShape circle;
      circle.setRadius(r);
      circle.setPointCount(1000);
      circle.setPosition(pos.first + 0.5, pos.second + 0.5);
      circle.setOrigin(r, r);
      circle.setFillColor(sf::Color(0, 200, 200, 50));
      window.draw(circle);
    }

    for (const auto& [x, y] : microbes) {
      sf::RectangleShape rectangle;
      rectangle.setSize(sf::Vector2f(1, 1));
      rectangle.setPosition(sf::Vector2f(x, y));
      rectangle.setFillColor(sf::Color(0, 0, 255));
      window.draw(rectangle);
    }

    {
      window.setView(view_text);

      sf::Text text;
      text.setFont(font);
      text.setString(stats_text);
      text.setCharacterSize(24);
      text.setFillColor(sf::Color::Black);
      window.draw(text);

      window.setView(view_world);
    }

    window.display();
  }

  stop = true;

  thread_update.join();

  world.save();

  return 0;
}
