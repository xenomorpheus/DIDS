/*
 * This is the DIDS (Duplicate Image Detection System) server.
 *
 *  Will fork() for longer running operations such as fullcompare and quickcompare.
 *
 *  Will multi-thread when doing fullcompare to make the most of available CPU.
 *
 * Please see the README file for further details.
 *
 *    Copyright (C) 2000-2015  Michael John Bruins, BSc.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#define CLIENT_MAX 100  // Note first two slots are for new incoming connections of IPv4 and IPv6.
#define COMPARE_SIZE  16
#define COMPARE_THRESHOLD 35000 // Lower means images must be more similar to match.
#define CPU_INFO_FILENAME  "/proc/cpuinfo"
#define BUFFER_SIZE 2048
#define COMMAND_LISTEN_TIMEOUT 60 // How long two wait for incoming command.
#define CLIENT_SLOT_FREE -1
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

// Standard
#define _GNU_SOURCE 1 // So we have TEMP_FAILURE_RETRY
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libpq-fe.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <wand/MagickWand.h>

// Custom
#include "ppm.h"

// Each connection to the server will have some details.
typedef struct Client_Info {
	int fd;  // File descriptor or if empty then CLIENT_SLOT_FREE
	char command_buffer[BUFFER_SIZE];
	int cmd_offset;
	pid_t pid;  // Process ID or 0 for no process.
// TODO struct timeval connection_timeout;
} Client_Info;

// Globals
int global_cpu_count = 0;
int global_child_process_count = 0; // Current count of living child processes.
int global_active_connection_count = 0; // Current count of active clients.
Client_Info global_client_detail[CLIENT_MAX];

/*
 * return the number of CPUs on the system
 */
int get_cpu_count(FILE *sock_fh) {
	FILE *fp = fopen(CPU_INFO_FILENAME, "r");
	char buf[4096];
	if (fp == NULL) {
		error(sock_fh, "Failed to open file %s for reading",
		CPU_INFO_FILENAME);
		return 0;
	}

	int cpu_count = 0;
	while (fgets(buf, sizeof buf, fp)) {
		if (strncmp("processor", buf, 9) == 0) {
			cpu_count++;
		}
	}

	fclose(fp);
	return cpu_count;
}

/*
 * load
 *
 * Read in all the PPM from SQL, one at a time.
 *
 * Return 0 on success
 *        non-zero on failure.
 */
int load(FILE *sock_fh, PGconn *psql, PicInfo **picinfo_list_ref) {
	int rc = ppm_load_all_from_sql(sock_fh, psql, picinfo_list_ref);
	if ((rc == 0) && (*picinfo_list_ref != NULL)) {
		rc = similar_but_different_add_to_picinfo_list(sock_fh, psql,
				*picinfo_list_ref);
	}
	return rc;
}

/*
 * add
 *
 * Add a resized image to both sql and into memory.
 *
 * external_ref will be duplicated, so may be free'ed afterwards.
 *
 * 1) ensure the images are all in memory.
 * 2) resize and store the new image in the database
 * 3) add the resized image to the list in memory
 *
 * Return zero on success, non-zero on failure.
 *
 * On success the list will be updated.
 */

int add(FILE *sock_fh, PGconn *psql, PicInfo **ppm_list_ref, char *filename,
		char *external_ref, int new_size) {
	PPM_Info *ppm_miniature;

	fprintf(sock_fh, "DEBUG: add external_ref '%s'\n", external_ref);

	ppm_miniature = ppm_miniature_from_filename(sock_fh, filename, new_size);
	if (!ppm_miniature) {
		error(sock_fh, "add - ppm_miniature_from_filename failed");
		return 1;
	}

	// store it in SQL
	int rc = ppm_store(sock_fh, psql, external_ref, ppm_miniature);
	if (rc) {
		error(sock_fh,
				"add - ppm_store for external_ref '%s' and filename %s, code %d",
				external_ref, filename, rc);
		ppm_info_free(ppm_miniature);
		return 1;
	}

	// Duplicate external_ref as PicInfoBuild will keep a pointer to it.
	external_ref = strdup(external_ref);
	if (!external_ref) {
		error(sock_fh, "add - strdup external_ref failed");
		return 1;
	}

	// add to the list in RAM
	PicInfo *hlp = PicInfoBuild(external_ref, ppm_miniature, NULL);
	if (!hlp) {
		error(sock_fh, "add - PicInfoBuild failed");
		return 1;

	}
	PicInfoAddToList(ppm_list_ref, hlp);
	return 0;
}

/*
 * del
 *
 * Delete a resized image from both sql and memory.
 * *
 * Return updated list on success.
 *        non-zero on failure
 */

int del(FILE *sock_fh, PGconn *psql, PicInfo **ppm_list_ref, char *external_ref) {
	PPM_Info *ppm_miniature;

	fprintf(sock_fh, "DEBUG: del external_ref '%s'\n", external_ref);

	// remove from SQL
	int rc = ppm_del(sock_fh, psql, external_ref);
	if (rc) {
		error(sock_fh, "del - ppm_del for external_ref '%s', code %d",
				external_ref, rc);
		return 1;
	}

	// del from the list in RAM
	rc = PicInfoDeleteFromList(ppm_list_ref, external_ref);
	// code 2 : Deleted from SQL, but not in RAM to delete.
	if (rc){
		if (rc == 2) {
			fprintf(sock_fh, "DEBUG: del could not find external_ref '%s'\n", external_ref);
		}
		else{
			error(sock_fh,
				"del - PicInfoDeleteFromList for external_ref '%s', code %d", external_ref, rc);
			return 2;
		}
	}
	return 0;
}

/*
 * debug_show_tree
 *
 */

void debug_show_tree(FILE *sock_fh, PicInfo *list) {
	PicInfo *current_pic = list;
	while (current_pic) {
		fprintf(sock_fh, "ref: '%s'\n", current_pic->external_ref);
		Similar_but_different *similar_but_different =
				current_pic->similar_but_different;
		while (similar_but_different) {
			fprintf(sock_fh, "   sbd: '%s'\n",
					similar_but_different->external_ref);
			similar_but_different = similar_but_different->next;
		}
		current_pic = current_pic->next;
	}
}

/*
 * Print error to socket and to STDERR.
 * Similar to fprintf except all lines printed will be prefaced with "Error: ".
 */

void error(FILE *sock_fh, const char *format, ...) {
	char buffer[BUFFER_SIZE];
	va_list args;

	va_start(args, format);
	vsnprintf(buffer, (sizeof buffer) - 1, format, args);
	va_end(args);

	fprintf(sock_fh, "Error: %s\n", buffer);
	fprintf(stderr, "Error: %s\n", buffer);
}

/*
 *  Print general diagnostic information.
 *
 *  return non-zero on error.
 */
int info(FILE *sock_fh, PicInfo *list) {
	fprintf(sock_fh, "property: version: 2.10\n");
	unsigned long long image_loaded_count = 0L;
	PicInfo *current_pic = list;
	while (current_pic) {
		image_loaded_count++;
		current_pic = current_pic->next;
	}
	fprintf(sock_fh, "property: image_loaded_count: %llu\n",
			image_loaded_count);
	fprintf(sock_fh, "property: cpu_count: %d\n", global_cpu_count);
	fprintf(sock_fh, "property: child_process_count: %d\n",
			global_child_process_count);
	fprintf(sock_fh, "property: active_connection_count: %d\n",
			global_active_connection_count);
	return 0;
}

/*
 *  Free the linked list of images from RAM.
 *
 */
void unload(PicInfo **list) {
	PicInfo *current_pic = *list;
	PicInfo *next;
	while (current_pic) {
		ppm_info_free(current_pic->picinf);
		next = current_pic->next;
		free(current_pic);
		current_pic = next;
	}
	*list = NULL;
}

/*
 *  Respond to commands requests and perform the commands:
 *
 *  COMMANDS:
 *   quit            : Stop listening for commands.
 *   add             : Learn a new image file by putting a new PPM into SQL and RAM.
 *   del             : Forget a PPM from both SQL and RAM.
 *   fullcompare     : Compare all PPMs in RAM and report potential duplicates.
 *   info            : Print statistics on e.g. how many PPMs in RAM, number of CPUs.
 *   load            : Load all PPM images from SQL into RAM.
 *   quickcompare    : Compare a single file to all PPMs in RAM and report potential duplicates.
 *   unload          : Free all PPM images from RAM.
 *   debug_sleep     : fork() then sleep for 60 seconds. Used to test fork()
 *   debug_show_tree : Show the memory structure of the PPM tree. Used to check structure.
 *
 *  Will fork() for longer running operations such as fullcompare and quickcompare.
 *
 *  Will load all PPMs into RAM before listening for commands.
 *
 *  Args:
 * new_sockfd       : The socket of the client.
 * cmd_buffer       : The buffer holding the command.
 * picinfo_list_ptr : Pointer, to pointer to the memory structure used to hold image details.
 * psql             : A postgreSQL connection.
 * server_loop_ptr  : Pointer to integer used to switch off the server's mail loop.
 * compare_size     : The height (and width) of the PPMs.
 * maxerr           : For images to be considered similar the difference must be below this amount.
 *
 */
void command_process(int new_sockfd, char *cmd_buffer,
		PicInfo **picinfo_list_ptr, PGconn *psql, int *server_loop_ptr,
		int compare_size, unsigned int maxerr) {

	// change to using a filehandle
	FILE *new_sockfh = fdopen(new_sockfd, "w+");
	if (new_sockfh == NULL) {
		error(stderr, "Failed to create file handle from file descriptor");
		return;
	}

	// If the command is load, or a command that requires thumbnails already loaded.
	// Lazy loading of PPMs from SQL into RAM.
	// Most commands require that the thumbnails be loaded into RAM.
	if ( (strcmp(cmd_buffer, "load") == 0) || (
			(!*picinfo_list_ptr)
			&& ((strcmp(cmd_buffer, "fullcompare") == 0)
					|| (strstr(cmd_buffer, "add ") == cmd_buffer)
					|| (strstr(cmd_buffer, "quickcompare ") == cmd_buffer)))) {

		// If command was to load, then report starting to load.
		if (strcmp(cmd_buffer, "load") == 0) {
			fprintf(new_sockfh, "LOAD\n");
			fflush(new_sockfh); // So any human watching sees we are working.
		}

		// If already loaded, just report success
		if (*picinfo_list_ptr){
			fprintf(new_sockfh, "DEBUG: Already loaded\n");
			fprintf(new_sockfh, "LOAD SUCCESS\n");
		}
		// Actually do the loading.
		else{
			// Load all PPMs from SQL into RAM
			int rc = load(new_sockfh, psql, picinfo_list_ptr);
			if (rc) {
				error(new_sockfh, "LOAD failed with code %d\n", rc);
				*server_loop_ptr = 0; // abort loop
			} else {
				// If command was 'load' then report complete.
				if (strcmp(cmd_buffer, "load") == 0) {
					fprintf(new_sockfh, "LOAD SUCCESS\n");
				}
			}
		}
		fflush(new_sockfh); // So any human watching sees we are working.
	}

	// load
	// This is here so that 'load' won't be reported as a bad command.
	if (strcmp(cmd_buffer, "load") == 0) {
		; // already done above
	}

	// quit
	else if (strcmp(cmd_buffer, "quit") == 0) {
		fprintf(new_sockfh, "QUIT\n");
		*server_loop_ptr = 0;
		fprintf(new_sockfh, "QUIT SUCCESS\n");
		fflush(new_sockfh);
	}

	// quickcompare filename  ( detatches )
	else if (strstr(cmd_buffer, "quickcompare ") == cmd_buffer) {
		pid_t fork_rc = fork();
		if (fork_rc < 0) {
			error(new_sockfh,
					"QUICKCOMPARE FAILED, fork() failed. errno=%d, error=%s",
					errno, strerror(errno));
		} else if (fork_rc == 0) { // Child
			// skip the command
			// no space as filenames can contain spaces.
			char *filename = strtok(cmd_buffer + strlen("quickcompare "),
					" \n");
			filename = strdup(filename);
			if (!filename) {
				fprintf(new_sockfh, "QUICKCOMPARE FAILED, no memory\n");
			} else {
				fprintf(new_sockfh, "QUICKCOMPARE\n");
				fprintf(new_sockfh, "DEBUG: '%s'\n", filename);
				fflush(new_sockfh);
				// NOTE increase in maxerr
				int rc = quickcompare(new_sockfh, *picinfo_list_ptr,
						maxerr * 10, filename, compare_size);
				if (rc) {
					fprintf(new_sockfh, "QUICKCOMPARE FAILED, code %d\n", rc);
				} else {
					fprintf(new_sockfh, "QUICKCOMPARE SUCCESS\n");
				}
				fflush(new_sockfh);
				free(filename);
				pthread_exit(NULL);
				exit(0);
			}
		} else { // Parent
			global_child_process_count++;
		}
	}

	// fullcompare ( detatches )
	else if (strcmp(cmd_buffer, "fullcompare") == 0) {
		pid_t fork_rc = fork();
		if (fork_rc < 0) {
			error(new_sockfh,
					"FULLCOMPARE FAILED, fork() failed. errno=%d, error=%s",
					errno, strerror(errno));
		} else if (fork_rc == 0) { // Child
			fprintf(new_sockfh, "FULLCOMPARE\n");
			fflush(new_sockfh);
			// double the CPU count
			int rc = fullcompare(new_sockfh, *picinfo_list_ptr, maxerr,
					global_cpu_count * 2);
			if (rc) {
				fprintf(new_sockfh, "FULLCOMPARE FAILED, code %d\n", rc);
			} else {
				fprintf(new_sockfh, "FULLCOMPARE SUCCESS\n");
			}
			pthread_exit(NULL);
			exit(0);
		} else { // Parent
			global_child_process_count++;
		}
	}

	// add external_ref_1 filename_1
	else if (strstr(cmd_buffer, "add ") == cmd_buffer) {
		char *external_ref = strtok(cmd_buffer + strlen("add "), " \n");
		external_ref = strdup(external_ref); // strtok reuses memory
		if (!external_ref) {
			fprintf(new_sockfh, "ADD FAILED, no memory\n");
		} else {
			// no space as filenames can contain spaces.
			char *filename = strtok(NULL, "\n");
			if (!filename) {
				free(external_ref);
				fprintf(new_sockfh, "ADD FAILED, no memory\n");
			} else {
				filename = strdup(filename); // strtok reuses memory
				if (!filename) {
					free(external_ref);
					fprintf(new_sockfh, "ADD FAILED, no memory\n");
				} else {
					fprintf(new_sockfh, "ADD\n");
					int rc = add(new_sockfh, psql, picinfo_list_ptr, filename,
							external_ref, compare_size);
					if (rc) {
						fprintf(new_sockfh, "ADD FAILED, code %d\n", rc);
					} else {
						fprintf(new_sockfh, "ADD SUCCESS %s %s\n", external_ref,
								filename);
					}
					free(filename);
					free(external_ref);
				}
			}
		}
	}

	// del external_ref_1
	else if (strstr(cmd_buffer, "del ") == cmd_buffer) {
		char *external_ref = strtok(cmd_buffer + strlen("del "), " \n");
		external_ref = strdup(external_ref); // strtok reuses memory
		if (!external_ref) {
			fprintf(new_sockfh, "DEL FAILED, no memory\n");
		} else {
			fprintf(new_sockfh, "DEL\n");
			int rc = del(new_sockfh, psql, picinfo_list_ptr, external_ref);
			if (rc) {
				fprintf(new_sockfh, "DEL FAILED, code %d\n", rc);
			} else {
				fprintf(new_sockfh, "DEL SUCCESS %s\n", external_ref);
			}
			free(external_ref);
		}
	}

	// info
	else if (strstr(cmd_buffer, "info") == cmd_buffer) {
		fprintf(new_sockfh, "INFO\n");
		int rc = info(new_sockfh, *picinfo_list_ptr);
		if (rc) {
			fprintf(new_sockfh, "INFO FAILED, code %d\n", rc);
		} else {
			fprintf(new_sockfh, "INFO SUCCESS\n");
		}
	}

	// unload
	else if (strstr(cmd_buffer, "unload") == cmd_buffer) {
		fprintf(new_sockfh, "UNLOAD\n");
		if (*picinfo_list_ptr) {
			unload(picinfo_list_ptr);
		}
		fprintf(new_sockfh, "UNLOAD SUCCESS\n");
		*picinfo_list_ptr = NULL; // To be sure.
	}

	// debug_show_tree
	else if (strstr(cmd_buffer, "debug_show_tree") == cmd_buffer) {
		fprintf(new_sockfh, "DEBUG_SHOW_TREE\n");
		debug_show_tree(new_sockfh, *picinfo_list_ptr);
		fprintf(new_sockfh, "DEBUG_SHOW_TREE SUCCESS\n");
	}

	// sleep
	// Only used for testing e.g. fork()
	else if (strstr(cmd_buffer, "debug_sleep") == cmd_buffer) {
		pid_t fork_rc = fork();
		if (fork_rc < 0) {
			error(new_sockfh, "fork() failed. errno=%d, error=%s", errno,
					strerror(errno));
		} else if (fork_rc == 0) { // Child
			fprintf(new_sockfh, "DEBUG_SLEEP\n");
			fflush(new_sockfh);
			sleep(COMMAND_LISTEN_TIMEOUT * 2); // longer so we can test COMMAND_LISTEN_TIMEOUT
			fprintf(new_sockfh, "DEBUG_SLEEP SUCCESS\n");
			pthread_exit(NULL);
			exit(0);
		} else { // Parent
			global_child_process_count++;
		}
	}

	// bad command
	else {
		error(new_sockfh, "BAD COMMAND: %s", cmd_buffer);
	}

	fclose(new_sockfh);
}

// Setup a IPV4 network socket to listen on
int create_port_listen_v4(FILE *orig_sockfh, int portno) {
	struct sockaddr_in serv_addr_v4;
	int listening_socketfd_v4 = socket(AF_INET, SOCK_STREAM, 0);
	if (listening_socketfd_v4 == -1) {
		error(orig_sockfh, "opening socket v4, errno=%d, error=%s", errno,
				strerror(errno));
		return 0;
	}
	bzero((char *) &serv_addr_v4, sizeof(serv_addr_v4));
	serv_addr_v4.sin_family = AF_INET;
	serv_addr_v4.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_addr_v4.sin_port = htons(portno);
	errno = 0;
	// should this be (struct sockaddr_in *)
	if (bind(listening_socketfd_v4, (struct sockaddr *) &serv_addr_v4,
			sizeof(serv_addr_v4)) == -1) {
		error(orig_sockfh, "bind() v4, errno=%d, error=%s", errno,
				strerror(errno));
		close(listening_socketfd_v4);
		return 0;
	}
	if (listen(listening_socketfd_v4, 10) == -1) {
		error(orig_sockfh, "listen() v4, errno=%d, error=%s", errno,
				strerror(errno));
		close(listening_socketfd_v4);
		return 0;
	}
	return listening_socketfd_v4;
}

// Setup a IPV6 network socket to listen on
int create_port_listen_v6(FILE *orig_sockfh, int portno) {
	struct sockaddr_in6 serv_addr_v6;
	int listening_socketfd_v6 = socket(AF_INET, SOCK_STREAM, 0);
	if (listening_socketfd_v6 == -1) {
		error(orig_sockfh, "opening socket() v6, errno=%d, error=%s", errno,
				strerror(errno));
		return 0;
	}
	bzero((char *) &serv_addr_v6, sizeof(serv_addr_v6));
	serv_addr_v6.sin6_family = AF_INET6;
	serv_addr_v6.sin6_addr = in6addr_loopback; // localhost
	serv_addr_v6.sin6_port = htons(portno);
	if (bind(listening_socketfd_v6, (struct sockaddr *) &serv_addr_v6,
			sizeof(serv_addr_v6)) == -1) {
		error(orig_sockfh, "bind() v6 errno=%d, error=%s", errno,
				strerror(errno));
		close(listening_socketfd_v6);
		return 0;
	}
	if (listen(listening_socketfd_v6, 10) == -1) {
		error(orig_sockfh, "listen() v6, errno=%d, error=%s", errno,
				strerror(errno));
		close(listening_socketfd_v6);
		return 0;
	}
	return listening_socketfd_v6;
}

/*
 *  Respond to commands requests and perform the commands:
 *  For a list of commands see command_process().
 *
 *  Will fork() for longer running operations such as fullcompare and quickcompare.
 *
 *  Will load all PPMs into RAM before listening for commands.
 *
 *  Args:
 *
 * orig_socketfh    : A file handle where text is sent, typically for any operational errors.
 * sql_info         : A string with SQL connection information.
 * portno           : Which network port to listen on.
 * compare_size     : The height (and width) of the PPMs.
 * maxerr           : For images to be considered similar the difference must be below this amount.
 *
 *  Return: non-zero on error.
 */
int server_loop(FILE *orig_sockfh, char *sql_info, int portno, int compare_size,
		unsigned int maxerr) {

	// Connect to SQL database
	PGconn *psql = ppm_sql_connect(orig_sockfh, sql_info);
	if (!psql) {
		error(orig_sockfh,
				"libpq error: PQconnectdb returned NULL.\nSQL details: %s",
				sql_info);
		return 1;
	}

	// All PPMs in RAM. Loaded from SQL.
	PicInfo *picinfo_list = NULL;

	// Load all PPMs from SQL into RAM BEFORE we start to listen on the socket.
	int rc = load(orig_sockfh, psql, &picinfo_list);
	if (rc) {
		error(orig_sockfh, "LOAD failed with code %d", rc);
		ppm_sql_disconnect(orig_sockfh, psql);
		return 1;
	}

	// Setup IPv4 and/or IPv6 ports to listen on.
	// Ports below first_real_client_index are for listening for new connections.
	int first_real_client_index = 0;
	int listening_socket = create_port_listen_v4(orig_sockfh, portno);
	if (listening_socket > 0) {
		global_client_detail[first_real_client_index].fd = listening_socket;
		first_real_client_index++;
	}

	// Setup a IPV6 network socket to listen on
	listening_socket = create_port_listen_v6(orig_sockfh, portno);
	if (listening_socket > 0) {
		global_client_detail[first_real_client_index].fd = listening_socket;
		first_real_client_index++;
	}
	if (first_real_client_index == 0) {
		if (picinfo_list) {
			unload(&picinfo_list);
		}
		ppm_sql_disconnect(orig_sockfh, psql);
		error(orig_sockfh, "No network found. Listening to radio instead.");
		return 2;
	}
	// Done setting up IPv4 and/or IPv6 listening ports.

	// Setup an array of incoming file descriptors.
	int index;
	for (index = first_real_client_index; index < CLIENT_MAX; index++) {
		global_client_detail[index].fd = CLIENT_SLOT_FREE;
		global_client_detail[index].command_buffer[0] = 0;
		global_client_detail[index].pid = 0;
		// TODO global_client_detail[index].connection_timeout = TODO ;
	}
	global_active_connection_count = first_real_client_index;

	// Listening Sockets have been created.
	int server_loop = 1;

	// Setup a timeout waiting for commands.
	// We time out so that we can reap children without needing to wait for commands.
	fd_set my_fd_set;
	struct timeval timeout;

	// listen for commands
	while (server_loop) {

		/* Initialize the file descriptor set we will block on. */
		FD_ZERO(&my_fd_set);

		// Setup FD set to block on. Also maximum FD.
		int fd_max = 0;
		for (index = 0; index < CLIENT_MAX; index++) {
			// Don't add closed FDs.
			if (global_client_detail[index].fd == CLIENT_SLOT_FREE)
				continue;
			// Stop listening for new connections if we have too many.
			if ((index < first_real_client_index)
					&& (global_active_connection_count >= CLIENT_MAX))
				continue;

			FD_SET(global_client_detail[index].fd, &my_fd_set);
			fd_max = max(fd_max, global_client_detail[index].fd);
		}

		/* Initialize the timeout data structure. */
		// We do timeout so we can do housekeeping without need to wait for client input to trigger the loop.
		timeout.tv_sec = COMMAND_LISTEN_TIMEOUT;
		timeout.tv_usec = 0;

		/* select() returns the count of FDs with waiting input, or -1 if error. */
		int fd_count_with_input = TEMP_FAILURE_RETRY(
				select(fd_max + 1, &my_fd_set, NULL, NULL, &timeout));
		if (fd_count_with_input < 0) {
			error(orig_sockfh, "select() failed. errno=%d, error=%s", errno,
					strerror(errno));
			continue;
		}

		// Housekeeping
		// Reaper: Clean up any child processes which have exited.
		pid_t late_pid;
		while ((late_pid = waitpid(-1, NULL, WNOHANG)) > 0) {
			global_child_process_count--;
			// Find the late client by pid then record it as dead.
			for (index = first_real_client_index; index < CLIENT_MAX; index++){
				if ((global_client_detail[index].fd != CLIENT_SLOT_FREE)
						 && (global_client_detail[index].pid == late_pid)) {
					global_client_detail[index].pid = 0;
					break;
				}
			}
		}

		// Housekeeping
		// TODO check for any stale client connections
		// TODO add a mechanism so that we expire connections (other than socked for new v4 and v6)
		// if they wait too long to send data.
		// Don't expire connections if the server is the one that hasn't responded.

		// There was a select() timeout, no FDs with input available.
		if (fd_count_with_input == 0)
			continue;

		// Read data from FDs.
		for (index = 0; index < CLIENT_MAX; index++) {
			// Check for valid FD.
			if (global_client_detail[index].fd == CLIENT_SLOT_FREE)
				continue;
			// Check for waiting data on FD.
			int fd = global_client_detail[index].fd;
			if (!FD_ISSET(fd, &my_fd_set))
				continue;
			// We have a new IPv4 or IPv6 connection.
			if (index < first_real_client_index) {
				int new_sockfd = accept(fd, NULL, NULL);
				if (new_sockfd < 0) {
					error(orig_sockfh, "accept()");
					continue;
				}
				int free_slot = first_real_client_index; // lower connections are for new IPv4 and IPv6 connections.
				for (; free_slot < CLIENT_MAX; free_slot++)
					if (global_client_detail[free_slot].fd == CLIENT_SLOT_FREE)
						break;
				// Are we are out of free slots?
				if (free_slot >= CLIENT_MAX) {
					error(orig_sockfh,
							"Internal Error: we somehow got more connections than we can handle.");
					char *mesg = "BUSY: Please come back later";
					int write_rc = write(new_sockfd, mesg, strlen(mesg));
					if (write_rc == -1) {
						error(orig_sockfh, "Failed to tell client to go away");
					}
					close(new_sockfd);
				}
				// Register new connection.
				else {
					global_client_detail[free_slot].fd = new_sockfd;
					global_client_detail[free_slot].pid = 0;
					bzero(global_client_detail[free_slot].command_buffer,
					BUFFER_SIZE);
					global_client_detail[free_slot].cmd_offset = 0;
					global_active_connection_count++;
					// TODO anything else?
					// TODO set timeout.
				}
			} else {
				// We have data on existing connection that needs to be read.
				int client_fd = global_client_detail[index].fd;
				char *cmd_buffer = global_client_detail[index].command_buffer;
				int cmd_offset = global_client_detail[index].cmd_offset;
				// Read data.
				int read_bytes = read(client_fd, &cmd_buffer[cmd_offset], BUFFER_SIZE - cmd_offset);
				if (read_bytes < 0) {
					// kill the connection as client has most likely gone away.
					error(orig_sockfh,
							"Failed to read from client, closing the FD.");
					close(client_fd);
					global_client_detail[index].fd = CLIENT_SLOT_FREE;
					continue;
				}
				global_client_detail[index].cmd_offset += read_bytes;
				// TODO update timeout.
				// Check if a command has been completed.
				int command_end = strcspn(cmd_buffer, "\r\n");
				if (command_end > 0) {
					cmd_buffer[command_end] = 0; // Strip trailing LF, CR, CRLF, LFCR, ...
					command_process(client_fd, cmd_buffer, &picinfo_list, psql,
							&server_loop, compare_size, maxerr);
					close(client_fd);
					global_client_detail[index].fd = CLIENT_SLOT_FREE;
					global_active_connection_count--;
				}
			}
		}
	}
	if (picinfo_list) {
		unload(&picinfo_list);
	}

	// close all sockets, including for new IPv4 and IPv6 connections.
	for (index = 0; index < CLIENT_MAX; index++)
		if (global_client_detail[index].fd != CLIENT_SLOT_FREE)
			close(global_client_detail[index].fd);

	// End of server loop
	ppm_sql_disconnect(orig_sockfh, psql);
	return 0;
}

/*
 *  Print command line usage information
 */

void usage() {
	fprintf(stderr, "\n");
	fprintf(stderr,
			"Usage: \"dbname = 'MyDatabase' user = 'MyUser' connect_timeout = '10'\" port\n");
	fprintf(stderr, "\n");
}

/*
 * Main
 *
 * Start the server_loop() to listen for DIDS commands.
 *
 * Arg 1:  SQL connection string  "dbname = 'my_database_name' user = 'my_sql_user' connect_timeout = '10'"
 * Arg 2:  Network port to listen on for commands.
 *
 */

int main(int argc, char *argv[]) {
	int compare_size = COMPARE_SIZE;
	unsigned int maxerr = COMPARE_THRESHOLD;
	if (argc < 3) {
		fprintf(stdout, "\nERROR: Not enough arguments\n");
		usage();
		exit(1);
	}
	char *sql_info = argv[1];
	int portno = atoi(argv[2]);
	if (!portno) {
		fprintf(stdout, "\nERROR: Invalid port\n");
		usage();
		exit(1);
	}

	// Work out how many CPUs we have. Default to 2
	global_cpu_count = get_cpu_count(stdout);
	if (global_cpu_count == 0) {
		global_cpu_count = 2;
	}

	MagickWandGenesis();

	server_loop(stdout, sql_info, portno, compare_size, maxerr);

	MagickWandTerminus();

	/* The final thing that main() should do */
	pthread_exit(NULL);

	exit(0);
}

