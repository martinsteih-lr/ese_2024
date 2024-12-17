# Log der Untersuchungen

```bash
tcpdump -i end0 dst 239.255.255.2 -X
``` 

```bash
trace-cmd record -e irq:softirq_raise -e irq:softirq_exit -e irq:softirq_entry -e irq:irq_handler_exit -e irq:irq_handler_entry -e sched -e syscalls:sys_exit_clock_nanosleep -e syscalls:sys_enter_clock_nanosleep -e syscalls:sys_exit_clock_gettime -e syscalls:sys_enter_clock_gettime -e sock:inet_sk_error_report -e sock:inet_sock_set_state -e net:netif_receive_skb_list_exit -e net:netif_rx_exit -e net:netif_receive_skb_exit -e net:napi_gro_receive_exit -e net:napi_gro_frags_exit -e net:netif_rx_entry -e net:netif_receive_skb_list_entry -e net:netif_receive_skb_entry -e net:napi_gro_receive_entry -e net:napi_gro_frags_entry -e net:netif_rx -e net:netif_receive_skb -e net:net_dev_queue -e net:net_dev_xmit_timeout -e net:net_dev_xmit -e net:net_dev_start_xmit

trace-cmd report -R -i trace.dat > trace_name.raw
``` 

## Todo

* Create some kind of StreamType for cli params 
* Stream Data fields
  * Mapping of EtherCat and application data
  * Mapping of EtherCat Domain and PDO data from certain streams