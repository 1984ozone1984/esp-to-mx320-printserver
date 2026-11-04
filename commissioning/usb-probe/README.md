# usb-probe — commissioning aid, not product code

Proves debugging-agenda items #3 (hub powered with the XIAO as its upstream
host), #4 (MX320 enumerates as class 07/01 and returns a 1284 Device ID),
#5 (MX320 port-status bits change) and helps #14 (cascaded hub, flash drive).

Delivered by a bench cold flash (FSD §2.2) built by CI
(`.github/workflows/probe.yml`); the product image returns the same way.

UART lines (115200 on the adapter):

| Line | Meaning |
|---|---|
| `probe: usb-host fw=<ver>` | probe booted, host stack installing |
| `probe: host installed` | USB host library up, root port powered |
| `probe: attach addr=<a> vid=<v> pid=<p> class=<c> speed=<s>` | device enumerated |
| `probe: intf addr=<a> num=<n> alt=<a> class=<c> sub=<s> proto=<p>` | each interface of it |
| `probe: deviceid addr=<a> len=<n> id=<1284 string>` | printer-class GET_DEVICE_ID |
| `probe: portstatus addr=<a> intf=<n> value=0x<xx> paper_empty=<0/1> select=<0/1> not_error=<0/1>` | on change, and every 30 s |
| `probe: gone addr=<a>` | device detached |
| `probe: alive <n> devices=<k>` | heartbeat every 5 s |
