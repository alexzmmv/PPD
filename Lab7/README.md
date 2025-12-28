# Lab7 — Distributed Polynomial Multiplication (MPI)

This folder contains an MPI implementation of polynomial multiplication using:

- Regular O(n^2) distributed across MPI ranks (`regular-mpi`).
- Karatsuba (one-level distribution) where the master delegates the three Karatsuba products to workers (`karatsuba-mpi`).

Build:

```bash
make
```

Run examples:

Regular distributed (4 processes recommended):

```bash
mpirun -np 4 ./lab7 regular-mpi 1000
```

Karatsuba distributed (requires >= 4 processes):

```bash
mpirun -np 4 ./lab7 karatsuba-mpi 1000
```

Compare with CPU implementations (single-process):

```bash
./lab7 regular-cpu 1000
./lab7 karatsuba-cpu 1000
```

Notes:

- The Karatsuba MPI implementation performs a single top-level distribution: the master splits the input and sends three subproblems to ranks 1..3. Workers compute the subproducts sequentially and return results.
- The regular MPI approach broadcasts the polynomials; each rank computes a subset of rows and an `MPI_Reduce` sums partial results on the root.
- Adjust degree and process count as needed. For accurate performance comparisons, run multiple repetitions and exclude polynomial generation time.



How to run on 2 devices (quick reference)

Prerequisites
- Install the same MPI implementation on both machines (Open MPI recommended).
- Ensure `lab7` binary is present at the same path on both machines, or use a shared filesystem (NFS).
- Enable passwordless SSH from the controller (the machine you run `mpirun` from) to the other machine.

On both machines:

1. Build or copy the binary

If you have a shared filesystem (same path on both): build on one host:
```bash
make -C Lab7
```

If no shared FS, build locally on each or copy the binary:
```bash
scp Lab7/lab7 user@nodeB:/home/user/Lab7/
```

2. Create a hostfile on the controller (hostnames must be reachable)
```text
# hosts
nodeA slots=1
nodeB slots=1
```

3. Run with `mpirun` from the controller

Example (2 processes across 2 machines):
```bash
mpirun -np 2 --hostfile hosts ./Lab7/lab7 regular-mpi 10000
```

Or explicitly:
```bash
mpirun -np 2 --host nodeA:1,nodeB:1 ./Lab7/lab7 regular-mpi 10000
```

4. If MPI is installed in a non-default location, provide `--prefix`:
```bash
mpirun --prefix /opt/openmpi -np 2 --hostfile hosts ./Lab7/lab7 regular-mpi 10000
```

5. If using Karatsuba distributed (top-level distribution to 3 workers), run with at least 4 processes (master + 3 workers):
```bash
mpirun -np 4 --hostfile hosts ./Lab7/lab7 karatsuba-mpi 10000
```

Quick checks & troubleshooting
- Verify SSH: `ssh nodeB hostname` from controller.
- Test MPI reachability: `mpirun -np 2 --host nodeA,nodeB hostname`.
- If you see daemon/connection errors, check PATH and `LD_LIBRARY_PATH` on remote hosts, and ensure matching MPI versions.

Notes
- For reliable benchmarking, run multiple repetitions and run the controller with isolated load.
- If you only have 2 processes but Karatsuba needs 4, run the CPU version or use the regular MPI distribution.
