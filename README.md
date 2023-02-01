# Computer Networks
## Computer Assignment 2

In this assignment we are about to build a wireless topology containing 3 sender, 3 receivers, and 1 load balancer.

Sender sand data to load balancer, and load balancer sends data to receivers.

Sender connects to load balancer using UDP, and load balancer connects to receivers using TCP.

### Topology

![Topology](Assets/Topology.png)

We will test this topology with 1Mbps, 10Mbps, and 100Mbps bandwidth and with error rate of
0.0%, 0.000001%, 0.00001%, 0.0001%, and 0.001%.

We expect higher throughput with higher bandwidth and lower throughput with more error rate.

<!---I hate this project
My teammate left me empty-handed--->