# Drone Simulator

# Advance Robots Programming - Assignment 1

Student: Bahri Riadh
ID:8335614

## 1 Project Overview

This project implements a multi-process drone simulation system using Ncurses for visualization and Named Pipes (FIFOs) for inter-process communication. The architecture follows a Star Topology centered around a Blackboard Server.

The goal is to navigate a drone (Blue +) to collect sequential targets (Green 1-9) while avoiding repulsive obstacles (Yellow o).

## 2. Components & Algorithms

This section details the logic implemented in each source file.

### A. Main Process (`src/main.c`)

**Role:** Process Launcher & Lifecycle Manager.  
**Primitives:** fork(), exec(), signal(), mkfifo().

**Logic:**

- Creates all Named Pipes (FIFOs) required for communication.
- Installs Signal Handlers (SIGINT) to catch Ctrl+C.
- Forks internal processes (Blackboard, Dynamics, Generators).
- Spawns external terminal windows (konsole/xterm) for the UI.

**Signal Handling Algorithm:**
On Signal(SIGINT):
Send SIGTERM to all child_pids
Unlink (delete) all named pipes
Exit gracefully
---

### B. Blackboard Server (`src/blackboard.c`)

**Role:** Central State Manager & Message Router.  
**Primitives:** open(O_RDWR), read(O_NONBLOCK), write().

**Algorithm:**

The server uses a Non-Blocking Polling Loop to prevent freezing if one client is slow.
Loop (1000 Hz):
For each Input Pipe (UI, Dynamics, Obstacles, Targets):
If Data Available:
Read Message
Update Internal State (Drone Pos, Score, etc.)
Switch(Message Type):
Case DRONE_STATE: Broadcast to Map & UI_Input
Case FORCE: Forward to Dynamics
Case OBSTACLE: Forward to Map & Dynamics
Case TARGET: Forward to Map & Dynamics
---

### C. Drone Dynamics (`src/dynamics.c`)

**Role:** Physics Engine.  
**Primitives:** Euler Integration, Vector Math.

#### Algorithm 1: Physics Model

Calculates movement based on the equation:  
`F = ma + kv`.
Loop (500 Hz):

1.Calculate Repulsion Force (F_rep):
For each Obstacle:
If Distance < 10m:
F_rep += (1/Distance - 1/Rho) * (1/Distance^2)

2.Calculate Total Force:
F_total = Command_Force + F_rep

3.Euler Integration:
Acceleration = (F_total - K * Velocity) / Mass
Velocity += Acceleration * T
Position += Velocity * T
#### Algorithm 2: Collision Detection
If Distance(Drone, Target[i]) < 2.0m:
If Target[i].ID == Next_Required_ID:
Mark Target as Collected
Regenerate Target Position (Respawn)
Send Update to Server
---

### D. UI Input (`src/ui_input.c`)

**Role:** Controller & Telemetry Display.  
**Primitives:** getch(), Ncurses.

**Algorithm:**
Loop (50 Hz):

1.Read Telemetry from Server (Position, Speed)

2.Read Input (Burst Mode):
While (Key = getch()) != ERR:
Process Key (Increment/Decrement Force Variables)

3.If Input Changed:
Send Force Vector to Server

5.Redraw Telemetry Data

---

### E. UI Map (`src/ui_map.c`)

**Role:** Visualizer.  
**Primitives:** Ncurses Colors, Coordinate Scaling.

**Algorithm:**

Loop (50 Hz):

1.Read World State from Server

2.Check Terminal Size:
Scale_X = Window_Width / Map_Width
Scale_Y = Window_Height / Map_Height

3.Draw Entities:
Screen_X = World_X * Scale_X
Draw(Drone, Obstacles, Targets)

---

### F. Generators (`src/obstacles.c`, `src/targets.c`)

**Role:** Procedural Generation.

**Algorithm:**
Loop:
Sleep(Interval)
Generate Random X, Y within bounds
Send MSG_OBSTACLE or MSG_TARGET to Server

--
## 3.Installation and  Running : 

### Prerequisites

- Linux Environment

- GCC Compiler & Make

- libncurses5-dev or similar

- konsole or xterm (for window spawning)

### Automated Setup

    The project includes scripts to handle installation and execution automatically.

 * A - Install Dependencies:
        ```bash
        chmod +x install.sh
        ./install.sh
        ```
 * B - Run the Simulation: This script cleans, compiles, and launches the app.
  ## How to Run
```bash
chmod +x run.sh
./run.sh
```

  ## Manual Build:
  If the user need to debug or build manually :
```bash
make clean
make
./main
```
### 4. Operational Instructions :

 ## Controls

Use the Input Window to pilot the drone. The controls apply force to the drone's engines, simulating a real thruster system.

 * E / C: Increase Up / Down Force

 * S / F: Increase Left / Right Force

 * D: Brake (Instantly resets all command forces to 0, simulating an air-brake to stop the drone)

 * R, Z, V, X: Apply Diagonal Forces (Combinations of X and Y thrust)

 * ESC: Quit the simulation (closes all windows safely)

 ## Gameplay Rules

 * Objective: Fly the drone (Blue +) into the Green Targets.

 * Sequence: You must collect targets in numerical order: 1 $\to$ 2 $\to$ 3 ... $\to$ 9.

 * Scoring: Collecting the correct target increments your score (internally tracked) and causes the target to respawn in a new location, creating an endless loop of gameplay.

 * Hazards: Avoid the Yellow Obstacles (o). They generate a repulsive force field that will push your drone away based on the inverse square law, making navigation challenging near walls or clusters of obstacles.

### 5. Configuration :

You can fine-tune the physics of the simulation without recompiling the code. Simply edit the config/params.txt file to adjust the drone's handling characteristics:

 * M: Mass of the drone. (Lower value = faster acceleration, "lighter" feel)

 * K: Viscous friction coefficient. (Higher value = stops faster/less drift, "tighter" handling)

 * F_STEP: Force applied per key press. (Higher value = more powerful engines, faster top speed)

### 4 - File Structure : 

 * src/main.c: Process launcher and signal handler.

 * src/blackboard.c: Server logic and message routing.

 * src/dynamics.c: Physics engine and collision detection.

 * src/ui_map.c: Map visualization window.

 * src/ui_input.c: Input controller window.

 * src/obstacles.c: Obstacle generator.

 * src/targets.c: Target generator.

 * src/params.c: Configuration file parser.

 * src/common.h: Shared constants, structs, and protocol definitions.

 * config/params.txt: Runtime configuration file.

 * Makefile: Compilation rules.

 * run.sh / install.sh: Automation scripts.
  