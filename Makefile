
all:
	g++ -std=c++2a -o genesis src/genesis_gui.cpp \
		-lsfml-graphics -lsfml-window -lsfml-system \
		-lpthread \
		-O2 -Wall -Wextra -Werror -pedantic
	g++ -std=c++2a -o genesis_console src/genesis_console.cpp \
		-O2 -Wall -Wextra -Werror -pedantic

