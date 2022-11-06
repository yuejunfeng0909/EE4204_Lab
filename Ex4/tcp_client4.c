#include "headsock.h"

// transmit data
float transmit_file(FILE *file_transmitted, int socket_descriptor, long *len);
//calcu the time interval between out and in
void calc_interval(struct timeval *out, struct timeval *in);

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("parameters not match");
	}

	// get host's information
	struct hostent *socket_host;
	socket_host = gethostbyname(argv[1]);
	if (socket_host == NULL) {
		printf("error when gethostby name");
		exit(0);
	}

	// print the remote host's information
	printf("host name: %s\n", socket_host->h_name); 
	
	// get the host's alternative names
	// TODO: find out what this is
	char ** pptr;
	for (pptr=socket_host->h_aliases; *pptr != NULL; pptr++)
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

	// create the socket
	int socket_descriptor;
	socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);

	// check if socket creation was successful
	if (socket_descriptor <0)
	{
		printf("error in socket");
		exit(1);
	}

	// socket configuration
	struct sockaddr_in communication_socket;
	communication_socket.sin_family = AF_INET; // set the address type to IPv4                                                  
	communication_socket.sin_port = htons(MYTCP_PORT); // set transport protocol port number
	memcpy(&(communication_socket.sin_addr.s_addr), *host_address, sizeof(struct in_addr)); // set the host's IP address
	bzero(&(communication_socket.sin_zero), 8); // researved for system use

	// setup socket connection
	int socket_connection_status;
	socket_connection_status = connect(socket_descriptor, (struct sockaddr *)&communication_socket, sizeof(struct sockaddr)); // connect the socket with the host
	
	// disconnect if connection failed
	if (socket_connection_status != 0) {
		printf ("connection failed\n"); 
		close(socket_descriptor); 
		exit(1);
	}
	
	FILE *file_transmitted;
	// read the file to be transmitted
	if((file_transmitted = fopen ("myfile.txt","r+t")) == NULL)
	{
		printf("File doesn't exit\n"); // exit if file doesn't exist
		exit(0);
	}

	// transmit the file and calculate the time interval
	float time_interval, throughput;
	long file_size;
	time_interval = transmit_file(file_transmitted, socket_descriptor, &file_size);                       //perform the transmission and receiving
	throughput = (file_size/(float)time_interval);                                         //caculate the average transmission rate
	printf("Time(ms) : %.3f, Data sent(byte): %d\nData rate: %f (Kbytes/s)\n", time_interval, (int)file_size, throughput);

	// close the socket and file
	close(socket_descriptor);
	fclose(file_transmitted);
	exit(0); // end of transmission
}

float transmit_file(FILE *file_transmitted, int socket_descriptor, long *len)
{
	float time_inv = 0.0;

	// get the file size
	fseek(file_transmitted, 0, SEEK_END); // go to the end of the file
	long file_size = ftell (file_transmitted); // get file size
	rewind(file_transmitted); // reset the file pointer to the beginning of the file
	printf("The file length is %d bytes\n", (int)file_size);
	printf("the packet length is %d bytes\n",DATALEN);

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
	char sends[DATALEN]; // buffer for the data to be transmitted in each packet
	while(character_index <= file_size)
	{
		// check if the remaining data is less than the packet size
		if ((file_size - character_index + 1) <= DATALEN)
			n_bytes_packet = file_size - character_index + 1;
		else 
			n_bytes_packet = DATALEN;
		
		// load the data into the packet
		memcpy(sends, (buffer+character_index), n_bytes_packet);

		// send the packet
		transmission_status = send(socket_descriptor, &sends, n_bytes_packet, 0);

		// check if the packet was sent successfully
		if(transmission_status == -1) {
			printf("send error!");
			// TODO: retransmission?
			exit(1);
		}
		character_index += n_bytes_packet;
	}

	// receive acknowledgement
	struct ack_so ack;
	if ((transmission_status = recv(socket_descriptor, &ack, 2, 0))==-1)                                   //receive the ack
	{
		printf("error when receiving\n");
		exit(1);
	}

	// check acknowledgement
	if (ack.num != 1|| ack.len != 0)
		printf("error in transmission\n");

	// compute the time interval
	gettimeofday(&transmission_end_time, NULL);
	calc_interval(&transmission_end_time, &transmission_start_time);                                                                 // get the whole trans time
	time_inv += (transmission_end_time.tv_sec)*1000.0 + (transmission_end_time.tv_usec)/1000.0;
	
	// update the number of bytes sent
	*len= character_index;

	return(time_inv);
}

void calc_interval(struct  timeval *out, struct timeval *in)
{
	// if the microsecond difference is negative, borrow from seconds
	if ((out->tv_usec -= in->tv_usec) <0)
	{
		--out ->tv_sec;
		out ->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}
