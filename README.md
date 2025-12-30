# CS350 Real-Time System Monitor

A real-time Linux system monitoring application featuring a **preemptive priority scheduler** with **starvation detection**, live system metrics, and a modern GTK3 graphical interface.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-green.svg)
![C Standard](https://img.shields.io/badge/C-C11-blue.svg)

---

## 📋 Table of Contents

- [Features](#-features)
- [Architecture](#-architecture)
- [Requirements](#-requirements)
- [Installation](#-installation)
- [Usage](#-usage)
- [Configuration](#-configuration)
- [Project Structure](#-project-structure)
- [Technical Details](#-technical-details)
- [Screenshots](#-screenshots)
- [Authors](#-authors)

---

## ✨ Features

### Core Scheduling
- **Preemptive Priority Scheduling**: 4 priority levels (0=highest, 3=lowest)
- **2-Second Time Quantum**: Fair CPU distribution with preemption
- **Starvation Detection**: Automatic warnings when tasks haven't run for 30+ seconds
- **Config Validation**: Warns if priority changes may cause task starvation
- **Dynamic Configuration**: Runtime priority and interval adjustment via config files

### System Monitoring
- **Real-Time Metrics**: CPU, RAM, Disk usage from `/proc` filesystem
- **Network Monitoring**: Packet counts from `/proc/net/dev`
- **Threshold Alerts**: Warning (70%) and critical (90%) thresholds
- **Color-Coded Output**: Visual feedback for system status

### User Interfaces
- **GTK3 GUI**: Modern dark-themed graphical interface with:
  - Live progress bars for all metrics
  - Scheduler task control panel
  - Starvation status indicators
  - Real-time alert display
- **Console Output**: Formatted ANSI-colored terminal output
- **HTML Reports**: Professional responsive web reports

### Data Export
- **JSON Export**: Web-ready metrics for dashboard integration
- **HTML Reports**: Standalone reports with CSS styling
- **Log Files**: Timestamped logs for all operations

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    CS350 SYSTEM MONITOR                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐      │
│  │   SCHEDULER  │    │   ANALYZER   │    │  GTK3 GUI    │      │
│  │              │    │              │    │              │      │
│  │ • 4 Priority │    │ • Threshold  │    │ • Live View  │      │
│  │   Queues     │    │   Checking   │    │ • Controls   │      │
│  │ • Starvation │    │ • Alert Gen  │    │ • Starvation │      │
│  │   Detection  │    │              │    │   Indicators │      │
│  └──────────────┘    └──────────────┘    └──────────────┘      │
│         │                   │                   │               │
│         └───────────────────┼───────────────────┘               │
│                             │                                   │
│                     ┌───────┴───────┐                           │
│                     │  /proc FS     │                           │
│                     │  • /proc/stat │                           │
│                     │  • /proc/mem  │                           │
│                     │  • /proc/net  │                           │
│                     └───────────────┘                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📦 Requirements

### System Requirements
- **Operating System**: Linux (kernel 3.0+)
- **Architecture**: x86_64, ARM64

### Build Requirements
- **Compiler**: GCC 7+ with C11 support
- **Libraries**: 
  - POSIX threads (pthreads)
  - GTK3 development libraries (for GUI)

### Installing Dependencies

**Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install build-essential libgtk-3-dev
```

**Fedora:**
```bash
sudo dnf install gcc gtk3-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel gtk3
```

---

## 🚀 Installation

### Clone and Build

```bash
# Clone the repository
git clone https://github.com/your-repo/cs350-system-monitor.git
cd cs350-system-monitor

# Build all core components
make all

# Build with GTK3 GUI (optional)
make gtk_gui

# Build with debug symbols
make debug
```

### Verify Installation

```bash
# Check built executables
ls -la build/

# Run help
make help
```

---

## 💻 Usage

### Running the Scheduler

```bash
# Start the preemptive scheduler
make run

# Or run directly
./build/scheduler
```

### Launching the GUI

```bash
# Build and launch GTK3 interface
make gui

# Or run directly
./build/gtk_gui
```

### Analyzing Metrics

```bash
# Run analyzer with metrics
./build/analyze 45.2 62.8 58.3 logs/net_log.txt

# Arguments: <cpu%> <ram%> <disk%> <netlog>
```

### Generating Reports

```bash
# Generate HTML report
make report

# Export JSON metrics
make export-metrics
```

---

## ⚙️ Configuration

### Priority Configuration
File: `config/priority_override.conf`

```
# Format: <task_id> <priority>
# Priority: 0 (highest) to 3 (lowest)
1 0
2 1
3 2
4 3
```

### Interval Configuration
File: `config/interval_override.conf`

```
# Format: <task_id> <interval_seconds>
# Interval: 1-300 seconds
1 5
2 10
3 15
4 10
```

### Alert Thresholds (in source)

| Metric | Warning | Critical |
|--------|---------|----------|
| CPU    | 70%     | 90%      |
| RAM    | 75%     | 90%      |
| Disk   | 80%     | 95%      |

---

## 📁 Project Structure

```
CS350Project/
├── src/
│   ├── scheduler.c         # Preemptive priority scheduler
│   ├── analyze.c           # Metrics analyzer with alerts
│   ├── gtk_gui.c           # GTK3 graphical interface
│   ├── report_generator.c  # HTML report generator
│   └── metrics_exporter.c  # JSON exporter for web UI
├── config/
│   ├── priority_override.conf
│   ├── interval_override.conf
│   └── sysmon.conf
├── logs/
│   ├── scheduler.log
│   ├── alerts.log
│   └── system_report.html
├── ui/
│   ├── index.html          # Web dashboard
│   └── metrics.json        # Exported metrics
├── build/                  # Compiled executables
├── scripts/                # Utility scripts
├── Makefile
└── README.md
```

---

## 🔧 Technical Details

### Scheduler Tasks

| ID | Task Name        | Default Priority | Default Interval |
|----|------------------|------------------|------------------|
| 1  | System Monitor   | 0 (Highest)      | 5 seconds        |
| 2  | Ethernet Fetch   | 1                | 10 seconds       |
| 3  | Analyzer         | 2                | 15 seconds       |
| 4  | Report Generator | 3 (Lowest)       | 10 seconds       |

### Starvation Detection

The scheduler monitors task execution times and generates warnings:

- **Warning**: Task hasn't run for 15+ seconds
- **Critical**: Task hasn't run for 30+ seconds (STARVING)

### Data Sources

| Metric  | Source            | Update Rate |
|---------|-------------------|-------------|
| CPU     | `/proc/stat`      | 1 second    |
| RAM     | `/proc/meminfo`   | 1 second    |
| Disk    | `statvfs("/")     | 5 seconds   |
| Network | `/proc/net/dev`   | 1 second    |

---

## 📸 Screenshots

### GTK3 GUI
The graphical interface provides:
- Real-time metric progress bars with color coding
- Task priority/interval controls
- Starvation status indicators
- Live status bar

### HTML Report
Generated reports include:
- Gradient progress bars
- Alert history with severity badges
- Mobile-responsive design
- Dark theme styling

---

## 👨‍💻 Authors

**CS350 Real-Time Systems Project**

Course: CS350 - Operating Systems  
Institution: [Your University]  
Semester: Fall 2024

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## 📞 Support

For issues and questions:
- Open a GitHub issue
- Contact the course instructor

---

*Built with ❤️ for CS350 Real-Time Systems*
