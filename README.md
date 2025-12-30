# CS350 Real-Time System Monitoring Project

A comprehensive real-time system monitoring solution featuring **preemptive priority scheduling**, automated metrics collection, HTML report generation, and multiple user interface options.

## 🎯 Project Overview

This project implements a **real-time system monitor** for Linux systems, demonstrating core concepts from real-time systems including:
- Preemptive priority scheduling with 4 priority levels
- Thread synchronization using mutexes and condition variables
- Periodic task execution with configurable intervals
- Live system metrics collection from `/proc` filesystem

## 🌟 Features

### 1. **Preemptive Priority Scheduler**
- **Real-time task scheduling** with 4 priority levels (0 = highest)
- **Preemption support**: Higher priority tasks can interrupt lower priority tasks
- **Condition variables** for efficient task synchronization
- **Thread-safe** implementation with mutexes
- **Configurable time quantum**: Default 2-second time slices per task
- **Graceful shutdown** via signal handling (Ctrl+C)

### 2. **System Monitoring Tasks**
| Task | Priority | Default Interval | Function |
|------|----------|-----------------|----------|
| System Monitor | P0 (Highest) | 5 seconds | CPU, RAM, Disk metrics |
| Ethernet Fetch | P1 | 10 seconds | Network packet capture |
| Data Analyzer | P2 | 15 seconds | Threshold analysis & alerts |
| Logging/Report | P3 (Lowest) | 10 seconds | Status logging |

### 3. **Multiple User Interfaces**
- **Terminal UI**: ncurses-style live dashboard with color-coded progress bars
- **GTK3 GUI**: Modern graphical interface with real-time controls
- **Web Dashboard**: Responsive HTML reports with visualizations

### 4. **Alert System**
- Configurable thresholds for CPU, RAM, Disk, and Network
- Alert logging with timestamps
- Visual warnings in all UI modes

## 📁 Project Structure

```
CS350Project/
├── src/                          # Source code
│   ├── scheduler.c               # Preemptive priority scheduler (main)
│   ├── analyze.c                 # System metrics analyzer
│   ├── report_generator.c        # HTML report generator
│   ├── metrics_exporter.c        # JSON data exporter for web UI
│   ├── terminal_ui.c             # Terminal dashboard UI
│   └── gtk_live_gui.c            # GTK3 graphical interface
├── build/                        # Compiled binaries
├── scripts/
│   ├── monitor_scheduler.sh      # System metrics collection script
│   ├── ethernet_fetch.sh         # Network monitoring script
│   ├── launch_with_ui.sh         # UI launcher with menu
│   └── archive_loop.sh           # Log archival automation
├── logs/                         # Generated logs and reports
│   └── archives/                 # Archived log bundles
├── ui/
│   ├── index.html                # Web-based dashboard
│   └── metrics.json              # Real-time metrics data
├── config/
│   ├── sysmon.conf               # Main configuration
│   ├── priority_override.conf    # Dynamic priority adjustment
│   └── interval_override.conf    # Dynamic interval adjustment
├── data/                         # Data storage
├── Makefile                      # Build system
└── README.md                     # This file
```

## 🚀 Getting Started

### Prerequisites
- **GCC compiler** (with C11 support)
- **pthread library** (POSIX threads)
- **Linux system** (uses /proc filesystem)
- **Bash shell** (for monitoring scripts)
- **GTK3 development libraries** (optional, for GUI)
  ```bash
  # Ubuntu/Debian
  sudo apt-get install libgtk-3-dev
  
  # Fedora/RHEL
  sudo dnf install gtk3-devel
  ```

### Building the Project

```bash
# Build all core components
make all

# Build with debug symbols
make debug

# Build individual components
make scheduler          # Build scheduler only
make analyze            # Build analyzer only
make terminal_ui        # Build terminal UI only
make gtk_live           # Build GTK GUI (requires GTK3)

# Clean build artifacts
make clean
```

### Running the System

#### Option 1: Interactive Launcher (Recommended)
```bash
make run
# Presents menu:
#   1) Debug console only
#   2) GTK Live GUI
#   3) Terminal UI
```

#### Option 2: Terminal UI
```bash
make ui
```

#### Option 3: GTK GUI
```bash
make gui   # Requires GTK3 libraries
```

### Generating Reports
```bash
# Generate HTML report
make generate-report

# Export metrics to JSON
make export-metrics
```

### Configuration

**Dynamic Priority Adjustment** (`config/priority_override.conf`):
```
# Format: task_id new_priority
1 0    # Task 1 -> Priority 0
2 1    # Task 2 -> Priority 1
```

**Dynamic Interval Adjustment** (`config/interval_override.conf`):
```
# Format: task_id interval_seconds
1 5     # Task 1 runs every 5 seconds
2 10    # Task 2 runs every 10 seconds
```
```bash
# Option 1: Direct file access
open ui/index.html   # macOS
start ui/index.html  # Windows
xdg-open ui/index.html  # Linux

# Option 2: Terminal UI (recommended)
make ui
# Shows live dashboard in terminal with auto-refresh
```

## 🔧 How Preemption Works

### Understanding Priority Interruption

In the enhanced scheduler, **preemption** allows higher-priority tasks to interrupt lower-priority tasks:

```
Priority Levels:
├── Priority 0 (Highest) - System Monitor
├── Priority 1           - Ethernet Fetch
├── Priority 2           - Data Analyzer
└── Priority 3 (Lowest)  - Logging
```

### Preemption Mechanism

1. **Task States**:
   - `READY`: Task is ready to execute
   - `RUNNING`: Task is currently executing
   - `IDLE`: Task is waiting for next interval
   - `PREEMPTED`: Task was interrupted by higher priority

2. **Preemption Flow**:
   ```
   Low Priority Task Running
           ↓
   High Priority Task Becomes READY
           ↓
   check_preemption() detects higher priority
           ↓
   Set preempt_flag = 1
           ↓
   Signal condition variable
           ↓
   Current task state → PREEMPTED
           ↓
   High priority task gets CPU
   ```

3. **Key Implementation**:
   ```c
   // Each task has:
   pthread_cond_t cond;      // For signaling
   int preempt_flag;         // Preemption indicator
   task_state_t state;       // Current state
   
   // Scheduler checks for preemption:
   void check_preemption() {
       // If higher priority task is READY
       // Signal current task to stop
       pthread_cond_signal(&current_task->cond);
   }
   ```

### Example Scenario

```
Time 0: Task 3 (Priority 2) starts running
Time 1: Task 1 (Priority 0) becomes READY
Time 1: Scheduler detects higher priority
Time 1: Task 3 receives preemption signal
Time 1: Task 3 state → PREEMPTED
Time 1: Task 1 gets CPU immediately
Time 3: Task 1 completes
Time 3: Task 3 returns to READY queue
```

## 📊 Monitoring Features

### Real-Time Metrics
- **CPU Usage**: System CPU utilization (%)
- **RAM Usage**: Memory consumption (%)
- **Disk Usage**: Storage utilization (%)
- **Network Packets**: Active network connections


### Terminal UI Display
```
╔════════════════════════════════════════════════════════════════╗
║       🖥️  REAL-TIME SYSTEM MONITOR - Terminal Dashboard       ║
╚════════════════════════════════════════════════════════════════╝

┌─ SYSTEM METRICS ──────────────────────────────────────────────┐
│ CPU Usage:    [████████████████████░░░░░░░░░░░░] 62.5%
│ RAM Usage:    [████████████████████████████░░░░] 75.3%
│ Disk Usage:   [████████████████████████░░░░░░░░] 68.1%
│ Network:      5 packets
└───────────────────────────────────────────────────────────────┘

┌─ SCHEDULER TASKS ─────────────────────────────────────────────┐
│ ID │ Priority │ Task Name       │ State   │ Executions │
├────┼──────────┼─────────────────┼─────────┼────────────┤
│  1 │   0      │ System Monitor  │ IDLE    │     12     │
│  2 │   1      │ Ethernet Fetch  │ IDLE    │      6     │
│  3 │   2      │ Analyzer        │ IDLE    │      4     │
│  4 │   3      │ Logging         │ IDLE    │      6     │
└────┴──────────┴─────────────────┴─────────┴────────────┘
```
### Alert Thresholds
```c
#define CPU_THRESHOLD 80    // 80% CPU
#define RAM_THRESHOLD 85    // 85% RAM
#define DISK_THRESHOLD 90   // 90% Disk
#define PACKET_THRESHOLD 8  // 8+ packets
```

## 🎨 User Interface Features

### Dashboard Components
1. **Live Metrics Cards**: Real-time system stats with color coding
2. **CPU History Chart**: Visual representation of last 20 samples
3. **Active Tasks Panel**: Status of all scheduler tasks
4. **Alerts Section**: Recent system alerts and warnings
5. **Control Panel**: Refresh, pause, and export controls

### Color Coding
- 🟢 **Green**: Normal range
- 🟡 **Yellow**: Warning threshold exceeded
- 🔴 **Red**: Critical threshold exceeded

## 📝 File Outputs

### Logs Directory
| File | Description |
|------|-------------|
| `scheduler.log` | Detailed scheduler activity log |
| `alerts.log` | System alerts with timestamps |
| `analysis_report.log` | Detailed analysis results |
| `system_report.html` | Generated HTML report |
| `netdump.log` | Network activity logs |

### UI Directory
| File | Description |
|------|-------------|
| `index.html` | Web-based dashboard |
| `metrics.json` | JSON data for web UI |

## 🧪 Testing the System

### Testing Preemption

1. **Start the scheduler**:
   ```bash
   make run   # Select option 1 for debug console
   ```

2. **Observe preemption in action**:
   ```
   [SCHEDULER] Task 3 (Priority 2) gets CPU
   [TASK 3] Starting execution
   [DISPATCHER] Task 1 is now READY (interval due)
   [PREEMPTION] Task 1 (Priority 0) preempting Task 3 (Priority 2)
   ⚡ PREEMPTION: Task 3 → Task 1
   [TASK 3] Was preempted, returning to READY state
   [SCHEDULER] Task 1 (Priority 0) gets CPU
   ```

### Testing Dynamic Configuration

1. **Modify priority** while scheduler is running:
   ```bash
   echo "1 2" > config/priority_override.conf  # Task 1 → Priority 2
   ```

2. **Modify interval**:
   ```bash
   echo "1 3" > config/interval_override.conf  # Task 1 runs every 3s
   ```

3. **Changes apply on next scheduler tick** (within 2 seconds)

## 📈 Performance Metrics

The system tracks:
- **Execution count**: Number of times each task completed
- **Time used**: CPU time consumed per execution
- **Preemptions**: Number of times task was interrupted
- **Interval compliance**: Actual vs. configured intervals

## 🔍 Troubleshooting

### Common Issues

| Problem | Solution |
|---------|----------|
| UI shows no data | Run `make export-metrics` first |
| No scheduler output | Check `logs/scheduler.log` exists |
| Terminal colors broken | Use modern terminal (Windows Terminal, iTerm2) |
| GTK build fails | Install GTK3: `sudo apt install libgtk-3-dev` |
| Permission denied | Run `chmod +x scripts/*.sh` |

### Debug Build

For verbose output and debugging:
```bash
make debug
```

## 📚 Technical Details

### Scheduling Algorithm
- **Type**: Preemptive Priority Scheduling
- **Priority Levels**: 4 (0 = highest, 3 = lowest)
- **Time Quantum**: 2 seconds
- **Preemption**: Immediate on higher priority task becoming READY

### Thread Synchronization
- **Mutexes**: Per-task lock + global scheduler lock
- **Condition Variables**: For task wake-up signaling
- **Atomic Operations**: Preempt flag checking

### System Calls Used
- `/proc/stat` - CPU statistics
- `/proc/meminfo` - Memory information
- `statvfs()` - Disk usage
- `/proc/net/dev` - Network statistics

## 👨‍🎓 Author

**CS350 Real-Time Systems Course Project**  
*Demonstrating preemptive scheduling, thread synchronization, and system monitoring*

## 📄 License

This project is for educational purposes as part of CS350 coursework.

### Data Flow
```
System → Scripts → Analyzer → Logs → Report Generator → HTML
                                  ↓
                            Metrics Exporter → JSON → Web UI
```

## 👥 Contributors

CS350 - Operating Systems Course Project

## 📄 License

Educational use only - CS350 Course Project

---

**Made with ❤️ for CS350 Operating Systems**
