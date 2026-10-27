#include "InAgent.h"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -h HOST       server host (default %s)\n"
        "  -p PORT       server port (default %d)\n"
        "  -i INTERVAL   metrics interval sec (default %d)\n"
        "  -d DEVICE_ID  device id (default %s)\n"
        "  -t TYPE       device type (default %s)\n"
        "  -f FIRMWARE   firmware version (default %s)\n"
        "  -v            verbose (DEBUG)\n",
        prog,
        DEFAULT_SERVER_HOST, DEFAULT_SERVER_PORT, DEFAULT_INTERVAL,
        DEFAULT_DEVICE_ID, DEFAULT_TYPE, DEFAULT_FIRMWARE);
}

int main(int argc, char **argv) {
    openlog("netwatch-agent", LOG_PID | LOG_CONS, LOG_USER);
    setlogmask(LOG_UPTO(LOG_INFO));

    agent_t agent;
    agent_init(&agent);

    int opt;
    while ((opt = getopt(argc, argv, "h:p:i:d:t:f:v")) != -1) {
        switch (opt) {
            case 'h':
                strncpy(agent.host, optarg, sizeof(agent.host) - 1);
                break;
            case 'p':
                agent.port = atoi(optarg);
                break;
            case 'i':
                agent.interval = atoi(optarg);
                if (agent.interval < 1) agent.interval = 1;
                break;
            case 'd':
                strncpy(agent.device_id, optarg, sizeof(agent.device_id) - 1);
                break;
            case 't':
                strncpy(agent.type, optarg, sizeof(agent.type) - 1);
                break;
            case 'f':
                strncpy(agent.firmware, optarg, sizeof(agent.firmware) - 1);
                break;
            case 'v':
                setlogmask(LOG_UPTO(LOG_DEBUG));
                break;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    syslog(LOG_INFO,
           "NetWatch agent starting: id=%s type=%s fw=%s -> %s:%d interval=%d",
           agent.device_id, agent.type, agent.firmware,
           agent.host, agent.port, agent.interval);

    time_t last_metrics = 0;
    time_t last_hb = 0;

    while (!g_stop) {
        /* reconnect / reboot */
        if (agent.sock < 0 || agent.need_reboot) {
            if (agent.need_reboot) {
                syslog(LOG_INFO, "Performing reboot (disconnect + re-register)");
                agent_cleanup(&agent);
                agent.need_reboot = 0;
                agent.uptime_start = time(NULL);
                sleep(2);
            }

            if (agent_connect(&agent) != OK) {
                syslog(LOG_WARNING, "Connect failed, retry in 5s");
                sleep(5);
                continue;
            }
            if (agent_register(&agent) != OK) {
                syslog(LOG_WARNING, "Register failed, retry in 5s");
                agent_cleanup(&agent);
                sleep(5);
                continue;
            }
            last_metrics = 0;
            last_hb = 0;
        }

        /* select with 1s timeout */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(agent.sock, &rfds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int sel = select(agent.sock + 1, &rfds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "select: %s", strerror(errno));
            agent_cleanup(&agent);
            continue;
        }

        if (sel > 0 && FD_ISSET(agent.sock, &rfds)) {
            if (agent_process_incoming(&agent) != OK) {
                syslog(LOG_WARNING, "Connection lost");
                agent_cleanup(&agent);
                continue;
            }
        }

        time_t now = time(NULL);

        if (now - last_hb >= HEARTBEAT_INTERVAL) {
            if (agent_send_heartbeat(&agent) != OK) {
                syslog(LOG_WARNING, "Heartbeat send failed");
                agent_cleanup(&agent);
                continue;
            }
            last_hb = now;
        }

        if (now - last_metrics >= agent.interval) {
            if (agent_send_metrics(&agent) != OK) {
                syslog(LOG_WARNING, "Metrics send failed");
                agent_cleanup(&agent);
                continue;
            }
            last_metrics = now;
        }
    }

    syslog(LOG_INFO, "Agent shutting down");
    agent_cleanup(&agent);
    closelog();
    return 0;
}