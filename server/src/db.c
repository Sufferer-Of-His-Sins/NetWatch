#include "InServer.h"

static PGconn *g_db = NULL;
static char g_conninfo[256];

int db_connect(void) {
    if (g_db) {
        PQfinish(g_db);
        g_db = NULL;
    }

    const char *host = getenv("NETWATCH_DB_HOST");
    const char *port = getenv("NETWATCH_DB_PORT");
    const char *user = getenv("NETWATCH_DB_USER");
    const char *pass = getenv("NETWATCH_DB_PASSWORD");
    const char *name = getenv("NETWATCH_DB_NAME");

    snprintf(g_conninfo, sizeof(g_conninfo),
             "host=%s port=%s dbname=%s user=%s password=%s",
             host ? host : "127.0.0.1",
             port ? port : "5432",
             name ? name : "netwatch",
             user ? user : "netwatch",
             pass ? pass : "netwatch");

    g_db = PQconnectdb(g_conninfo);
    if (PQstatus(g_db) != CONNECTION_OK) {
        syslog(LOG_ERR, "DB connect failed: %s", PQerrorMessage(g_db));
        PQfinish(g_db);
        g_db = NULL;
        return ERR;
    }
    syslog(LOG_INFO, "Connected to PostgreSQL");
    return OK;
}

void db_disconnect(void) {
    if (g_db) {
        PQfinish(g_db);
        g_db = NULL;
    }
}

int db_ensure(void) {
    if (g_db && PQstatus(g_db) == CONNECTION_OK)
        return OK;
    return db_connect();
}

int db_upsert_device(const char *device_id, const char *type,
                     const char *firmware, int online, time_t last_seen) {
    if (db_ensure() != OK) return ERR;

    const char *sql =
        "INSERT INTO devices (device_id, type, firmware, online, last_seen) "
        "VALUES ($1, $2, $3, $4, to_timestamp($5)) "
        "ON CONFLICT (device_id) DO UPDATE SET "
        "type = EXCLUDED.type, firmware = EXCLUDED.firmware, "
        "online = EXCLUDED.online, last_seen = EXCLUDED.last_seen";

    char online_s[2] = { online ? 't' : 'f', 0 };
    char ts_s[32];
    snprintf(ts_s, sizeof(ts_s), "%ld", (long)last_seen);

    const char *vals[5] = {
        device_id,
        type ? type : "",
        firmware ? firmware : "",
        online_s,
        ts_s
    };

    PGresult *res = PQexecParams(g_db, sql, 5, NULL, vals, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_ERR, "db_upsert_device: %s", PQerrorMessage(g_db));
        PQclear(res);
        return ERR;
    }
    PQclear(res);
    return OK;
}

int db_insert_metrics(const char *device_id, time_t ts,
                      double cpu, double ram, long long uptime, json_t *ports) {
    if (db_ensure() != OK) return ERR;

    const char *sql =
        "INSERT INTO metrics (device_id, ts, cpu, ram, uptime, ports) "
        "VALUES ($1, to_timestamp($2), $3, $4, $5, $6::jsonb)";

    char ts_s[32], cpu_s[32], ram_s[32], up_s[32];
    snprintf(ts_s, sizeof(ts_s), "%ld", (long)ts);
    snprintf(cpu_s, sizeof(cpu_s), "%.2f", cpu);
    snprintf(ram_s, sizeof(ram_s), "%.2f", ram);
    snprintf(up_s, sizeof(up_s), "%lld", uptime);

    char *ports_str = ports ? json_dumps(ports, JSON_COMPACT) : strdup("[]");
    if (!ports_str) return ERR;

    const char *vals[6] = { device_id, ts_s, cpu_s, ram_s, up_s, ports_str };
    PGresult *res = PQexecParams(g_db, sql, 6, NULL, vals, NULL, NULL, 0);
    free(ports_str);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_ERR, "db_insert_metrics: %s", PQerrorMessage(g_db));
        PQclear(res);
        return ERR;
    }
    PQclear(res);
    return OK;
}

int db_insert_alert(const char *device_id, const char *kind, json_t *details) {
    if (db_ensure() != OK) return ERR;

    const char *sql =
        "INSERT INTO alerts (device_id, kind, details) VALUES ($1, $2, $3::jsonb)";

    char *det = details ? json_dumps(details, JSON_COMPACT) : strdup("{}");
    if (!det) return ERR;

    const char *vals[3] = { device_id ? device_id : "", kind, det };
    PGresult *res = PQexecParams(g_db, sql, 3, NULL, vals, NULL, NULL, 0);
    free(det);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        syslog(LOG_ERR, "db_insert_alert: %s", PQerrorMessage(g_db));
        PQclear(res);
        return ERR;
    }
    PQclear(res);
    return OK;
}

json_t *db_list_devices(void) {
    if (db_ensure() != OK) return NULL;

    PGresult *res = PQexec(g_db,
        "SELECT device_id, type, firmware, online, "
        "EXTRACT(EPOCH FROM last_seen)::bigint "
        "FROM devices ORDER BY device_id");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        syslog(LOG_ERR, "db_list_devices: %s", PQerrorMessage(g_db));
        PQclear(res);
        return NULL;
    }

    json_t *arr = json_array();
    int n = PQntuples(res);
    for (int i = 0; i < n; i++) {
        json_t *obj = json_object();
        json_object_set_new(obj, "device_id", json_string(PQgetvalue(res, i, 0)));
        json_object_set_new(obj, "type", json_string(PQgetvalue(res, i, 1)));
        json_object_set_new(obj, "firmware", json_string(PQgetvalue(res, i, 2)));
        json_object_set_new(obj, "online",
            (PQgetvalue(res, i, 3)[0] == 't') ? json_true() : json_false());
        json_object_set_new(obj, "last_seen",
            json_integer(atoll(PQgetvalue(res, i, 4))));
        json_array_append_new(arr, obj);
    }
    PQclear(res);
    return arr;
}

json_t *db_device_details(const char *device_id) {
    if (db_ensure() != OK) return NULL;

    const char *sql =
        "SELECT m.cpu, m.ram, m.uptime, m.ports, "
        "EXTRACT(EPOCH FROM m.ts)::bigint, d.type, d.firmware "
        "FROM metrics m JOIN devices d ON d.device_id = m.device_id "
        "WHERE m.device_id = $1 ORDER BY m.ts DESC LIMIT 1";

    const char *vals[1] = { device_id };
    PGresult *res = PQexecParams(g_db, sql, 1, NULL, vals, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return NULL;
    }

    json_t *obj = json_object();
    json_object_set_new(obj, "device_id", json_string(device_id));
    json_object_set_new(obj, "cpu", json_real(atof(PQgetvalue(res, 0, 0))));
    json_object_set_new(obj, "ram", json_real(atof(PQgetvalue(res, 0, 1))));
    json_object_set_new(obj, "uptime", json_integer(atoll(PQgetvalue(res, 0, 2))));

    json_error_t err;
    json_t *ports = json_loads(PQgetvalue(res, 0, 3), 0, &err);
    json_object_set_new(obj, "ports", ports ? ports : json_array());

    json_object_set_new(obj, "ts", json_integer(atoll(PQgetvalue(res, 0, 4))));
    json_object_set_new(obj, "type", json_string(PQgetvalue(res, 0, 5)));
    json_object_set_new(obj, "firmware", json_string(PQgetvalue(res, 0, 6)));

    PQclear(res);
    return obj;
}

json_t *db_metrics_history(const char *device_id, time_t from, time_t to) {
    if (db_ensure() != OK) return NULL;

    const char *sql =
        "SELECT EXTRACT(EPOCH FROM ts)::bigint, cpu, ram, uptime, ports "
        "FROM metrics WHERE device_id = $1 "
        "AND ts >= to_timestamp($2) AND ts <= to_timestamp($3) "
        "ORDER BY ts ASC LIMIT 100";

    char from_s[32], to_s[32];
    snprintf(from_s, sizeof(from_s), "%ld", (long)from);
    snprintf(to_s, sizeof(to_s), "%ld", (long)to);

    const char *vals[3] = { device_id, from_s, to_s };
    PGresult *res = PQexecParams(g_db, sql, 3, NULL, vals, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return NULL;
    }

    json_t *arr = json_array();
    int n = PQntuples(res);
    for (int i = 0; i < n; i++) {
        json_t *row = json_object();
        json_object_set_new(row, "ts", json_integer(atoll(PQgetvalue(res, i, 0))));
        json_object_set_new(row, "cpu", json_real(atof(PQgetvalue(res, i, 1))));
        json_object_set_new(row, "ram", json_real(atof(PQgetvalue(res, i, 2))));
        json_object_set_new(row, "uptime", json_integer(atoll(PQgetvalue(res, i, 3))));
        json_error_t err;
        json_t *ports = json_loads(PQgetvalue(res, i, 4), 0, &err);
        json_object_set_new(row, "ports", ports ? ports : json_array());
        json_array_append_new(arr, row);
    }
    PQclear(res);
    return arr;
}

json_t *db_alerts(void) {
    if (db_ensure() != OK) return NULL;

    PGresult *res = PQexec(g_db,
        "SELECT device_id, kind, EXTRACT(EPOCH FROM ts)::bigint, details "
        "FROM alerts ORDER BY ts DESC LIMIT 50");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return NULL;
    }

    json_t *arr = json_array();
    int n = PQntuples(res);
    for (int i = 0; i < n; i++) {
        json_t *obj = json_object();
        json_object_set_new(obj, "device_id", json_string(PQgetvalue(res, i, 0)));
        json_object_set_new(obj, "kind", json_string(PQgetvalue(res, i, 1)));
        json_object_set_new(obj, "ts", json_integer(atoll(PQgetvalue(res, i, 2))));
        json_error_t err;
        json_t *det = json_loads(PQgetvalue(res, i, 3), 0, &err);
        json_object_set_new(obj, "details", det ? det : json_object());
        json_array_append_new(arr, obj);
    }
    PQclear(res);
    return arr;
}