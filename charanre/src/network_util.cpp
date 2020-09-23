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
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <iostream>

#include "../include/global.h"
#include "../include/connection_manager.h"
using namespace std;

ssize_t recvALL(int sock_index, char *buffer, ssize_t nbytes)
{
    cout<<"inside receive all function "<<nbytes<<endl;
    ssize_t bytes = 0;
    bytes = recv(sock_index, buffer, nbytes, 0);
    cout<<"bytes received is "<<bytes<<endl;
    if(bytes == 0) return -1;
    while(bytes != nbytes)
        bytes += recv(sock_index, buffer+bytes, nbytes-bytes, 0);
    cout<<"inside receive all "<<bytes<<" "<<*buffer<<endl;
    return bytes;
}

ssize_t sendALL(int sock_index, char *buffer, ssize_t nbytes)
{
    ssize_t bytes = 0;
    bytes = send(sock_index, buffer, nbytes, 0);
    cout<<"inside send all nbytes "<<nbytes<<" "<<"bytes_sent "<<bytes<<endl;
    if(bytes == 0) return -1;
    while(bytes != nbytes)
        bytes += send(sock_index, buffer+bytes, nbytes-bytes, 0);

    return bytes;
}

ssize_t sendtoALL(char *buffer, uint16_t nbytes, struct sockaddr_in* ip_addr)
{
    cout<<"inside sendtoall function"<<buffer<<endl;
    ssize_t bytes = 0;
    bytes = sendto(router_socket,buffer, nbytes, 0, (struct sockaddr *)ip_addr, sizeof(ip_addr));
    cout<<"initial bytes sent "<<bytes<<endl;

    if(bytes == 0) return -1;
    while(bytes != nbytes)
        bytes += sendto(router_socket, buffer+bytes, nbytes-bytes, 0, (struct sockaddr *)ip_addr, sizeof(ip_addr));
    cout<<"returning bytes sent in sendall "<<bytes<<endl;
    return bytes;
}
