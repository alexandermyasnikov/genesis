
#include <iostream>
#include "genesis.h"

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

  using namespace genesis_n;

  world_t world;
  world.config_file_name = config_file_name;
  world.world_file_name = world_file_name;
  world.init();

  for (size_t i{}; i < 1000; ++i) {
    world.update();
  }

  return 0;
}
