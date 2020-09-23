/**
 * @network_util
 * @author  Charan Reddy BOdennagari <charanre@buffalo.edu>
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
 * Network I/O utility functions. send/recvALL are simple wrappers for
 * the underlying send() and recv() system calls to ensure nbytes are always
 * sent/received.
 * n_manager_2.txt
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <fstream>
#include <unistd.h>
#include <sys/time.h>

#include "../include/control_handler.h"
#include "../include/global.h"
#include "../include/network_util.h"
#include "../include/control_header_lib.h"
#include "../include/author.h"
#include "../include/connection_manager.h"
#include "../include/file_transfer.h"

#define DATA_HEADER_LEN 12
#define DATA_PAYLOAD_LEN 1024

using namespace std;
vector<struct stats> send_stats;
FILE * recv_fd = NULL;
char * last_packet= (char *) malloc(DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
char * penultimate_packet= (char *) malloc(DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
char * current_packet= (char *) malloc(DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
char file_dum[]="/tmp/file_transfer.txt";



uint16_t get_next_hop(uint16_t router_id){
	for (int i =0; i<num_nodes ;i++){
		if(routing_table[i].router_id == router_id)
			return routing_table[i].next_hop;
	}
}

int create_data_connection(char ip_address[INET_ADDRSTRLEN], uint16_t portno){
	 ofstream fileout;
        fileout.open(file_dum, ios::app );
	gettimeofday(&currtime,NULL);
	fileout<< "create connection "<<ip_address<<" "<<portno<<" time "<<currtime.tv_sec<<endl;
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in dst_address;
	dst_address.sin_family = AF_INET;
	dst_address.sin_port = htons(portno);
	inet_pton(AF_INET,ip_address,&dst_address.sin_addr);
	fileout<<"calling connect for sock "<<sock_fd<<endl;
	if(connect(sock_fd, (struct sockaddr *) &dst_address, sizeof(dst_address))<0){
		gettimeofday(&currtime,NULL);
		fileout<<"ERROR(\"create data connection failed\")"<<" "<<currtime.tv_sec<<endl;	

	}
	fileout<<"file_transfer socket created "<<sock_fd<<" "<<currtime.tv_sec<<endl;
	fileout.close();
return sock_fd;

}
void send_file(int sock_index, char * cntrl_payload, int payload_len){
	 ofstream fileout;
        fileout.open(file_dum, ios::app );
	fileout<<"inside send file"<<endl;
	struct stats new_struct;
	char * filename = (char *) malloc(payload_len-7); 
	memset(filename,'\0',payload_len-7);
/*	char * last_packet= (char *) malloc(DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	char * penultimate_packet= (char *) malloc(DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	char * current_packet= (char *) malloc(DATA_HEADER_LEN+DATA_PAYLOAD_LEN);*/
	char * file_header = (char *) malloc (DATA_HEADER_LEN);
	char * file_payload = (char *) malloc (DATA_PAYLOAD_LEN);
	memset(last_packet,0,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	memset(penultimate_packet,0,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	memset(current_packet,0,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	memset(file_header,0,DATA_HEADER_LEN);
	memset(file_payload,0,DATA_PAYLOAD_LEN);
	FILE *send_fd;
	char destip[INET_ADDRSTRLEN],next_hop_ip[INET_ADDRSTRLEN];
	uint8_t ttl,t_id,fin,mid = 0;
	uint32_t dst_ip_int,next_hop_ip_int;
	uint16_t seq_num,cur_seq,next_hop,dst_id,dst_port,send_sock;
	int file_size,send_bytes=0;
	fin = 1<<7;

	memcpy(&destip,cntrl_payload,4);
	memcpy(&dst_ip_int,cntrl_payload,4);
	memcpy(&ttl,cntrl_payload+4,1);
	memcpy(&t_id,cntrl_payload+5,1);
	memcpy(&seq_num,cntrl_payload+6,2);
	memcpy(filename,cntrl_payload+8,payload_len-8);
	inet_ntop(AF_INET, destip,destip,INET_ADDRSTRLEN);
	seq_num = ntohs(seq_num);
	cur_seq = seq_num;
	gettimeofday(&currtime,NULL);
	cout<<"file transfer req for "<<destip<<" time "<<currtime.tv_sec<<" "<<seq_num<<" "<<filename<<endl;	
	fileout<<"file transfer req for "<<destip<<" time "<<currtime.tv_sec<<" "<<seq_num<<" "<<filename<<endl;	
	send_fd = fopen(filename,"rb");
	if(send_fd == NULL)
		fileout<<"file open failed"<<endl;
	fileout<<"opened file descriptor"<<endl;
	for(int i =0 ; i<num_nodes; i++){
		if(!strcmp(routing_table[i].ip_addr, destip)){
			dst_id = routing_table[i].router_id;
			next_hop = routing_table[i].next_hop;
			dst_port = routing_table[i].data_port;
			break;
		}
	}
	fileout<< "next_hop id for destination is "<<next_hop<<endl;
	for (int i =0 ; i<num_nodes; i++){
		if(routing_table[i].router_id == next_hop){
			strcpy(next_hop_ip,routing_table[i].ip_addr);
			dst_port = routing_table[i].data_port;
			break;
		}
	}
	fileout<<"next hop ip is "<<destip<<" "<<dst_port<<endl;
	fseek(send_fd,0,SEEK_END);
	fileout<<"fseek done "<<ftell(send_fd)<<endl;
	file_size = ftell(send_fd);
	rewind(send_fd);
	fileout<<"rewind done file size is "<<file_size<<" "<<send_bytes<<endl;
	inet_pton(AF_INET,destip,&dst_ip_int);
	send_sock = create_data_connection(next_hop_ip,dst_port);
	gettimeofday(&currtime,NULL);
	fileout<<"socket created is "<<send_sock<<" time "<<currtime.tv_sec<<endl;
	char * init_cntrl_resp;
        init_cntrl_resp = create_response_header(sock_index,5,0,0);
        sendALL(sock_index,init_cntrl_resp,CNTRL_RESP_HEADER_SIZE);
        free(init_cntrl_resp);
	while(send_bytes<file_size){
	gettimeofday(&currtime,NULL);
		fileout<<"enter while loop sent bytes "<<send_bytes<<" cur_seq "<<cur_seq<<" time "<<currtime.tv_sec<<endl;	
		fread(file_payload,DATA_PAYLOAD_LEN,1,send_fd);
		if(send_bytes+1024 < file_size){
			cur_seq = htons(cur_seq);
			memcpy(file_header,&dst_ip_int,4);
			memcpy(file_header+4,&t_id,1);
			memcpy(file_header+5,&ttl,1);
			memcpy(file_header+6,&cur_seq,2);
			memcpy(file_header+8,&mid,1);

		}
		else{
			fileout<<"sending the final packdet with fin set"<<endl;
			cur_seq = htons(cur_seq);
                        memcpy(file_header,&dst_ip_int,4);
                        memcpy(file_header+4,&t_id,1);
                        memcpy(file_header+5,&ttl,1);
                        memcpy(file_header+6,&cur_seq,2);
                        memcpy(file_header+8,&fin,1);
			new_struct.end_seq = ntohs(cur_seq);
			fileout<<"sending the final packdet with fin set "<<ntohs(cur_seq)<<endl;
		}
		cur_seq = ntohs(cur_seq);
		memcpy(current_packet,file_header,DATA_HEADER_LEN);
		memcpy(current_packet+DATA_HEADER_LEN,file_payload,DATA_PAYLOAD_LEN);
		int bytes = sendALL(send_sock,current_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
		memcpy(penultimate_packet,last_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);		
		memcpy(last_packet,current_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);		
		cur_seq++;	
		send_bytes+=bytes-12;
	}
	
	new_struct.src_id = my_id;
	new_struct.dst_id = dst_id;
	new_struct.transfer_id = t_id;
	new_struct.ttl = ttl;
	new_struct.start_seq = seq_num;
	send_stats.push_back(new_struct);
	fileout<<"end of file transfer "<<new_struct.start_seq<<" "<<new_struct.end_seq<<endl;
	close(send_sock);
	fileout.close();

	
}



bool receive_data(int sock_index){
	 ofstream fileout;
        fileout.open(file_dum, ios::app );
	fileout<<"inside receive data"<<endl;
	char * packet_header = (char*)malloc(DATA_HEADER_LEN);

	if(recvALL(sock_index,packet_header,DATA_HEADER_LEN)<0){
		remove_data_conn(sock_index);
		return false;
	
	}
	char destip[INET_ADDRSTRLEN];
	uint8_t ttl,t_id,fin;
	uint16_t seq_num; 
	memcpy(destip,packet_header,4);
	memcpy(&ttl,packet_header+5,1);	
	memcpy(&t_id,packet_header+4,1);	
	memcpy(&seq_num,packet_header+6,2);	
	inet_ntop(AF_INET,destip,destip,INET_ADDRSTRLEN);
	fileout<<"destination ip is "<<destip<<endl;
	if(!strcmp(destip,my_ip)){
		store_file(sock_index,packet_header);
	}
	else{
		forward_packet(sock_index, packet_header);
	}
	free(packet_header);
	fileout.close();
	
}
void store_file(int sock_index,char * recv_header){
	 ofstream fileout;
        fileout.open(file_dum, ios::app );
	fileout<<"inside store file"<<endl;	
	uint16_t payload_len = DATA_HEADER_LEN+DATA_PAYLOAD_LEN;
	char * packet_header = (char*)malloc(DATA_HEADER_LEN);
	char * packet_payload = (char*) malloc(DATA_PAYLOAD_LEN);
	FILE * recv_fd=NULL;
	memcpy(packet_header,recv_header,DATA_HEADER_LEN);
	fileout<<"got the header "<<endl;
	//recvALL(sock_index,packet_header,DATA_HEADER_LEN);
	fileout<<"received payload "<<endl;
	char destip[INET_ADDRSTRLEN];
	uint8_t ttl,t_id,fin=0,k=0;
	uint16_t seq_num,prev_seq=0; 
	memcpy(destip,packet_header,4);
	memcpy(&ttl,packet_header+5,1);	
	memcpy(&t_id,packet_header+4,1);	
	memcpy(&seq_num,packet_header+6,2);	
	seq_num = htons(seq_num);
	fileout<<"headers copied "<<destip<<" "<<ttl<<" "<<t_id<<" "<<seq_num<<endl;
	

	if(recv_fd == NULL){
		struct stats new_conn;
		new_conn.transfer_id = t_id;
		new_conn.start_seq = seq_num;
		fileout<<"recv_fd is null creating fd"<<endl;
		char fileid[8];
		sprintf(fileid, "%d", t_id);
		fileout<<"t_id converted "<<t_id<<endl;
		char filename[64];
		strcpy(filename,"file-");
		strcat(filename,fileid);
		recv_fd = fopen(filename,"wb");
		fileout<<"file write_fd created "<<endl;
		send_stats.push_back(new_conn);
	}
	recvALL(sock_index,packet_payload,DATA_PAYLOAD_LEN);
	if(ttl!=0){
		fwrite(packet_payload,1,DATA_PAYLOAD_LEN,recv_fd);
	}
	fileout<<"writing the initial bytes "<<endl;
	while(1){
		fileout<<"packet received inside while "<<++k<<endl;
		recvALL(sock_index,packet_header,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
		memcpy(packet_payload,packet_header+DATA_HEADER_LEN,DATA_PAYLOAD_LEN);
		memcpy(&fin,packet_header+8,1);
		memcpy(&ttl,packet_header+5,1);	
		memcpy(penultimate_packet,last_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
                memcpy(last_packet,packet_header,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
		memcpy(&seq_num,packet_header+6,2);	
		seq_num = htons(seq_num);
		//if(seq_num > prev_seq){
		fileout<<"writing to file with seq num "<<seq_num<<endl;
		if(ttl!=0){
			fwrite(packet_payload,1,DATA_PAYLOAD_LEN,recv_fd);
		}
		else{
			cout<<"ttl expired"<<endl;
		}
		prev_seq = seq_num;
		//}
		char finish[8];
		sprintf(finish, "%d", fin);
		fileout<<"finish bit is "<<finish<<endl;	
		fin = fin>>7;
		fin = fin||0;
		if(fin == 1){
                        fileout<<"finish bit received send_stats contain "<<send_stats.size()<<endl;
			fclose(recv_fd);
			for(int i =0; i<send_stats.size();i++){
				if(send_stats[i].transfer_id==t_id){
					send_stats[i].end_seq = seq_num;
					fileout<<"Setting the end_Seq "<<send_stats[i].end_seq<<endl;
				}
			}
			break;
		}
		
	}
	for(int i = 0;i<send_stats.size();i++){
		if(send_stats[i].transfer_id==t_id){
			fileout<<"file written "<<send_stats[i].start_seq<<" "<<send_stats[i].end_seq<<endl;
		}
	}
	free(packet_header);
	free(packet_payload);
	fileout.close();
	
}

void forward_packet(int sock_index, char * recv_header){
	 ofstream fileout;
        fileout.open(file_dum, ios::app );
	fileout<<"entered forward packet"<<endl;
	uint16_t payload_len = DATA_HEADER_LEN+DATA_PAYLOAD_LEN;
	char * packet_header = (char*)malloc(DATA_HEADER_LEN);
	char * packet_payload = (char*) malloc(DATA_PAYLOAD_LEN);
	char destip[INET_ADDRSTRLEN];
	uint8_t ttl,t_id,fin=0,k=0;
	uint16_t seq_num,dst_id,dst_port,prev_seq=0; 
	uint32_t dst_ip_int;
	bool existing;
	int dst_sock;
	memcpy(destip,recv_header,4);
	memcpy(&ttl,recv_header+5,1);	
	memcpy(&t_id,recv_header+4,1);	
	memcpy(&seq_num,recv_header+6,2);	
	memcpy(&fin,recv_header+8,1);
	//cout<<"destination ip is "<<destip<<endl;
	inet_ntop(AF_INET,destip,destip,INET_ADDRSTRLEN);
	fileout<<"destination ip is "<<destip<<endl;
	seq_num = htons(seq_num);
	recvALL(sock_index,packet_payload,DATA_PAYLOAD_LEN);
	inet_pton(AF_INET,destip,&dst_ip_int);
	ttl = ttl-1;	
	for(int i = 0; i< send_stats.size(); i++){
		if(send_stats[i].transfer_id == t_id){
			dst_sock = send_stats[i].next_hop_sock;
			existing = TRUE;	
		}
	}

	if(!existing){
		for(int i=0; i<num_nodes; i++){
			cout<<destip<<" "<<routing_table[i].ip_addr<<endl;
			if(!strcmp(routing_table[i].ip_addr,destip)){
				dst_id = routing_table[i].next_hop;
			}
		}
		fileout<<"next hop id for dest is "<<dst_id<<endl;
		for(int i =0 ; i<num_nodes; i++){
                	if(routing_table[i].router_id == dst_id){
                        	//destip = routing_table[i].ip_addr;
				strcpy(destip,routing_table[i].ip_addr);
                        	//next_hop = routing_table[i].next_hop;
                        	dst_port = routing_table[i].data_port;
                	}
        	}
		fileout<<"next hop is "<<destip<<" "<<dst_port<<endl;
		dst_sock = create_data_connection(destip,dst_port); 
		struct stats new_struct;
		new_struct.transfer_id = t_id;
		new_struct.ttl = ttl;
		new_struct.start_seq = seq_num;
		new_struct.end_seq = seq_num-1;
		new_struct.next_hop_sock = dst_sock;
		send_stats.push_back(new_struct);
	}
	memcpy(recv_header,&dst_ip_int,4);
	memcpy(recv_header+5,&ttl,1);
	memcpy(current_packet,recv_header,DATA_HEADER_LEN);
	memcpy(current_packet+DATA_HEADER_LEN,packet_payload,DATA_PAYLOAD_LEN);
	if(ttl!=0){
		sendALL(dst_sock,current_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	}
	while(1){
                recvALL(sock_index,current_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
                memcpy(&fin,current_packet+8,1);
                memcpy(&ttl,current_packet+5,1);
                memcpy(penultimate_packet,last_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
                memcpy(last_packet,current_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
                memcpy(&seq_num,current_packet+6,2);
                seq_num = htons(seq_num);
                /*if(seq_num > prev_seq){*/
                fileout<<"writing to file with seq num "<<seq_num<<endl;
				ttl = ttl-1;
                prev_seq = seq_num;
                /*}*/
		if(ttl!=0){
			sendALL(dst_sock,current_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
		}
		else{
			fileout<<"ttl expired"<<endl;
		}
                char finish[8];
                sprintf(finish, "%d", fin);
                cout<<"finish bit is "<<finish<<endl;
                fin = fin>>7;
                fin = fin||0;
                if(fin == 1){
                        fileout<<"finish bit received send_stats contain "<<send_stats.size()<<endl;
                        for(int i =0; i<send_stats.size();i++){
                                if(send_stats[i].transfer_id==t_id){
                                        send_stats[i].end_seq = seq_num;
                                        fileout<<"Setting the end_Seq "<<send_stats[i].end_seq<<endl;
                                }
                        }
			close(dst_sock);
                        break;
                }
        }	
	free(packet_header);
	free(packet_payload);
	fileout.close();

}


void send_file_stats(int sock_index, char * cntrl_payload, uint16_t payload_len){
	 ofstream fileout;
        fileout.open(file_dum, ios::app );
	uint8_t transfer_id,ttl;
	uint16_t start_seq,end_seq,temp_seq;
	memcpy(&transfer_id,cntrl_payload,1);
	fileout<<"read the transfer_id "<<send_stats.size()<<endl;
	for(int i =0; i<send_stats.size();i++){
		if(send_stats[i].transfer_id==transfer_id){
			start_seq = send_stats[i].start_seq;
			end_seq = send_stats[i].end_seq;
			ttl = send_stats[i].ttl;
			fileout<<"transfer_id found "<<start_seq<<" "<<end_seq<<endl;
		}
	}
	uint16_t num_packets = end_seq-start_seq+1;
	int payload_length = num_packets*2+4;
	char * payload = (char * )malloc(payload_length);
	char * stats_cntrl_header;
	memcpy(payload,&transfer_id,1);
	memcpy(payload+1,&ttl,1);
	int j =0;
	for(int i = start_seq; i<=end_seq;i++){
		temp_seq = htons(i);
		memcpy(payload+4+j*2,&temp_seq,2);
		j++;
		
	}
	char * stats_resp = (char*) malloc (payload_length+CNTRL_RESP_HEADER_SIZE);
        stats_cntrl_header = create_response_header(sock_index,6,0,payload_length);
	memcpy(stats_resp,stats_cntrl_header,CNTRL_RESP_HEADER_SIZE);
	memcpy(stats_resp+CNTRL_RESP_HEADER_SIZE,payload,payload_length);
        sendALL(sock_index,stats_resp,payload_length+CNTRL_RESP_HEADER_SIZE);
	free(payload);
	free(stats_cntrl_header);
	free(stats_resp);
	remove_control_conn(sock_index);
	FD_CLR(sock_index, &master_list);
	fileout.close();
}

void lst_packet_resp(int sock_index){
	 ofstream fileout;
        fileout.open(file_dum, ios::app );
	uint16_t seq_num;
        memcpy(&seq_num,last_packet+6,2);
        seq_num = htons(seq_num);
	fileout<<"requested last packet "<<seq_num<<" "<<CNTRL_RESP_HEADER_SIZE<<endl;
	
	char * last_packet_header = (char *)malloc(CNTRL_RESP_HEADER_SIZE);
	char * last_packet_payload =(char *)malloc(DATA_HEADER_LEN+DATA_PAYLOAD_LEN); 
	char * resp_packet = (char*)malloc(2*DATA_HEADER_LEN+DATA_PAYLOAD_LEN);

	memcpy(last_packet_payload, last_packet, DATA_HEADER_LEN+DATA_PAYLOAD_LEN);

	last_packet_header = create_response_header(sock_index,7,0,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	memcpy(resp_packet,last_packet_header,CNTRL_RESP_HEADER_SIZE);
	memcpy(resp_packet+CNTRL_RESP_HEADER_SIZE, last_packet_payload, DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
        sendALL(sock_index,resp_packet,2*DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
        //sendALL(sock_index,last_packet_header,CNTRL_RESP_HEADER_SIZE);
        //cout<<sendALL(sock_index,last_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN)<<endl;
	free(last_packet_header);
	free(last_packet_payload);
	free(resp_packet);
	remove_control_conn(sock_index);
	FD_CLR(sock_index, &master_list);
	fileout.close();
}


void penultimate_packet_resp(int sock_index){
	 ofstream fileout;
        fileout.open(file_dum, ios::app );
	uint16_t seq_num;
        memcpy(&seq_num,penultimate_packet+6,2);
        seq_num = htons(seq_num);
	fileout<<"requested penultimate packet "<<seq_num<<" "<<CNTRL_RESP_HEADER_SIZE<<endl;

	char * penultimate_packet_header = (char *)malloc(CNTRL_RESP_HEADER_SIZE);
	char * resp_packet = (char*)malloc(2*DATA_HEADER_LEN+DATA_PAYLOAD_LEN);

	//memcpy(penultimate_packet_payload, penultimate_packet, DATA_HEADER_LEN+DATA_PAYLOAD_LEN);

	penultimate_packet_header = create_response_header(sock_index,8,0,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	memcpy(resp_packet,penultimate_packet_header,CNTRL_RESP_HEADER_SIZE);
	memcpy(resp_packet+CNTRL_RESP_HEADER_SIZE, penultimate_packet, DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
        cout<<sendALL(sock_index,resp_packet,2*DATA_HEADER_LEN+DATA_PAYLOAD_LEN)<<endl;
        //sendALL(sock_index,penultimate_packet,DATA_HEADER_LEN+DATA_PAYLOAD_LEN);
	free(penultimate_packet_header);
	free(resp_packet);
	remove_control_conn(sock_index);
	FD_CLR(sock_index, &master_list);
	fileout.close();
}
