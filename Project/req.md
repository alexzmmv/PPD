Each student or team of 2 students will take one project. It is ok to take a theme that is not listed below (but check with the lab advisor before starting).

Each project will have 2 implementations: one with "regular" threads or tasks/futures, and one distributed (possibly, but not required, using MPI).

Simulate n-body problem with gravitational force using Barnes-Hut algorithm.
The program have a config.txt containing the bodies initial positions and masses, and the simulation parameters (time step, number of steps, etc). The program will show the bodies moving in a window using OpenGL Library.
The simulation will be done in 2D space.

A body visual is represented as a circle with radius square root to the mass

config.txt example:

```
# ---- Simulation ----
time_step = 0.01
num_steps = 10000
theta = 0.5
softening = 0.01
gravitational_constant = 1.0

window_width = 800
window_height = 800

# ---- Parallel ----
num_threads = 8

# ---- Bodies ----
# BODIES
# id mass x y vx vy

Bodies:
1 10.0 0.0 0.0 0.0 0.0
2 5.0 100.0 0.0 0.0 1