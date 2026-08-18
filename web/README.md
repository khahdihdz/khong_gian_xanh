# Web dashboard

Open `dashboard.html` from the ESP32 web server once the project serves the `web/` assets.

The dashboard expects:

```text
GET /api/status
GET /api/history
```

`/api/status` should return `temperature`, `humidity`, `co2`, `tvoc`, `aqi`, and optional `aqiLabel`.

`/api/history` returns an array of samples with `ts`, `temperature`, `humidity`, `co2`, `tvoc`, and `aqi`.
