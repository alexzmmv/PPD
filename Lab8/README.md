# Lab8: Distributed Shared Memory (DSM)

This lab implements a simple DSM library and a sample main program. It supports:

- Integer variables replicated across subscribed nodes
- Write and compare-and-exchange operations
- Change callbacks delivered in the same total order across all subscribers sharing the same group
- Static subscriptions with per-group leaders for ordering, no centralized server

## Build

```
make
```

## Config

Edit `config.txt` to define nodes and variables.

Example:

```
Nodes:
1 127.0.0.1 5001
2 127.0.0.1 5002
3 127.0.0.1 5003

Variables:
1 0 1,2
2 100 1,2,3
```

- Variable `1` is subscribed by nodes `{1,2}`.
- Variable `2` is subscribed by nodes `{1,2,3}`.
- The leader per group is the lowest node id; the leader totally orders operations for that group.

## Run (local test)

Start background nodes (servers):

```
./dsm_lab8 --id 1 --config config.txt --daemon &
./dsm_lab8 --id 2 --config config.txt --daemon &
```

Then issue operations from a third node:

```
# Write var 2
printf "w 2 123\nq\n" | ./dsm_lab8 --id 3 --config config.txt

# Compare-and-exchange var 2
printf "c 2 123 456\nq\n" | ./dsm_lab8 --id 3 --config config.txt
```

Expected: All nodes subscribed to `var 2` print the same sequence of `COMMIT` callbacks with increasing `seq` numbers.

## Notes

- Only nodes in a variable's subscriber group can write/compare-exchange that variable; attempts by others are rejected.
- Messages are exchanged only among subscribers of that variable.
- Total order is enforced per subscriber group. Variables with identical subscriber sets share the same leader and sequence, ensuring consistent cross-variable ordering for those nodes.
- Computers are assumed non-faulty; no leader election or retransmission logic is implemented.

## Cleanup

To stop background processes:

```
pkill dsm_lab8
```
