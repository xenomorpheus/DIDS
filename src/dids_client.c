/*
 * This is the DIDS (Duplicate Image Detection System) client.
 * Please see the README file for further details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>

void error(const char *msg) {
	perror(msg);
	exit(0);
}

void help(char *argv[]) {
	fprintf(stderr, "\n");
	fprintf(stderr, "usage %s --hostname localhost --port 10000 COMMAND ARGS \n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "COMMAND and ARGS:\n");
	fprintf(stderr, "     load - load all image info from memory\n");
	fprintf(stderr, "     fullcompare\n");
	fprintf(stderr, "     add external_ref_1 filename_1 \n");
	fprintf(stderr, "     del external_ref_1 \n");
	fprintf(stderr, "     quickcompare filename\n");
	fprintf(stderr, "     unload - unload all ppm info from memory\n");
	fprintf(stderr, "     info   - server info\n");
	fprintf(stderr, "     debug_show_tree - show server image structure\n");
	fprintf(stderr, "     debug_sleep - sleep for a while to test launch of sub-process\n");
	fprintf(stderr, "     quit   - server quit\n");
	fprintf(stderr, "     help   - this message\n");
	fprintf(stderr, "\n");
}

// Read and print reply
void read_and_print_reply(int sockfd){
	int buff_size = 4096;
	char buffer[buff_size];
	int n = 1;
	while (n > 0) {
		bzero(buffer, buff_size);
		n = read(sockfd, buffer, buff_size);
		if (n < 0){
			error("ERROR reading from socket");
		}
		if (n > 0)
			printf("%s", buffer);
	}
}

/* Flag set by ‘--verbose’. Not currently supported. */
static int verbose_flag;

int main(int argc, char **argv) {
	int portno = 10000; // default port
	struct hostent *server = gethostbyname("localhost");  // default host
	int n;
	struct sockaddr_in serv_addr;
	char *command;
	int sockfd; 

	static struct option long_options[] = {
		/* These options set a flag. */
		{"verbose", no_argument,       &verbose_flag, 1},
		{"brief",   no_argument,       &verbose_flag, 0},
		/* These options don’t set a flag.
		We distinguish them by their indices. */
		{"hostname",  required_argument, 0, 'h'},
		{"port",  required_argument, 0, 'p'},
		{0, 0, 0, 0}
	};
	while (1){
		/* getopt_long stores the option index here. */
		int option_index = 0;
		int c = getopt_long (argc, argv, "h:p:", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c) {
			case 'h':
				server = gethostbyname(optarg);
				break;

			case 'p':
				portno = atoi(optarg);
				break;

			case '?':
				/* getopt_long already printed an error message. */
				break;

			default:
				abort ();
		}
	}

	/* Instead of reporting ‘--verbose’
	and ‘--brief’ as they are encountered,
	we report the final status resulting from them. */
	if (verbose_flag)
		puts ("verbose flag is set");

	// Invalid arg count
	int arg_count = argc - optind;
	if ( arg_count <= 0 ){
		help(argv);
		exit(0);
	}
	command = argv[optind];

	// help
	if (strcmp(command, "help") == 0) {
		help(argv);
		exit(0);
	}

	// Setup the socket
	if (server == NULL) {
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		error("ERROR opening socket");
		exit(0);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr,
			server->h_length);
	serv_addr.sin_port = htons(portno);
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");

	int buff_size = 4096;
	char command_and_args_buffer[buff_size];

	// Commands without arguments:
	// info, quit, load, fullcompare, unload, debug_show_tree, debug_sleep.
	if ((strcmp(command, "info") == 0)
			|| (strcmp(command, "quit") == 0)
			|| (strcmp(command, "load") == 0)
			|| (strcmp(command, "fullcompare") == 0)
			|| (strcmp(command, "unload") == 0)
			|| (strcmp(command, "debug_sleep") == 0)
			|| (strcmp(command, "debug_show_tree") == 0)) {

		snprintf(command_and_args_buffer, buff_size, "%s\n", command);

		// Send to server
		n = write(sockfd, command_and_args_buffer, strlen(command_and_args_buffer));
		if (n < 0)
			error("ERROR writing to socket");

		read_and_print_reply(sockfd);
	}

	// Add new PPMs to SQL by reading image from file.
	else if (strcmp(command, "add") == 0) {

		if (arg_count < 3) {
			fprintf(stderr,
					"usage %s [options] add external_ref_1 filename_1\n",
					argv[0]);
			exit(0);
		}
		char *external_ref = argv[optind + 1];
		char *filename     = argv[optind + 2];

		snprintf(command_and_args_buffer, buff_size, "%s %s %s\n", command, external_ref,
				filename);

		// Send to server
		n = write(sockfd, command_and_args_buffer, strlen(command_and_args_buffer));
		if (n < 0)
			error("ERROR writing to socket");

		read_and_print_reply(sockfd);
	}

	// Del PPM from SQL
	else if (strcmp(command, "del") == 0) {

		if (arg_count < 2) {
			fprintf(stderr, "usage %s [options] del external_ref_1\n",
					argv[0]);
			exit(0);
		}
		char *external_ref = argv[optind + 1];

		snprintf(command_and_args_buffer, buff_size, "%s %s\n", command, external_ref);

		// Send to server
		n = write(sockfd, command_and_args_buffer, strlen(command_and_args_buffer));
		if (n < 0)
			error("ERROR writing to socket");

		read_and_print_reply(sockfd);
	}

	// Compare a file to existing PPMs in SQL.
	else if (strcmp(command, "quickcompare") == 0) {

		if (arg_count < 2) {
			fprintf(stderr, "usage %s [options] quickcompare filename\n",
					argv[0]);
			exit(0);
		}
		char *filename = argv[optind + 1];

		snprintf(command_and_args_buffer, buff_size, "%s %s\n", command, filename);

		// Send to server
		n = write(sockfd, command_and_args_buffer, strlen(command_and_args_buffer));
		if (n < 0)
			error("ERROR writing to socket");

		read_and_print_reply(sockfd);
	} else {
		help(argv);
	}

	close(sockfd);
	return 0;
}
