#define WIFI_SSID "<ssid>"
#define WIFI_PASSWORD "<password>"
#define HUE_BRIDGE_URL "http://<host (note, http as it is assumed to be in a safe env)>/api/<token>/groups/<group (usually 0)>/action"
#define INFLUX_URL "https://<host>/api/v2/query?org=<org>"
#define INFLUX_AUTHORIZATION_HEADER "Token <token>"
#define INFLUX_QUERY "\
   from(bucket: \"ruuvi\") \
|> range(start: -10m) \
|> filter(fn: (r) => r[\"_measurement\"] == \"ruuvi_measurement\") \
|> filter(fn: (r) => r[\"_field\"] == \"temperature\" or r[\"_field\"] == \"pressure\" or r[\"_field\"] == \"humidity\") \
|> filter(fn: (r) => r[\"host\"] == \"<host>\") \
|> filter(fn: (r) => r[\"mac\"] == \"<ruuvi-sensor-mac>\") \
|> filter(fn: (r) => r[\"name\"] == \"<ruuvi-sensor-mac>\") \
|> aggregateWindow(every: 15m, fn: mean, createEmpty: false) \
|> yield(name: \"mean\")"
#define HUE_LIGHT_SCENE1 "<json-payload>"
#define HUE_LIGHT_SCENE2 "<json-payload>"
#define HUE_LIGHT_SCENE3 "<json-payload>"
