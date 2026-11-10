CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -g -Iinclude -D_GNU_SOURCE
LDFLAGS  = -ljansson -lpq -lm

BINDIR   = bin
AGENT    = $(BINDIR)/agent
SERVER   = $(BINDIR)/server
OPERATOR = $(BINDIR)/operator

AGENT_SRCS = agent/src/main.c agent/src/agent.c agent/src/frame.c \
             agent/src/register.c agent/src/metrics.c agent/src/heartbeat.c \
             agent/src/command.c

SERVER_SRCS = server/src/main.c server/src/Server_main.c \
              server/src/Register_ack.c server/src/Heartbeat_ack.c \
              server/src/Query_result.c server/src/Command.c server/src/Alert.c \
              server/src/up_or_lower_amount_size.c server/src/db.c

OPERATOR_SRCS = operator/src/main.c operator/src/operator_main.c \
                operator/src/frame.c operator/src/list_device_and_state.c \
                operator/src/device_detail.c operator/src/metrics_history.c \
                operator/src/allert_journal.c operator/src/sending_command_device.c

.PHONY: all clean dirs

all: dirs $(AGENT) $(SERVER) $(OPERATOR)

dirs:
	@mkdir -p $(BINDIR)

$(AGENT): $(AGENT_SRCS) include/library.h include/InAgent.h
	$(CC) $(CFLAGS) -o $@ $(AGENT_SRCS) $(LDFLAGS)

$(SERVER): $(SERVER_SRCS) include/library.h include/InServer.h
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRCS) $(LDFLAGS)

$(OPERATOR): $(OPERATOR_SRCS) include/library.h include/InOperator.h
	$(CC) $(CFLAGS) -o $@ $(OPERATOR_SRCS) $(LDFLAGS)

clean:
	rm -rf $(BINDIR)