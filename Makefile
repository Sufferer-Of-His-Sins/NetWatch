CC = gcc
CFLAGS   = -Wall -Wextra -g -O0 -Iinclude -I.
LDFLAGS  = -ljansson -lpq

# Папки
BIN_DIR  = bin
AGENT_DIR = agent/src
SERVER_DIR = server/src
OP_DIR = operator/src

# Бинарники
AGENT_BIN = $(BIN_DIR)/agent
SERVER_BIN = $(BIN_DIR)/server
OPERATOR_BIN= $(BIN_DIR)/operator

# ---------- Исходники ----------
AGENT_SRC = $(wildcard $(AGENT_DIR)/*.c)
SERVER_SRC= $(wildcard $(SERVER_DIR)/*.c)
OP_SRC = $(wildcard $(OP_DIR)/*.c)

# Объектники
AGENT_OBJ = $(AGENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)
OP_OBJ = $(OP_SRC:.c=.o)

.PHONY: all clean dirs agent server operator run-db stop-db

all: dirs $(AGENT_BIN) $(SERVER_BIN) $(OPERATOR_BIN)

dirs:
	@mkdir -p $(BIN_DIR)

# ---------- Сборка ----------
$(AGENT_BIN): $(AGENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "→ agent built"

$(SERVER_BIN): $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "→ server built"

$(OPERATOR_BIN): $(OP_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "→ operator built"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---------- Утилиты ----------
clean:
	rm -f $(AGENT_OBJ) $(SERVER_OBJ) $(OP_OBJ)
	rm -f $(AGENT_BIN) $(SERVER_BIN) $(OPERATOR_BIN)
	@echo "cleaned"

run-db:
	docker compose up -d
	@echo "PostgreSQL started (port 5432)"

stop-db:
	docker compose down