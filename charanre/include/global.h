#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <vector>
#include <iostream>
#include <arpa/inet.h>
using namespace std;
typedef enum {FALSE, TRUE} ;

#define ERROR(err_msg) {perror(err_msg); exit(EXIT_FAILURE);}

/* https://scaryreasoner.wordpress.com/2009/02/28/checking-sizeof-at-compile-time/ */
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)])) // Interesting stuff to read if you are interested to know how this works

extern uint16_t CONTROL_PORT,ROUTER_PORT,DATA_PORT,num_nodes,my_id;
extern fd_set master_list, watch_list;
extern int head_fd;

struct router_info{
        char ip_addr[INET_ADDRSTRLEN];
        uint32_t router_ip;
        uint32_t next_hop;
        uint16_t router_id;
        uint16_t router_port;
        uint16_t data_port;
        uint16_t link_cost;
        uint16_t path_cost;
	uint16_t updates;
        bool neighbor;
        };

extern vector <struct router_info> routing_table;
extern struct timeval timeout;
extern uint16_t update_interval;
extern char my_ip[INET_ADDRSTRLEN];
#endif
