
all: console gui

gui:
	g++ -std=c++2a -o genesis_gui src/genesis_gui.cpp \
		-lsfml-graphics -lsfml-window -lsfml-system \
		-lpthread \
		-fconcepts -O2 -Wall -Wextra -Werror -pedantic

console:
	g++ -std=c++2a -o genesis_console src/genesis_console.cpp \
		-fconcepts -O2 -Wall -Wextra -Werror -pedantic

