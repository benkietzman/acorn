# acorn
This application provides a load balanced and fault tolerant micro-service solution in a networked environment. Each server hosts a gateway Acorn that maintains an active registry of all Acorn micro-services being hosted on the given server. Multiple gateways register themselves with router Acorns which are capable of providing load balancing across the gateways.
