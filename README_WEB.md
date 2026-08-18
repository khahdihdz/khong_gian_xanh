# Web interface

The project now includes a responsive Vietnamese dashboard at `web/dashboard.html` and a lightweight API helper in `src/web_api.h`.

## Endpoints

- `GET /api/status` — current sensor values and AQI label.
- `GET /api/history` — last 60 samples for charts.

## History

`WebHistory` stores up to 60 samples in RAM and uses a ring buffer, so it has a fixed memory footprint and does not grow over time.

## Dashboard features

- Live temperature, humidity, CO₂, TVOC and AQI cards.
- Automatic warning banner for high temperature/humidity, CO₂, TVOC and AQI.
- CO₂ history line chart.
- Automatic refresh every 5 seconds; history every 30 seconds.
- Vietnamese UI and responsive mobile layout.

The existing firmware should wire `WebHistory::add()` into the sensor sampling interval and register the API routes with the project's existing `WebServer` instance. This keeps the new web layer independent of the current sensor and Wi-Fi code.
