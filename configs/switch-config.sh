#!/bin/bash

bridge vlan add dev swp0 vid 20
bridge vlan add dev swp3 vid 20

bridge fdb add 01:00:5e:7f:ff:01 dev swp0 #vlan 20
bridge fdb add 01:00:5e:7f:ff:02 dev swp3 #vlan 20

tc qdisc replace dev swp0 parent root handle 100 taprio \
num_tc 8 \
map 0 1 2 3 4 5 6 7 \
queues 1@0 1@1 1@2 1@3 1@4 1@5 1@6 1@7 \
base-time 0 \
sched-entry S 7f 1161920 \
sched-entry S 80 500000 \
sched-entry S 7f 338080 \
flags 0x02

tc qdisc replace dev swp3 parent root handle 100 taprio \
num_tc 8 \
map 0 1 2 3 4 5 6 7 \
queues 1@0 1@1 1@2 1@3 1@4 1@5 1@6 1@7 \
base-time 0 \
sched-entry S 7f 1176640 \
sched-entry S 80 500000 \
sched-entry S 7f 338080 \
flags 0x02
