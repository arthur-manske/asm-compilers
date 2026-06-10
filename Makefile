CC      = clang
CFLAGS  = -std=c99 -Isrc

LLVM_CFLAGS  := $(shell llvm-config --cflags)
LLVM_LDFLAGS := $(shell llvm-config --ldflags)
LLVM_LIBS    := $(shell llvm-config --libs)
LLVM_SYS     := $(shell llvm-config --system-libs)

LIBFYAML_CFLAGS := $(shell pkg-config --cflags libfyaml)
LIBFYAML_LIBS   := $(shell pkg-config --libs libfyaml)

CFLAGS  += $(LLVM_CFLAGS) $(LIBFYAML_CFLAGS)
LDFLAGS += $(LLVM_LDFLAGS) $(LLVM_LIBS) $(LLVM_SYS) $(LIBFYAML_LIBS)

RE2C = re2c

SRCS := $(shell find src -name "*.c")
LEX_RE := $(wildcard src/c/lexer/*.re)
LEX_C  := $(LEX_RE:.re=.re.c)

SRCS += $(LEX_C)

OBJS := $(SRCS:.c=.o)

TARGET = dpp

all: $(TARGET)

src/c/lexer/lexer.re.c: 
	$(RE2C) -o src/c/lexer/lexer.re.c src/c/lexer/lexer.re

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(LEX_C)

TEST_SRC = ./hello-world.c
TEST_OUT = ./hello-world.elf


TEST_SRC  = tests/hello-world.c
TEST_BIN  = tests/hello-world.out
TEST_EXP  = Hello, World!

test: $(TARGET)
	./$(TARGET) $(TEST_SRC) -o $(TEST_BIN)
	@OUTPUT=$$($(TEST_BIN)); \
	if [ "$$OUTPUT" = "$(TEST_EXP)" ]; then \
		echo "TEST OK"; \
	else \
		echo "TEST FAIL"; \
		echo "EXPECTED: $(TEST_EXP)"; \
		echo "GOT: $$OUTPUT"; \
		exit 1; \
	fi

test_declarator: tests/test_declarator.o $(filter-out src/main.o, $(OBJS))
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: all clean test test_declarator
