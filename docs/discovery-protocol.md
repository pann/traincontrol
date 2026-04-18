# TrainCtrl Discovery

The firmware exposes itself on the network in two independent ways so that
control software can pick whichever is easiest to consume:

1. **mDNS** — `trainctrl-<XXXX>.local` advertising `_modbus._tcp` on port 502.
   Compatible with generic Bonjour/Avahi clients.
2. **UDP broadcast beacon** — periodic JSON announcement on UDP port 55502.
   Convenient for control software that doesn't want to link an mDNS
   stack, or for networks where multicast is filtered.

Both run side-by-side; clients can use whichever they prefer.

## mDNS

- **Service type**: `_modbus._tcp`
- **Hostname**: `trainctrl-<XXXX>.local`, where `XXXX` is the last two
  bytes of the STA MAC in lowercase hex (stable across reboots).
- **Port**: 502 (the Modbus TCP server).

Quick check from a Linux laptop on the same L2 subnet:

```bash
avahi-resolve -n trainctrl-f5fd.local
avahi-browse -rt _modbus._tcp
```

## UDP broadcast beacon

- **Transport**: UDP, broadcast (`255.255.255.255`)
- **Port**: `55502`
- **Interval**: one datagram every 5 s, starting once the board obtains
  an IP address
- **Payload**: a single JSON object, ASCII-encoded, no trailing newline:

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

## Reference listener (shell)

```bash
# Show every beacon as it arrives:
nc -lu 55502
# Or with sender address visible:
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
  the board. On Linux, `SO_REUSEADDR` is enough.
- Some routers block broadcast across VLANs/SSIDs. The listener must be
  on the same L2 segment as the board.

## Guarantees

- **Direction**: board → network only. The firmware does not listen on
  port 55502; sending to it has no effect.
- **Frequency**: 5 s interval, hard-coded in `sw/main/discovery.c`
  (`BEACON_INTERVAL_MS`). Expect up to one interval of discovery latency.
- **Drop tolerance**: each announcement is self-contained. Missed packets
  just delay discovery.
- **Name stability**: the `name` field is derived from the STA MAC, so it
  does not change across reboots or reflashes.

## Extending the payload

Add fields only; do not rename or remove existing ones. Clients MUST
ignore unknown keys. Good candidates for future fields: firmware version,
uptime, a feature bitmask. Keep the payload under the IPv4 minimum MTU
(576 bytes) to stay safe.
