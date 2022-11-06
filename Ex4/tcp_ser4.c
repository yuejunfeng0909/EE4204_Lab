/**********************************
tcp_ser.c: the source file of the server in tcp transmission 
***********************************/


#include "headsock.h"

#define BACKLOG 10

void str_ser(int socket_descriptor); // transmitting and receiving function

int main(void)
{
	struct sockaddr_in server_addr, client_addr;

	// prepare the socket
	int socket_descriptor = socket(AF_INET, SOCK_STREAM, 0); //create socket
	if (socket_descriptor <0)
	{
		printf("error in socket!");
		exit(1);
	}
	
	// prepare the address of the server
	server_addr.sin_family = AF_INET; // IPv4 protocol
	server_addr.sin_port = htons(MYTCP_PORT); // change the byte order from host to network (short host) (e.g. big endian to small endian)
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // change the byte order from host to network (long host) (e.g. big endian to small endian)
	bzero(&(server_addr.sin_zero), 8); // set the rest of the struct to 0
	
	// bind the socket with the server address
	int socket_binding_success;
	socket_binding_success = bind(socket_descriptor, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
	if (socket_binding_success < 0)
	{
		printf("error in binding");
		exit(1);
	}
	
	// listen to the socket
	int socket_listening_success;
	socket_listening_success = listen(socket_descriptor, BACKLOG);
	if (socket_listening_success < 0) {
		printf("error in listening");
		exit(1);
	}

	pid_t pid;
	int accepted_socket, socket_addr_len;
	while (1)
	{
		printf("waiting for data\n");
		socket_addr_len = sizeof (struct sockaddr_in);

		// accept the packet
		accepted_socket = accept(socket_descriptor, (struct sockaddr *)&client_addr, &socket_addr_len);
		if (accepted_socket <0)
		{
			printf("error in accept\n");
			exit(1);
		}

		// create a child process to handle the accepted packet
		pid = fork();
		if (pid == 0) 
		{
			// child process
			close(socket_descriptor);
			str_ser(accepted_socket);
			close(accepted_socket);
			exit(0);
		}
		else {
			// parent process
			close(accepted_socket);
		}
	}
	close(socket_descriptor);
	exit(0);
}

void str_ser(int socket_descriptor)
{
	char total_received_buffer[BUFSIZE];
	char packet_buffer[DATALEN];
	int transmission_finished = 0, packet_bytes_received = 0;
	long total_received_bytes=0;
	
	printf("receiving data!\n");

	while(!transmission_finished)
	{
		if ((packet_bytes_received= recv(socket_descriptor, &packet_buffer, DATALEN, 0))==-1)                                   //receive the packet
		{
			printf("error while receiving packet\n");
			exit(1);
		}
		if (packet_buffer[packet_bytes_received-1] == '\0') // if it is the end of the file
		{
			transmission_finished = 1;
			packet_bytes_received--;
		}
		memcpy((total_received_buffer+total_received_bytes), packet_buffer, packet_bytes_received);
		total_received_bytes += packet_bytes_received;
	}

	// prep ack packet
	struct ack_so ack;
	ack.num = 1;
	ack.len = 0;
	packet_bytes_received = send(socket_descriptor, &ack, 2, 0);
	if (packet_bytes_received == -1)
	{
		printf("send error!\n"); //send the ack
		exit(1);
	}

	// write the received data into a file
	FILE *fp;
	if ((fp = fopen ("myTCPreceive.txt","wt")) == NULL)
	{
		printf("File doesn't exit\n");
		exit(0);
	}
	fwrite (total_received_buffer, 1, total_received_bytes, fp); //write data into file
	fclose(fp);
	printf("a file has been successfully received!\nthe total data received is %d bytes\n", (int)total_received_bytes);
}
