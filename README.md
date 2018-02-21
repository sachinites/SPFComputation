# SPFComputation
This project is about building the unicast routing table by performing the Shortest path tree computation on an input  Network Topology(Graph). The core design of this project is based on SDN architecture where the routing table construction protocol closely imitates the ISIS(Intermediate System to Intermediate System) protocol. There is a single central SDN controller called instance which have the entire view of the entire network topology, and programmethe forwarding plane (routing tables) of individual nodes.

List of RFCs supported :
1. RFC7490 - Remote LFA
2. RFC7775 - Route preference
3. RFC5280 - Link and node protection LFA
4. RFC2966 - Route leaking
5. RFC5286 - LFA
6. RFC3906 - Traffic engineering shortcuts

How to Run :
1. download the entire source.
2. run 'make all'
3. run - ./rpd executable
4. Follow the command line instructions
