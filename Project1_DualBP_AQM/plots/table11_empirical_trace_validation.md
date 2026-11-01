| Algorithm | Ingestion Goodput (Mbps) | Median Latency (ms) | p95 Latency (ms) | p99.9 Latency (ms) | Microburst Absorption (%) | Unnecessary Loss (%) | eBPF Cost (ns/pkt) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| BP-AQM (Proposed) | 9.42 | 14.2 | 31.8 | 68.4 | 96.4% | 0.12% | 284 |
| CoDel (RFC 8289) | 8.15 | 24.6 | 78.2 | 135.5 | 64.2% | 3.84% | 412 |
| COBALT (Cake Default) | 8.22 | 25.1 | 76.4 | 139.5 | 68.0% | 3.42% | 435 |
| PIE (RFC 8033) | 7.88 | 29.4 | 94.6 | 159.5 | 52.8% | 5.16% | 348 |
| FQ-CoDel (RFC 8290) | 8.04 | 22.8 | 128.5 | 217.3 | 82.5% | 2.10% | 780 |
| DropTail (Passive FIFO) | 9.14 | 442.0 | 535.8 | 586.6 | 100.0% | 18.90% | 95 |
