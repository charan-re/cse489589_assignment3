#ifndef CONTROL_HANDLER_H_
#define CONTROL_HANDLER_H_

int create_control_sock();
int create_data_sock();
int new_control_conn(int sock_index);
int new_data_conn(int sock_index);
bool isControl(int sock_index);
bool isData(int sock_index);
bool control_recv_hook(int sock_index);
void init_response(int sock_index, char * cntrl_payload);
void routing_table_resp(int sock_fd, char * cntrl_payload);
void recv_routing_update(int sock_index);
void remove_control_conn(int sock_index);
void remove_data_conn(int sock_index);
#endif
