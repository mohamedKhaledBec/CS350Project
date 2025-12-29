# CS350 Real-Time System Monitoring Project

A comprehensive real-time system monitoring solution featuring **preemptive priority scheduling**, automated metrics collection, HTML report generation, and a modern web-based user interface.

## 🌟 Features

### 1. **Preemptive Priority Scheduler**
- **Real-time task scheduling** with 4 priority levels (0 = highest)
- **Preemption support**: Higher priority tasks can interrupt lower priority tasks
- **Condition variables** for efficient task synchronization
- **Thread-safe** implementation with mutexes
- **Time quantum**: 2-second time slices per task

### 2. **System Monitoring Tasks**
- **Task 1 (Priority 0)**: System metrics collection (CPU, RAM, Disk)
- **Task 2 (Priority 1)**: Ethernet/Network monitoring
- **Task 3 (Priority 2)**: Data analysis and alert generation
- **Task 4 (Priority 3)**: Logging and reporting

### 3. **HTML Report Generator**
- Beautiful, responsive HTML reports
- Real-time metrics display with color-coded warnings
- Alert history tracking
- Historical data tables
- Professional styling with gradients and shadows

### 4. **Terminal-Based User Interface**
- **Live dashboard** in the terminal with auto-refresh (2-second intervals)
- **Color-coded metrics**: CPU, RAM, Disk, Network with progress bars
- **Task status table**: Shows all tasks with execution counts
- **Alert monitoring**: Real-time alert display in terminal
- **Unicode box drawing**: Beautiful professional layout
- **ANSI colors**: Green/Yellow/Red for status indication
- **No browser needed**: Pure terminal-based monitoring

## 📁 Project Structure

```
CS350Project/
├── src/
│   ├── scheduler.c          # Main scheduler with preemption
│   ├── analyze.c             # System metrics analyzer
│   ├── report_generator.c    # HTML report generator
│   ├── metrics_exporter.c    # JSON data exporter for UI
│   └── terminal_ui.c         # Terminal dashboard UI
├── build/                    # Compiled binaries
├── scripts/
│   ├── monitor_scheduler.sh  # System monitoring script
│   └── ethernet_fetch.sh     # Network monitoring script
├── logs/                     # Generated logs and reports
├── ui/
│   ├── index.html           # Web-based dashboard
│   └── metrics.json         # Real-time metrics data
├── config/
│   └── sysmon.conf          # Configuration files
├── data/                    # Data storage
└── Makefile                 # Build system

```

## 🚀 Getting Started

### Prerequisites
- GCC compiler
- pthread library
- Bash (for monitoring scripts)
- Modern web browser (for UI)

### Building the Project

```bash
# Build all components
make all

# Build individual components
make scheduler          # Build scheduler
make analyze            # Build analyzer
make report_generator   # Build report generator
make metrics_exporter   # Build JSON exporter
```

### Running the System

#### 1. Start the Scheduler
```bash
make run
# Or directly:
cd build && ./scheduler
```

#### 2. Generate Reports
```bash
# Generate HTML report
make generate-report

# Export metrics to JSON
make export-metrics
```

#### 3. Open the Terminal UI
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

### Color Codingterminal dashboard
make export-metrics   # Generate JSON for web
- 🟡 **Yellow**: Warning threshold exceeded
- 🔴 **Red**: Critical threshold exceeded

## 📝 File Outputs

### Logs Directory
- `alerts.log`: All system alerts with timestamps
- `analysis_report.log`: Detailed analysis results
- `system_report.html`: Generated HTML report
- `netdump.log`: Network activity logs

### UI Directory
- `indpen terminal UI** (in another terminal):
   ```bash
   make ui
   ```

3. **Observe console output**:
   ```
   Initializing Real-Time System Monitor...
   Verbose mode: OFF

   === SCHEDULER STARTING ===

   [Tick 1] Scheduler running...
   → Task 1 running (P0)
   ✓ Monitoring cycle complete

   ⚡ PREEMPTION: Task 2 → Task 1
   ```

4. **Check terminal UI**:
   - Color-coded metrics (Green/Yellow/Red)
   - Task execution counts incrementing
   - Live preemption events
## 🧪 Testing Preemption

To test the preemption system:

1. **Start the scheduler**:
   ```bash
   make run
   ```

2. **Observe console output**:
   ```
   [SCHEDULER] Task 3 (Priority 2) gets CPU
   [TASK 3] Starting execution
   [DISPATCHER] Task 1 is now READY (interval due)
   [PREEMPTION] Task 1 (Priority 0) preempting Task 3 (Priority 2)
   [TASK 3] Was preempted, returning to READY state
   [SCHEDULER] Task 1 (Priority 0) gets CPU
   ```

3. **Check metrics**:
   - Task execution counts
   - Preemption occurrences
   - Time slice usage

## 📈 Performance Metrics

The system tracks:
- **Execution count**: Number of times each task ran
- **TEnsure scheduler has run and generated logs
   - Check logs/ directory exists
   - Wait a few seconds for data to accumulate

2. **Terminal colors not showing**:
   - Use Windows Terminal, iTerm2, or modern terminal
   - WSL/Linux terminals support ANSI colors by defaulterrupted
- **Response time**: Time from READY to RUNNING

## 🔍 Troubleshooting

### Common Issues

1. **UI shows no data**:
   - Run `make export-metrics` to generate metrics.json
   - Ensure scheduler has run and generated logs

2. **Scheduler not compiling**:
   - Check pthread library: `gcc -pthread`
   - Verify GCC version: `gcc --version`

3. **Scripts not executable**:
   - Run: `chmod +x scripts/*.sh`

4. **Port already in use (HTTP server)**:
   - Try different port: `python -m http.server 8080`

## 🎯 Future Enhancements

- [ ] WebSocket support for real-time streaming
- [ ] Database integration for long-term metrics
- [ ] Email/SMS alerts for critical events
- [ ] Machine learning for anomaly detection
- [ ] Multi-node monitoring support
- [ ] REST API for external integrations

## 📚 Technical Details

### Threading Model
- **Main thread**: Scheduler dispatcher
- **Worker threads**: One per task (4 total)
- **Synchronization**: Mutexes and condition variables

### Scheduling Algorithm
- **Type**: Preemptive priority scheduling
- **Priority levels**: 4 (0-3)
- **Time quantum**: 2 seconds
- **Preemption**: Yes (higher priority interrupts lower)

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
