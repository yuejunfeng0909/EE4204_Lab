#include "headsock.h"

int data_unit_size, batch_ack_size;

// transmit data
float transmit_file(FILE *file_transmitted, int socket_descriptor, long *len);
//calcu the time interval between end_time and in
void calc_interval(struct timeval *end_time, struct timeval *start_time);

int main(int argc, char **argv) // host_name, data_unit_size, batch_ack_size
{
	if (argc != 4) {
		printf("parameters not match\n");
	}

	// get host's information
	struct hostent *socket_host;
	socket_host = gethostbyname(argv[1]);
	if (socket_host == NULL) {
		printf("error when gethostby name\n");
		exit(0);
	}

	// set data unit size
	data_unit_size = atoi(argv[2]);

	// set batch ack size
	batch_ack_size = atoi(argv[3]);

	// print the remote host's information
	printf("host name: %s\n", socket_host->h_name); 
	
	// get the host's alternative names
	// TODO: find out what this is
	char ** pptr;
	for (pptr = socket_host->h_aliases; *pptr != NULL; pptr++)
		printf("the aliases name is: %s\n", *pptr);
	
	// print address type
	switch(socket_host->h_addrtype)
	{
		case AF_INET:
			printf("IPv4 (AF_INET)\n");
		break;
		default:
			printf("unknown address type\n");
		break;
	}

	// get the host's IP address
	struct in_addr **host_address;
	host_address = (struct in_addr **)socket_host->h_addr_list;

	// socket configuration
	struct sockaddr_in communication_socket;
	communication_socket.sin_family = AF_INET; // set the address type to IPv4                                                  
	communication_socket.sin_port = htons(MYTCP_PORT); // set transport protocol port number
	memcpy(&(communication_socket.sin_addr.s_addr), *host_address, sizeof(struct in_addr)); // set the host's IP address
	bzero(&(communication_socket.sin_zero), 8); // researved for system use

	// transmit the file and calculate the time interval
	int repeat_time = 50;
	float time_interval, throughput, averaged_throughput = 0, average_time_interval = 0;
	long file_size_transmitted;

	FILE *file_transmitted;
	// read the file to be transmitted
	if((file_transmitted = fopen ("myfile.txt","r+t")) == NULL)
	{
		printf("File doesn't exit\n"); // exit if file doesn't exist
		exit(0);
	}

	// transmit the file 10 times
	for (int i = 0; i < repeat_time; i++) {
		// create the socket
		int socket_descriptor;
		socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);

		// check if socket creation was successful
		if (socket_descriptor <0)
		{
			printf("error in socket\n");
			exit(1);
		}

		// setup socket connection
		int socket_connection_status;
		socket_connection_status = connect(socket_descriptor, (struct sockaddr *)&communication_socket, sizeof(struct sockaddr)); // connect the socket with the host
		
		// disconnect if connection failed
		if (socket_connection_status != 0) {
			printf ("connection failed\n"); 
			close(socket_descriptor); 
			exit(1);
		}

		time_interval = transmit_file(file_transmitted, socket_descriptor, &file_size_transmitted);
		throughput = file_size_transmitted / time_interval;
		printf("Time(ms) : %.3f, Data sent(byte): %d, Data rate: %.3f (Kbytes/s)\n", time_interval, (int)file_size_transmitted, throughput);

		close(socket_descriptor);

		averaged_throughput += throughput / repeat_time;
		average_time_interval += time_interval / repeat_time;
	}

	printf("Average throughput: %f\n", averaged_throughput);
	printf("Average time interval: %f\n", average_time_interval);

	// close the file
	fclose(file_transmitted);
	exit(0); // end of transmission
}

float transmit_file(FILE *file_transmitted, int socket_descriptor, long *len)
{
	// get the file size
	fseek(file_transmitted, 0, SEEK_END); // go to the end of the file
	long file_size = ftell (file_transmitted); // get file size
	rewind(file_transmitted); // reset the file pointer to the beginning of the file
	// printf("The file length is %d bytes\n", (int)file_size);
	// printf("the packet length is %d bytes\n", data_unit_size);

	// allocate memory and read the file
	char *buffer = (char*)malloc(file_size+1);
	if (buffer == NULL) exit (2); // memory allocation error
	fread(buffer, 1, file_size, file_transmitted); // read the file

	// TODO: find out what this is
	buffer[file_size] ='\0'; //append the end byte

	// get time at the beginning of transmission
	struct timeval transmission_start_time, transmission_end_time;
	gettimeofday(&transmission_start_time, NULL);

	// start transmitting in packets
	long character_index = 0; // current character index
	int transmission_status, n_bytes_packet;
	int jumping_window_remaining = batch_ack_size;
	struct ack_so ack;
	char sends[data_unit_size]; // buffer for the data to be transmitted in each packet
	while(character_index <= file_size)
	{
		// check if the remaining data is less than the packet size
		if ((file_size - character_index + 1) <= data_unit_size)
			n_bytes_packet = file_size - character_index + 1;
		else 
			n_bytes_packet = data_unit_size;
		
		// load the data into the packet
		memcpy(sends, (buffer+character_index), n_bytes_packet);
		// printf("transmit progress: %d/%d\n", (int)character_index, (int)file_size);

		// send the packet
		transmission_status = send(socket_descriptor, &sends, n_bytes_packet, 0);

		// check if the packet was sent successfully
		if(transmission_status == -1) {
			printf("send error!\n");
			// TODO: retransmission?
			exit(1);
		}
		character_index += n_bytes_packet;

		jumping_window_remaining--;
		if (jumping_window_remaining == 0 || character_index == file_size) {
			
			// wait for acknowledgement
			if ((transmission_status = recv(socket_descriptor, &ack, 2, 0)) == -1)                                   //receive the ack
				{
					printf("error while receiving ack\n");
					exit(1);
				}

			// check acknowledgement
			if (ack.num != 1|| ack.len != 0)
				printf("error in transmission\n");

			// restore jumping window back
			jumping_window_remaining = batch_ack_size;
		}
	}

	// compute the time interval
	gettimeofday(&transmission_end_time, NULL);
	calc_interval(&transmission_end_time, &transmission_start_time);                                                                 // get the whole trans time
	float transmission_time = (transmission_end_time.tv_sec)*1000.0 + (transmission_end_time.tv_usec)/1000.0;
	
	// update the number of bytes sent
	*len= character_index;

	return(transmission_time);
}

void calc_interval(struct  timeval *end_time, struct timeval *start_time)
{
	// if the microsecond difference is negative, borrow from seconds
	if ((end_time->tv_usec -= start_time->tv_usec) <0)
	{
		--end_time ->tv_sec;
		end_time ->tv_usec += 1000000;
	}
	end_time->tv_sec -= start_time->tv_sec;
}
