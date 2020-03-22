
all:
	g++ -std=c++2a -o genesis src/genesis_gui.cpp -lsfml-graphics -lsfml-window -lsfml-system -lpthread -O0 -Wall -Wextra -Werror -pedantic

