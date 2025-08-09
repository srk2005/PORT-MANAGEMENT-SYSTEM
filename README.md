
# Port Management System 

A comprehensive port scheduling and management system implemented in C for efficiently managing ship arrivals, dock allocation, cargo handling, and ship departures at Tortuga Port.

##  Project Overview

This project simulates Captain Jack Sparrow's transformation from pirate to Portmaster, managing the bustling cargo port of Tortuga. The system handles complex scheduling challenges involving:

- **Multi-type Ship Management**: Regular incoming, emergency incoming, and outgoing ships
- **Dynamic Dock Allocation**: Category-based assignment with capacity constraints  
- **Intelligent Cargo Handling**: Crane optimization for efficient loading/unloading
- **Emergency Prioritization**: Maximum allocation algorithm ensuring emergency ships get priority
- **Secure Authentication**: Radio frequency guessing system for ship undocking

##  Key Features

###  Emergency Ship Priority System
- **Maximum Allocation Guarantee**: Emergency ships get priority assignment to all available compatible docks
- **No Preemption**: Emergency ships cannot displace already docked ships
- **Category Compatibility**: Ships only dock at docks with category ≥ ship category

###  Efficient Scheduling Algorithms
- **Emergency First**: Greedy maximum matching for emergency ships
- **EDF for Regular Ships**: Earliest deadline first with waiting time constraints
- **Optimal Cargo Assignment**: Best-fit crane allocation for each cargo item

###  Advanced IPC Communication
- **System V Message Queues**: Bidirectional communication with validation module
- **Shared Memory Integration**: Efficient data sharing for ship requests and authentication
- **Multi-Solver Architecture**: Parallel radio frequency guessing with multiple solver processes

##  Technical Specifications

### System Constraints
| Component | Range | Description |
|-----------|-------|-------------|
| **Docks** | 1-30 | Each dock has category 1-25 |
| **Ships** | Up to 1100 | 500 regular + 100 emergency + 500 outgoing |
| **Cargo** | 1-200 per ship | Weight range: 1-30 units |
| **Cranes** | 1-25 per dock | Capacity range: 1-30 units |
| **Performance** | ≤ 600 timesteps | Must complete within 6 minutes real-time |

### Architecture
- **Language**: POSIX-compliant C with System V IPC
- **Concurrency**: Multi-process architecture with solver integration
- **Memory Management**: Static allocation for predictable performance
- **Communication**: Message queues + shared memory for low-latency IPC

## Getting Started

### Prerequisites
- **OS**: Ubuntu 22.04/24.04 LTS (lab-compatible)
- **Compiler**: GCC with C11 support
- **Architecture**: Intel/AMD x64 or ARM64 (Mac M1)

### Installation

1. **Clone the repository**
   ```bash
   cd port-management-system
   ```

2. **Compile the scheduler**
   ```bash
   gcc -O2 -std=c11 scheduler.c -o scheduler.out
   ```

3. **Set validation permissions**
   ```bash
   chmod 777 validation.out
   ```

### Project Structure
```
port-management-system/
├── scheduler.c              # Main scheduler implementation (950+ lines)
├── validation.out           # Validation module (provided by course)
├── testcase_1/             # Basic test case
│   ├── input.txt           # IPC keys and dock configuration
│   └── expected_output.txt # Expected results
├── testcase_2/ ... testcase_6/  # Progressive complexity test cases
├── README.md               # This documentation
```

## Usage Instructions

### Running Test Cases

**Important**: Always start validation first!

1. **Terminal 1 - Start Validation**:
   ```bash
   ./validation.out 1
   ```

2. **Terminal 2 - Run Scheduler**:
   ```bash
   ./scheduler.out 1
   ```

Replace `1` with desired test case number (1-6).

--- 
