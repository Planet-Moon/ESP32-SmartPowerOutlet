# ESP32 smart power outlet

Using a ESP32-S3-DevKitC-1 Board.

## List of configured URIs

### Overview

| URI | Http Methods | |
|---|---|---|
|/led|GET \| POST| [Led](#Led)|
|/relais| GET \| POST|[Relais](#Relais)|

### Led

HTTP GET | POST /led

#### GET Structure

```json
[
    {
        "red":0,
        "green":0,
        "blue":0
    }
]
```

#### POST Structure

```json
{
    "red":0,
    "green":0,
    "blue":0
}
```

### Relais

HTTP GET | POST /relias

#### GET | POST Structure

```json
{
    "state":true
}
```
