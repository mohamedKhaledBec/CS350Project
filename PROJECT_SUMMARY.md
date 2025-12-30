# Real-Time System Monitor - Project Summary

## Features

### Preemptive Priority Scheduler
- 4 priority levels (0=highest to 3=lowest)
- Full preemption support
- Thread synchronization with mutexes and condition variables
- 2-second time quantum
- Starvation detection (30-second threshold)

### Scheduled Tasks
| Task | Priority | Interval | Function |
|------|----------|----------|----------|
| System Monitor | 0 | 5s | CPU/RAM/Disk collection |
| Ethernet Fetch | 1 | 10s | Network statistics |
| Analyzer | 2 | 15s | Metrics analysis & alerts |

### GTK GUI
- Real-time CPU, RAM, Disk percentages
- Network packets per second
- Color-coded thresholds (Green/Yellow/Red)
- Auto-refresh every 2 seconds

### Alert Thresholds
| Metric | Warning | Critical |
|--------|---------|----------|
| CPU | 70% | 90% |
| RAM | 75% | 90% |
| Disk | 80% | 95% |
| Network | 50 pkt/s | 100 pkt/s |

---

## Project Structure
```
CS350Project/
├── src/
│   ├── scheduler.c         # Preemptive scheduler
│   ├── gtk_gui.c           # GTK monitor interface
│   ├── analyze.c           # Metrics analyzer
│   ├── report_generator.c  # HTML reports
│   └── metrics_exporter.c  # JSON export
├── scripts/
│   ├── monitor_scheduler.sh
│   └── ethernet_fetch.sh
├── config/
│   ├── sysmon.conf
│   ├── priority_override.conf
│   └── interval_override.conf
├── logs/
├── build/
└── Makefile
```

---

## Commands
```bash
make all      # Build
make gui      # Run GUI
make setup    # Create directories
make reset    # Clean and rebuild
```

---

## Preemption Example
```
Time 0: Task 3 starts (Priority 2)
Time 1: Task 1 ready (Priority 0) → PREEMPTS Task 3
Time 2: Task 1 completes
Time 3: Task 3 resumes
```

---

**Course:** CS350 - Operating Systems  
**Date:** December 2024
