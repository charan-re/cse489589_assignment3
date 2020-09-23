/**
 * @control_handler
 * @author  Swetank Kumar Saha <swetankk@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * Handler for the control plane.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/queue.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "../include/global.h"
#include "../include/connection_manager.h"
#include "../include/network_util.h"
#include "../include/control_header_lib.h"
#include "../include/author.h"

#ifndef PACKET_USING_STRUCT
    #define CNTRL_CONTROL_CODE_OFFSET 0x04
    #define CNTRL_PAYLOAD_LEN_OFFSET 0x06
#endif

#define RU_INFO 8
#define RU_PAYLOAD_LEN 12
#define INIT_INFO 4
#define INIT_PAYLOAD_LEN 12
#define RTABLE_LEN 8
#define TWO 2
#define FOUR 4
#define SIX 6
#define EIGHT 8
#define TEN 10

uint16_t padding = 0;

/* Linked List for active control connections */
struct ControlConn
{
    int sockfd;
    LIST_ENTRY(ControlConn) next;
}*connection, *conn_temp;
LIST_HEAD(ControlConnsHead, ControlConn) control_conn_list;

int create_control_sock()
{
    int sock;
    struct sockaddr_in control_addr;
    socklen_t addrlen = sizeof(control_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        ERROR("socket() failed");

    /* Make socket re-usable */
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");

    bzero(&control_addr, sizeof(control_addr));

    control_addr.sin_family = AF_INET;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_port = htons(CONTROL_PORT);

    if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0)
        ERROR("bind() failed");

    if(listen(sock, 5) < 0)
        ERROR("listen() failed");

    LIST_INIT(&control_conn_list);

    return sock;
}

int create_router_sock(){
	int sock;
	struct sockaddr_in router_addr;
	socklen_t addrlen = sizeof(router_addr);
	sock = socket(AF_INET,SOCK_DGRAM,0);
	if(sock<0)
		ERROR("Socket() failed");
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        	ERROR("setsockopt() failed");

    	bzero(&router_addr, sizeof(router_addr));

    	router_addr.sin_family = AF_INET;
    	router_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	router_addr.sin_port = htons(ROUTER_PORT);

    	if(bind(sock, (struct sockaddr *)&router_addr, sizeof(router_addr)) < 0)
        	ERROR("bind() failed");
	return sock;
}

int new_control_conn(int sock_index)
{
    int fdaccept, caddr_len;
    struct sockaddr_in remote_controller_addr;

    caddr_len = sizeof(remote_controller_addr);
    fdaccept = accept(sock_index, (struct sockaddr *)&remote_controller_addr, &caddr_len);
    if(fdaccept < 0)
        ERROR("accept() failed");

    /* Insert into list of active control connections */
    connection = malloc(sizeof(struct ControlConn));
    connection->sockfd = fdaccept;
    LIST_INSERT_HEAD(&control_conn_list, connection, next);

    return fdaccept;
}

void remove_control_conn(int sock_index)
{
    LIST_FOREACH(connection, &control_conn_list, next) {
        if(connection->sockfd == sock_index) LIST_REMOVE(connection, next); // this may be unsafe?
        free(connection);
    }

    close(sock_index);
}

bool isControl(int sock_index)
{
    LIST_FOREACH(connection, &control_conn_list, next)
        if(connection->sockfd == sock_index) return TRUE;

    return FALSE;
}

bool control_recv_hook(int sock_index)
{
    char *cntrl_header, *cntrl_payload;
    uint8_t control_code;
    uint16_t payload_len;

    /* Get control header */
    cntrl_header = (char *) malloc(sizeof(char)*CNTRL_HEADER_SIZE);
    bzero(cntrl_header, CNTRL_HEADER_SIZE);

    if(recvALL(sock_index, cntrl_header, CNTRL_HEADER_SIZE) < 0){
        remove_control_conn(sock_index);
        free(cntrl_header);
        return FALSE;
    }

    /* Get control code and payload length from the header */
    #ifdef PACKET_USING_STRUCT
        /** ASSERT(sizeof(struct CONTROL_HEADER) == 8) 
          * This is not really necessary with the __packed__ directive supplied during declaration (see control_header_lib.h).
          * If this fails, comment #define PACKET_USING_STRUCT in control_header_lib.h
          */
        BUILD_BUG_ON(sizeof(struct CONTROL_HEADER) != CNTRL_HEADER_SIZE); // This will FAIL during compilation itself; See comment above.

        struct CONTROL_HEADER *header = (struct CONTROL_HEADER *) cntrl_header;
        control_code = header->control_code;
        payload_len = ntohs(header->payload_len);
    #endif
    #ifndef PACKET_USING_STRUCT
        memcpy(&control_code, cntrl_header+CNTRL_CONTROL_CODE_OFFSET, sizeof(control_code));
        memcpy(&payload_len, cntrl_header+CNTRL_PAYLOAD_LEN_OFFSET, sizeof(payload_len));
        payload_len = ntohs(payload_len);
    #endif

    free(cntrl_header);

    /* Get control payload */
    if(payload_len != 0){
        cntrl_payload = (char *) malloc(sizeof(char)*payload_len);
        bzero(cntrl_payload, payload_len);

        if(recvALL(sock_index, cntrl_payload, payload_len) < 0){
            remove_control_conn(sock_index);
            free(cntrl_payload);
            return FALSE;
        }
    }

    /* Triage on control_code */
    switch(control_code){
        case 0: author_response(sock_index);
                break;
	case 1: init_response(sock_index, cntrl_payload);
                break;
	case 2: routing_table_resp(sock_index, cntrl_payload);
		break;
        /* .......
        case 1: init_response(sock_index, cntrl_payload);
                break;

            .........
           ....... 
         ......*/
    }

    if(payload_len != 0) free(cntrl_payload);
    return TRUE;
}


void init_response(int sock_index, char * cntrl_payload){
	
	printf("in init_response function\n");
	memcpy(&num_nodes,cntrl_payload,sizeof(num_nodes));
	num_nodes = ntohs(num_nodes);	
	memcpy(&update_interval,cntrl_payload+TWO,sizeof(update_interval));
	update_interval= ntohs(update_interval);	
	timeout.tv_sec = update_interval;
	timeout.tv_usec = 0;
	gettimeofday(&senttime,NULL);
	

	printf("in init update_intrvl %d no nodes %d\n",update_interval, num_nodes);

	for(int i =0; i<num_nodes;i++){
		memcpy(&routing_table[i].router_id, cntrl_payload+INIT_INFO+i*INIT_PAYLOAD_LEN, sizeof(routing_table[i].router_id)); 
		routing_table[i].router_id = ntohs(routing_table[i].router_id);
		memcpy(&routing_table[i].router_port, cntrl_payload+INIT_INFO+i*INIT_PAYLOAD_LEN+TWO, sizeof(routing_table[i].router_port)); 
		routing_table[i].router_port = ntohs(routing_table[i].router_port);
		memcpy(&routing_table[i].data_port, cntrl_payload+INIT_INFO+i*INIT_PAYLOAD_LEN+FOUR, sizeof(routing_table[i].data_port)); 
		routing_table[i].data_port = ntohs(routing_table[i].data_port);
		memcpy(&routing_table[i].link_cost, cntrl_payload+INIT_INFO+i*INIT_PAYLOAD_LEN+SIX, sizeof(routing_table[i].link_cost)); 
		routing_table[i].link_cost = ntohs(routing_table[i].link_cost);
		memcpy(&routing_table[i].router_ip, cntrl_payload+INIT_INFO+i*INIT_PAYLOAD_LEN+EIGHT, sizeof(routing_table[i].router_ip)); 
		routing_table[i].router_ip = routing_table[i].router_ip;
		routing_table[i].router_ip = ntohl(routing_table[i].router_ip);
		memcpy(&routing_table[i].ip_addr, cntrl_payload+INIT_INFO+i*INIT_PAYLOAD_LEN+EIGHT, sizeof(routing_table[i].ip_addr)); 
		inet_ntop(AF_INET,routing_table[i].ip_addr,routing_table[i].ip_addr,INET_ADDRSTRLEN);


		if(routing_table[i].link_cost == 0){
			strcpy(my_ip, routing_table[i].ip_addr);
			my_id = routing_table[i].router_id;
			routing_table[i].path_cost = routing_table[i].link_cost;
			routing_table[i].next_hop = routing_table[i].router_id;
			routing_table[i].neighbor = FALSE;
			routing_table[i].updates = 0;
			ROUTER_PORT = routing_table[i].router_port;
			DATA_PORT = routing_table[i].data_port;
		}
		else if(routing_table[i].link_cost == 65535){
			routing_table[i].next_hop = 65535;
			routing_table[i].updates = 0;
			routing_table[i].path_cost = routing_table[i].link_cost;
			routing_table[i].neighbor = FALSE;
		}
		else{
			routing_table[i].next_hop = routing_table[i].router_id;
			routing_table[i].path_cost = routing_table[i].link_cost;
			routing_table[i].updates = 0;
			routing_table[i].neighbor = TRUE;
		}
		
	}
	FILE * recv_fd;
	char filename[64];
	strcpy(filename,"/tmp/");
	strcat(filename,my_ip);
	strcat(filename,"_init");
	recv_fd = fopen(filename,"wb");
	fwrite(cntrl_payload,64,1,recv_fd);
	fclose(recv_fd);
	
	char * init_cntrl_resp;
	init_cntrl_resp = create_response_header(sock_index,1,0,0);
	sendALL(sock_index,init_cntrl_resp,CNTRL_RESP_HEADER_SIZE);	
	free(init_cntrl_resp);
/*	gettimeofday(&timeout,NULL);
 *	if(timeout.tv_usec > senttime.tv_usec){
 *		timeout.tv_sec = update_interval-(timeout.tv_sec-senttime.tv_sec)-1;
 *		timeout.tv_usec = 1000000-(senttime.tv_usec+(1000000-timeout.tv_usec-50000));}
 *	else{
 *		timeout.tv_sec = update_interval-(timeout.tv_sec-senttime.tv_sec);
 *		timeout.tv_usec = senttime.tv_usec-timeout.tv_usec;}*/
	timeout.tv_sec = update_interval;
	timeout.tv_usec = 0;
			router_socket = create_router_sock(); 
			FD_SET(router_socket,&master_list);
    			if(router_socket > head_fd)
        			head_fd = router_socket;
/*			data_socket = create_data_sock(); 
			FD_SET(data_socket,&master_list);
    			if(data_socket > head_fd)
        			head_fd = data_socket;*/
}



void routing_table_resp(int sock_fd, char * cntrl_payload){
	printf("in routing_table_resp function\n");
	uint16_t payload_len, packet_len;
	payload_len = num_nodes*RTABLE_LEN;
	char * routing_table_payload = (char *)malloc(payload_len);
	memset(routing_table_payload,'\0',payload_len);
	char * routing_table_header = create_response_header(sock_fd,2,0,payload_len);
	char * routing_table_resp =(char *)malloc(CNTRL_RESP_HEADER_SIZE+payload_len); 
	for(int i =0; i<num_nodes;i++){
		
		uint16_t router_id, next_hop,path_cost;
		router_id = htons(routing_table[i].router_id);
		next_hop = htons(routing_table[i].next_hop);
		path_cost = htons(routing_table[i].path_cost);
		
		memcpy(routing_table_payload+i*RTABLE_LEN, &router_id,TWO);
		memcpy(routing_table_payload+i*RTABLE_LEN+TWO, &padding,TWO);
		memcpy(routing_table_payload+i*RTABLE_LEN+FOUR, &next_hop,TWO);
		memcpy(routing_table_payload+i*RTABLE_LEN+SIX, &path_cost,TWO);
	}
	memcpy(routing_table_resp,routing_table_header,CNTRL_RESP_HEADER_SIZE);
	memcpy(routing_table_resp+CNTRL_RESP_HEADER_SIZE,routing_table_payload,payload_len);
	int bytes_sent = sendALL(sock_fd, routing_table_resp,(CNTRL_RESP_HEADER_SIZE+payload_len));
	free(routing_table_header);
	free(routing_table_resp);
	free(routing_table_payload);

}


void send_routing_update(){
	printf("in send routing update function\n");
	uint16_t payload_len, packet_len,update_fields, source_port,r_port,r_id,r_cost;
	uint32_t sourceip,r_ip;
	struct sockaddr_in dest;
	update_fields = htons(num_nodes);
	source_port = htons(ROUTER_PORT);
	inet_pton(AF_INET,my_ip,&sourceip);
	payload_len = RU_PAYLOAD_LEN*num_nodes+RU_INFO;
	char * routing_update_packet = (char*) malloc(payload_len);
	memset(routing_update_packet, '\0', payload_len);
	memcpy(routing_update_packet,&update_fields,2);
	memcpy(routing_update_packet+TWO,&source_port,2);
	memcpy(routing_update_packet+FOUR,&sourceip,4);
	for(int i =0; i<num_nodes; i++){
		
		if(routing_table[i].updates > 2){
			routing_table[i].path_cost = 65535;
			routing_table[i].link_cost = 65535;
			routing_table[i].next_hop = 65535;
			routing_table[i].neighbor = FALSE;
		}
		
		inet_pton(AF_INET,routing_table[i].ip_addr,&r_ip);
		r_port = htons(routing_table[i].router_port);
		r_id = htons(routing_table[i].router_id);
		r_cost = htons(routing_table[i].path_cost);
		memcpy(routing_update_packet+i*RU_PAYLOAD_LEN+RU_INFO,&r_ip,FOUR);
		memcpy(routing_update_packet+i*RU_PAYLOAD_LEN+RU_INFO+FOUR,&r_port,TWO);
		memcpy(routing_update_packet+i*RU_PAYLOAD_LEN+RU_INFO+SIX,&padding,TWO);
		memcpy(routing_update_packet+i*RU_PAYLOAD_LEN+RU_INFO+EIGHT,&r_id,TWO);
		memcpy(routing_update_packet+i*RU_PAYLOAD_LEN+RU_INFO+TEN,&r_cost,TWO);
			
	}
	for(int i =0;i<num_nodes; i++){
		dest.sin_family = AF_INET;
		dest.sin_port = htons(routing_table[i].router_port);
		
		uint32_t ipaddress;
		//inet_pton(AF_INET,routing_table[i].ip_addr,&ipaddress);
		ipaddress = htonl(routing_table[i].router_ip);
		socklen_t struct_len = sizeof(struct sockaddr_in);
		memcpy(&dest.sin_addr.s_addr,&ipaddress,4);
		int udp_sock = socket(AF_INET,SOCK_DGRAM,0);
		printf("verfying %s, %d %d \n",routing_table[i].ip_addr, routing_table[i].neighbor,routing_table[i].updates);
		if(routing_table[i].neighbor == TRUE && routing_table[i].link_cost!=0){
			printf("sending rupdate to %s\n",routing_table[i].ip_addr);
			routing_table[i].updates =routing_table[i].updates+1;;
			sendto(udp_sock,routing_update_packet, payload_len, 0, (struct sockaddr *)&dest, struct_len);
		}	
		close(udp_sock);	
	}
	return;	
}



void recv_routing_update(int sock_index){

	printf("receive_routing_update_called\n ");
	ssize_t payload_len =RU_PAYLOAD_LEN*num_nodes + RU_INFO; 
	char * buffer = (char *) malloc (payload_len);
	struct sockaddr_in src_addr;
	src_addr.sin_family = AF_INET;	
	socklen_t struct_len = sizeof(struct sockaddr_in);
	int bytes = recvfrom(sock_index,buffer,payload_len,0,(struct sockaddr *)&src_addr, &struct_len);
	uint16_t num_fields,src_port;
	uint32_t src_ipaddr;
	char src_ip[INET_ADDRSTRLEN];
	int remote_cost,remote_id;
	memcpy(&num_fields,buffer,TWO);
	memcpy(&src_port,buffer+TWO,TWO);
	memcpy(&src_ipaddr,buffer+FOUR,FOUR);
	memcpy(src_ip,buffer+FOUR,FOUR);
	num_fields = ntohs(num_fields);

	FILE * recv_fd;
	char filename[64];
	strcpy(filename,"/tmp/");
	strcat(filename,my_ip);
	strcat(filename,"_recv");
	recv_fd = fopen(filename,"wb");
	fwrite(buffer,payload_len,1,recv_fd);
	fclose(recv_fd);

	src_port = ntohs(src_port);
	inet_ntop(AF_INET,src_ip,src_ip,INET_ADDRSTRLEN);
	for(int i = 0 ; i<num_nodes;i++){
		if(strcmp(routing_table[i].ip_addr,src_ip)==0 && routing_table[i].router_port == src_port){
			remote_cost = routing_table[i].link_cost;
			remote_id = routing_table[i].router_id;
			routing_table[i].updates = 0;
			break;
		}	
	}
	calculate_dist_vec(buffer,num_fields,remote_cost,remote_id);
	
}
void calculate_dist_vec(char * payload, int num_fields,int remote_cost,int remote_id){
	printf("calculate_distance_vector\n ");
	uint16_t r_id,l_cost;	
	bool change = FALSE;
	for(int i =0; i<num_fields;i++){
		memcpy(&r_id, payload+RU_INFO+i*RU_PAYLOAD_LEN+EIGHT,TWO);
		memcpy(&l_cost, payload+RU_INFO+i*RU_PAYLOAD_LEN+TEN,TWO);
		r_id = ntohs(r_id);
		l_cost = ntohs(l_cost);
		for (int j =0 ; j<num_nodes;j++){
			if(routing_table[i].router_id == r_id){
				if(routing_table[i].next_hop == remote_id){
					routing_table[i].path_cost = (l_cost + remote_cost);
				} 
				else if(routing_table[i].path_cost>(l_cost + remote_cost)){
					routing_table[i].path_cost = (l_cost + remote_cost);
					routing_table[i].next_hop = remote_id;
					change = TRUE;
				}
			}
		}
	}
	for(int i =0; i<num_nodes;i++){
	}
	if(change){
		send_routing_update();
	}
}

