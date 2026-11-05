CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -Iinclude
LDFLAGS = -ljansson -lpq -lm

BINDIR = bin
AGENT = $(BINDIR)/agent
SERVER = $(BINDIR)/server
OPERATOR = $(BINDIR)/operator

AGENT_SRCS = agent/src/main.c agent/src/agent.c agent/src/frame.c \
               agent/src/register.c agent/src/metrics.c agent/src/heartbeat.c \
               agent/src/command.c
SERVER_SRCS = server/src/main.c server/src/server.c server/src/frame.c \
               server/src/handlers.c server/src/db.c
OPERATOR_SRCS = operator/src/main.c operator/src/operator.c operator/src/frame.c

.PHONY: all clean agent server operator test dirs

all: dirs $(AGENT) $(SERVER) $(OPERATOR)

dirs:
	@mkdir -p $(BINDIR)

$(AGENT): $(AGENT_SRCS) include/library.h include/InAgent.h
	$(CC) $(CFLAGS) -o $@ $(AGENT_SRCS) $(LDFLAGS)

$(SERVER): $(SERVER_SRCS) include/library.h include/InServer.h
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRCS) $(LDFLAGS)

$(OPERATOR): $(OPERATOR_SRCS) include/library.h include/InOperator.h
	$(CC) $(CFLAGS) -o $@ $(OPERATOR_SRCS) $(LDFLAGS)

agent: dirs $(AGENT)
server: dirs $(SERVER)
operator: dirs $(OPERATOR)

clean:
	rm -rf $(BINDIR)

test:
	@echo "Build assembled. Manually:"
	@echo "  docker compose up -d"
	@echo "  ./bin/server &"
	@echo "  ./bin/agent -d sw-001 &"
	@echo "  ./bin/operator"