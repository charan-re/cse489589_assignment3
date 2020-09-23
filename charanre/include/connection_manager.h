#ifndef CONNECTION_MANAGER_H_
#define CONNECTION_MANAGER_H_

extern int control_socket, router_socket, data_socket;
extern struct timeval currtime,senttime;
void init();
void send_routing_update();
#endif
