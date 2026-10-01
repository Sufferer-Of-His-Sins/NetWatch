CREATE TABLE devices (
    device_id   TEXT PRIMARY KEY,
    type        TEXT,
    firmware    TEXT,
    online      BOOLEAN NOT NULL DEFAULT FALSE,
    last_seen   TIMESTAMPTZ
);

CREATE TABLE metrics (
    id          BIGSERIAL PRIMARY KEY,
    device_id   TEXT NOT NULL REFERENCES devices(device_id),
    ts          TIMESTAMPTZ NOT NULL,
    cpu         REAL,
    ram         REAL,
    uptime      BIGINT,
    ports       JSONB
);
CREATE INDEX idx_metrics_device_ts ON metrics (device_id, ts);

CREATE TABLE alerts (
    id          BIGSERIAL PRIMARY KEY,
    device_id   TEXT,
    kind        TEXT NOT NULL,
    ts          TIMESTAMPTZ NOT NULL DEFAULT now(),
    details     JSONB
);
CREATE INDEX idx_alerts_ts ON alerts (ts);
