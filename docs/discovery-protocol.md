# TrainCtrl Discovery Protocol

Auto-discovery mechanism used by the TrainCtrl shield firmware in place of mDNS.
The firmware periodically sends a UDP broadcast containing its identity and
Modbus endpoint. Control software listens for the broadcast and uses the
announced IP to connect.

## Wire format

- **Transport**: UDP, broadcast (`255.255.255.255`)
- **Port**: `55502`
- **Interval**: one datagram every 5 s, starting as soon as the board obtains
  an IP address
- **Payload**: a single JSON object, ASCII-encoded, no trailing newline. Example:

  ```json
  {"app":"traincontrol","name":"trainctrl-f5fd","ip":"192.168.1.47","modbus":502}
  ```

### Field reference

| Field    | Type   | Description                                                        |
|----------|--------|--------------------------------------------------------------------|
| `app`    | string | Always `"traincontrol"`. Use this as the filter to ignore other    |
|          |        | UDP traffic on port 55502.                                         |
| `name`   | string | Board hostname, format `trainctrl-<XXXX>` where `XXXX` is the      |
|          |        | last 2 bytes of the STA MAC in lowercase hex. Stable across boots. |
| `ip`     | string | IPv4 address the board received from DHCP. Dotted-decimal.         |
| `modbus` | number | TCP port where the Modbus server listens. Currently always `502`.  |

## Reference listener (Linux shell)

Quick manual check — should print a line every 5 s once a board is online on
the same L2 subnet:

```bash
nc -lu 55502
```

Or with the sender's address visible:

```bash
socat -u UDP-RECVFROM:55502,fork STDOUT
```

## Reference listener (Python)

```python
import json
import socket

DISCOVERY_PORT = 55502

def discover(timeout=10):
    """Yield (addr, info_dict) for each TrainCtrl announcement heard."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", DISCOVERY_PORT))
    sock.settimeout(timeout)
    try:
        while True:
            data, addr = sock.recvfrom(512)
            try:
                info = json.loads(data.decode("utf-8"))
            except (ValueError, UnicodeDecodeError):
                continue
            if info.get("app") != "traincontrol":
                continue
            yield addr, info
    except socket.timeout:
        return
    finally:
        sock.close()

if __name__ == "__main__":
    for addr, info in discover(timeout=15):
        print(f"{info['name']} at {info['ip']}:{info['modbus']} (from {addr[0]})")
```

**Binding caveats:**

- Bind to `""` (INADDR_ANY). Some OSes reject binding directly to
  `255.255.255.255`.
- If your host has multiple interfaces, pick the one on the same subnet as
  the board. On Linux, `SO_REUSEADDR` is enough; on Windows you may also need
  `SO_REUSEPORT` equivalents.
- Many routers block broadcast across VLANs/SSIDs. The listener must be on
  the same L2 segment as the board.

## Behaviour and guarantees

- **Direction**: board → network only. The firmware does not listen on port
  55502; sending to it has no effect.
- **Frequency**: 5 s interval is hard-coded in `discovery.c`
  (`BEACON_INTERVAL_MS`). Acceptable discovery latency is one interval.
- **Start/stop**: beacon starts on `IP_EVENT_STA_GOT_IP`. On WiFi
  disconnect it keeps the task alive but the inner `sendto()` calls fail
  silently until STA reconnects and gets a new IP.
- **Drop tolerance**: each announcement is self-contained. Missing one or
  more packets just delays discovery — no state to resync.
- **Stability of `name`**: derived from the STA MAC, so the hostname does
  not change across reboots or reflashes.

## Typical client integration

1. On startup, open a UDP socket bound to `0.0.0.0:55502`.
2. Read datagrams; parse JSON; filter by `app == "traincontrol"`.
3. Cache `(name → ip, modbus_port)` and refresh whenever a new announcement
   arrives. Consider a board "lost" if no announcement is heard for ~3× the
   interval (15 s).
4. For Modbus I/O, open a TCP connection to `ip:modbus`. Prefer the
   announced IP over a cached one — DHCP leases can rotate.

## Extending the payload

Add fields only; do not rename or remove existing ones. Clients MUST ignore
unknown keys. Good candidates for future fields: firmware version, uptime,
a feature bitmask, TLS-enabled Modbus port. Format remains a flat JSON
object; keep it under the IPv4 minimum MTU (576 bytes) to stay safe.
