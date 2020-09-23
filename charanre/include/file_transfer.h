#ifndef NETWORK_UTIL_H_
#define NETWORK_UTIL_H_

#endif
struct stats{
	uint16_t src_id;
	uint16_t dst_id;
	uint8_t transfer_id;
	uint8_t ttl;
	uint16_t start_seq;
	uint16_t end_seq;
	uint16_t next_hop_sock;
	FILE * fd;
};
	
void send_file(int sock_index, char * cntrl_payload, int payload_len);
bool receive_data(int sock_index);
void send_file_stats(int sock_index, char * cntrl_payload, uint16_t payload_len);
void lst_packet_resp(int sock_index);
void penultimate_packet_resp(int sock_index);
void store_file(int sock_index,char * recv_header);
void forward_packet(int sock_index, char * recv_header);

