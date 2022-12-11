# COMPILER OPTIONS
CC := gcc
FLAGS := -O3 -std=c17 -pedantic -Wall -Werror -Wuninitialized -Wshadow -Wwrite-strings -Wconversion -Wunreachable-code -D_POSIX_SOURCE
FLAGS_DEBUG := -O0 -g -DDEBUG -std=c17 -pedantic -Wall -Werror -Wuninitialized -Wshadow -Wwrite-strings -Wconversion -Wunreachable-code -D_POSIX_SOURCE

# DIRECTORIES
SRC_DIR := src
SRC_FILES := ${wildcard ${SRC_DIR}/*.c}

OBJ_DIR := obj
OBJ_FILES := ${patsubst ${SRC_DIR}/%.c, ${OBJ_DIR}/%.o, ${SRC_FILES}}

BIN_DIR := bin
BIN_FILES := ${BIN_DIR}/main

TEST_DIR := tests
TEST_FILES := ${wildcard ${TEST_DIR}/*.c}

# TARGETS
.SILENT: dirs build run clean debug

.PHONY: all
all: clean dirs build run

.PHONY: debug
debug: dirs clean
	${CC} ${FLAGS_DEBUG} ${SRC_FILES} -o ${BIN_FILES}

# MAKE DIRECTORIES
dirs:
	-@ mkdir ${SRC_DIR} ${OBJ_DIR} ${BIN_DIR} ${TEST_DIR} 2>/dev/null || true

# RUN THE EXECUTABLE
run: build
	./${BIN_FILES}

# COMPILE OBJECT FILES INTO EXECUTABLE
build: ${OBJ_FILES}
	${CC} ${FLAGS} ${OBJ_FILES} -o ${BIN_FILES}

# COMPILE THE SOURCE FILES 
${OBJ_DIR}/%.o: ${SRC_DIR}/%.c
	@${CC} ${FLAGS} -c $< -o $@

# REMOVE TMP FILES
.PHONY: clean
clean:
	rm -r ${BIN_DIR}/* ${OBJ_DIR}/* 2>/dev/null || true