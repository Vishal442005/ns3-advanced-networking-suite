| TCP Stack | Algorithm | Goodput (Mbps) | UDP p95 (ms) | Short FCT p95 (ms) | Loss (%) | Jain Fairness |
| --- | --- | --- | --- | --- | --- | --- |
| TcpCubic | burst | 3.62 | 83.07 | 3102.24 | 22.68% | 0.9943 |
| TcpCubic | codel | 7.78 | 147.06 | 1075.09 | 1.45% | 0.9717 |
| TcpCubic | cobalt | 7.67 | 147.69 | 985.12 | 1.32% | 0.9760 |
| TcpCubic | droptail | 8.72 | 641.50 | 3385.68 | 0.00% | 0.9994 |
| TcpBbr | burst | 9.48 | 62.35 | 1500.69 | 98.66% | 0.9985 |
| TcpBbr | codel | 3.15 | 68.88 | 511.54 | 0.68% | 0.7602 |
| TcpBbr | cobalt | 3.56 | 76.31 | 707.26 | 0.82% | 0.6834 |
| TcpBbr | droptail | 9.14 | 177.59 | 1108.06 | 0.00% | 0.9957 |
| TcpNewReno | burst | 6.14 | 53.64 | 2304.91 | 8.09% | 0.9969 |
| TcpNewReno | codel | 8.75 | 65.59 | 1096.39 | 0.74% | 0.9854 |
| TcpNewReno | cobalt | 8.67 | 67.78 | 1187.45 | 0.83% | 0.9868 |
| TcpNewReno | droptail | 9.14 | 541.73 | 3703.34 | 0.00% | 1.0000 |