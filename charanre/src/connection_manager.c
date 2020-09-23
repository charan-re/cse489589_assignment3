/**
 * @connection_manager
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
 * Connection Manager listens for incoming connections/messages from the
 * controller and other routers and calls the desginated handlers.
 */

#include <sys/select.h>
#include <sys/time.h>

#include "../include/connection_manager.h"
#include "../include/global.h"
#include "../include/control_handler.h"

/*fd_set master_list, watch_list;
int head_fd;*/

void main_loop()
{
    int selret, sock_index, fdaccept;
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    while(TRUE){
        watch_list = master_list;
        selret = select(head_fd+1, &watch_list, NULL, NULL, &timeout);
	gettimeofday(&currtime,NULL);
	printf("current time is %ld %ld timeout %d %d\n",currtime.tv_sec,currtime.tv_usec,timeout.tv_sec,timeout.tv_usec);
        if(selret < 0)
            ERROR("select failed.");

        /* Loop through file descriptors to check which ones are ready */
        for(sock_index=0; sock_index<=head_fd; sock_index+=1){
	printf("select returned %d sock_index  %ld\n",selret, sock_index);
		if(selret==0){
			printf("checking if selret is 0\n");
			gettimeofday(&currtime,NULL);
			printf("current time is %ld %ld timeout %d %d\n",currtime.tv_sec,currtime.tv_usec,timeout.tv_sec,timeout.tv_usec);
			send_routing_update();
			timeout.tv_sec = update_interval;
			timeout.tv_usec = 0;
			break;
	    	}

            if(FD_ISSET(sock_index, &watch_list)){

                /* control_socket */
                if(sock_index == control_socket){
		    printf("the selected socket is the control socketi\n ");
		gettimeofday(&currtime,NULL);
		printf("current time is %ld %ld timeout %d %d\n",currtime.tv_sec,currtime.tv_usec,timeout.tv_sec,timeout.tv_usec);
                    fdaccept = new_control_conn(sock_index);

                    /* Add to watched socket list */
                    FD_SET(fdaccept, &master_list);
                    if(fdaccept > head_fd) head_fd = fdaccept;
                }

                /* router_socket */
                else if(sock_index == router_socket){
		    printf("the selected socket is the router socket\n ");
		    recv_routing_update(sock_index);
                    //call handler that will call recvfrom() .....
                }

                /* data_socket */
                else if(sock_index == data_socket){
                    //new_data_conn(sock_index);
                }

                /* Existing connection */
                else{
                    if(isControl(sock_index)){
                        if(!control_recv_hook(sock_index)) FD_CLR(sock_index, &master_list);
		    	printf("the selected socket is the control connection\n ");
			if(ROUTER_PORT!=0 && router_socket ==0){
				printf("calling create_router_socket in mainloop");
				create_router_sock();
			}
			gettimeofday(&currtime,NULL);
			printf("current time is %ld %ld timeout %d %d\n",currtime.tv_sec,currtime.tv_usec,timeout.tv_sec,timeout.tv_usec);
                    }
                    //else if isData(sock_index);
                    else ERROR("Unknown socket index");
                }
            }
        }
    }
}

void init()
{
    control_socket = create_control_sock();

    //router_socket and data_socket will be initialized after INIT from controller

    FD_ZERO(&master_list);
    FD_ZERO(&watch_list);

    /* Register the control socket */
    FD_SET(control_socket, &master_list);
    head_fd = control_socket;

    main_loop();
}
