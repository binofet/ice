#pragma once
namespace ice::net {

/*!
 *
 *  This is derived from code by David Shelley
 *  You can email me for the original source joel.larsen@gmail.com
 *
 **/

#ifdef WIN32
} // namespace ice::net
#include <windows.h>
#include <winsock.h>
namespace ice::net {
#else
} // namespace ice::net
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netinet/in.h> /* INET constants and stuff */
#include <sys/wait.h>   /* socket specific definitions */
namespace ice::net {
#endif
} // namespace ice::net

#include <stdio.h>
namespace ice::net {} // namespace ice::net
#include "ice/globals.h"
namespace ice::net {

/*! Provides an interface for sockets
 **/
class socket {
public:
	socket(void);

	/// Configure the local IPv4 endpoint before opening the legacy UDP socket.
	ice::result set_home_address(int src_port_num, const char *scr_ip_address);
	/// Configure the remote IPv4 endpoint used by subsequent legacy UDP writes.
	ice::result set_dest_address(int src_port_num, const char *scr_ip_address);

	/// Close the native socket; further I/O requires initialization again.
	ice::result close(void);

	/// Send caller-owned bytes through the initialized legacy socket; inspect status for transport
	/// failure.
	ice::result send_packet(const char *send_buf, int buf_len);
	/// Read into caller-owned output storage; inspect the returned count/status and diagnostics
	/// before consuming data.
	ice::result read(char *recv_buf, int *bytes_read);

protected:
#ifdef WIN32
	WSADATA wsa_data_;  //! data memeber used to initialize the socket
	SOCKET  my_socket_; //! socket used to read write data
#else
	int my_socket_;
#endif

	sockaddr_in scr_addr_info_;    // Socket address information
	sockaddr_in client_addr_info_; // Socket address information

	int source_port_num_;
	int result_;
	int client_port_num_;

	char source_ip_address_[80];
	char client_ip_address_[80];

	/// This is a set that can be used to mark the file descriptors we want to check for I/O
	fd_set rset_; // required for select function

	struct timeval timeout_;

	int bytes_read_;
	int addr_len_;
	int select_test_;
}; // class icSocket

} // namespace ice::net
