CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -I src

TEST_BIN := tests/test_runner
TEST_SRCS := tests/test_runner.c src/gfx/gfx.c

.PHONY: test clean

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS)
	$(CC) $(CFLAGS) $(TEST_SRCS) -o $(TEST_BIN)

clean:
	rm -f $(TEST_BIN) reader_sim
	rm -rf out
