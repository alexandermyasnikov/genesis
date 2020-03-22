
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

  microbes = {};
  for (const auto& [key, _] : world.ctx.microbes) {
    microbes.push_back(key);
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

  sf::RenderWindow window(sf::VideoMode(800, 800), "Genesis", sf::Style::Close | sf::Style::Resize);
  window.setFramerateLimit(60);

  const auto& config = world.ctx.config;

  sf::View view(sf::FloatRect(0, 0, 800, 800));

  while (window.isOpen()) {
    sf::Event event;
    while (window.pollEvent(event)) {

      switch (event.type) {
        case sf::Event::Closed: {
          window.close();
          break;

        } case sf::Event::Resized: {
          double aspect_ratio = double(window.getSize().x) / window.getSize().y;
          view.setSize(config.x_max * aspect_ratio, config.x_max);
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
        view.zoom(0.9);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Subtract)) {
        view.zoom(1.1);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
        view.move(-10, 0);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
        view.move(10, 0);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
        view.move(0, -10);
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
        view.move(0, 10);
      }
    }

    if (mutex_world.try_lock_for(std::chrono::milliseconds(1))) {
      std::cout << "try_lock ok" << std::endl;

      areas = {};
      for (const auto& [_, area] : world.ctx.config.areas) {
        areas.push_back({area.pos, area.radius});
      }

      microbes = {};
      for (const auto& [key, _] : world.ctx.microbes) {
        microbes.push_back(key);
      }

      mutex_world.unlock();
    } else {
      // std::cout << "try_lock failed" << std::endl;
    }

    window.clear(sf::Color(240, 240, 240));
    window.setView(view);

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

    window.display();
  }

  stop = true;

  thread_update.join();

  world.save();

  return 0;
}