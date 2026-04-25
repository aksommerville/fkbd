# fkbd

Fake keyboard: Create a uinput device that looks like a keyboard and feed it from an evdev gamepad.
For playing games without gamepad support.

## REST API

All GET are static files (`src/www/`).
Everything else is POST, and it's a REST call.

```
/heartbeat
REQUEST:
RESPONSE:
  Single byte: '0' if disconnected or '1' if connected.
The main thing you use this for is to check that the server's still there.
```

```
/shutdown
REQUEST:
RESPONSE:
Terminates the server. You won't receive a response.
```

```
/scan
REQUEST:
RESPONSE:
  [{
    path: string,
    name: string,
    vid: int,
    pid: int,
    version: int,
    buttons: [int],
    axes: [{
      id: int,
      lo: int,
      hi: int,
    },...],
  },...]
Rescan devices directory.
```

```
/devices
Same response as `/scan` but don't refresh first.
```

```
/connect
REQUEST:
  {
    path: string,
  }
RESPONSE:
Disconnect any curent device and connect this one.
Omit path or send empty string to leave disconnected.
```

TODO: Useful endpoints. List devices, list maps, pick a device, edit a map...
