CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -I src -I assets
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

TEST_BIN := tests/test_runner
TEST_SRCS := tests/test_runner.c src/gfx/gfx.c src/platform/sim_display.c src/platform/sdl_display.c src/app/app_state.c src/ui/pages.c src/font/font.c
SIM_SRCS := src/main.c src/gfx/gfx.c src/platform/sim_display.c src/app/app_state.c src/ui/pages.c src/font/font.c
SDL_SIM_SRCS := src/main_sdl.c src/gfx/gfx.c src/platform/sdl_display.c src/app/app_state.c src/ui/pages.c src/font/font.c

.PHONY: test clean

test: $(TEST_BIN)
	./$(TEST_BIN)

reader_sim: $(SIM_SRCS)
	$(CC) $(CFLAGS) $(SIM_SRCS) -o reader_sim

reader_sim_sdl: $(SDL_SIM_SRCS)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(SDL_SIM_SRCS) $(SDL_LIBS) -o reader_sim_sdl

$(TEST_BIN): $(TEST_SRCS)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(TEST_SRCS) $(SDL_LIBS) -o $(TEST_BIN)

clean:
	rm -f $(TEST_BIN) reader_sim reader_sim_sdl
	rm -rf out
