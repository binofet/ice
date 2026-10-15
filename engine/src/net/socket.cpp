
#include "ice/net/socket.h"

#include <iostream>
namespace ice::net {

// CONSTRUCTOR

/*! default c'tor
 **/
socket::socket(void) {
#ifdef WIN32
	FD_ZERO(&rset_);

	bytes_read_ = 0;

	ZeroMemory(&source_ip_address_, sizeof(source_ip_address_));
	// Clear out the memory in the structure
	ZeroMemory(&client_addr_info_, sizeof(client_addr_info_));

	// STEP 1 //////////////////////////
	//  Initialize Winsock
	result_ = WSAStartup(MAKEWORD(2, 2), &wsa_data_);
	if (result_ != 0) {
		ice::warning("WSAStartup failed");
	} /////////////////////////

#endif
}

ice::result socket::set_home_address(int         src_port_num [[maybe_unused]],
                                     const char *scr_ip_address [[maybe_unused]]) {
#ifdef WIN32
	// STEP 2 ///////////////////////
	// Fill out the information needed to initialize a socket and clear out the memory in the
	// structure
	ZeroMemory(&scr_addr_info_, sizeof(scr_addr_info_));
	socket::scr_addr_info_.sin_family = AF_INET;                      // address family Internet
	socket::scr_addr_info_.sin_port   = htons((u_short)src_port_num); // Port to connect on
	// If no error occurs, the inet_addr function returns an unsigned long value containing a
	// suitable binary representation
	//  of the Internet address given.
	// If an application does not care what local address is assigned, specify the constant value
	// INADDR_ANY for an IPv4 local address
	//  or the constant value in6addr_any for an IPv6 local address in the sa_data member of the
	//  name parameter.
	scr_addr_info_.sin_addr.s_addr = inet_addr(scr_ip_address); // Target IP

	// Double check that the address was stored correctly
	if (scr_addr_info_.sin_addr.s_addr == INADDR_NONE) {
		ice::warning("An error occured converting the char pointer to IP address");
		return IC_FAIL_GEN;
	} ///////////////////////////////////////////////////////////////////

	// STEP 3 //////////////////
	// Create a generice UDP socket that has no information with it.
	my_socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (my_socket_ == INVALID_SOCKET) {
		ice::warning("Couldn't create generic socket");
		return IC_FAIL_GEN;
	} //////////////////////////////////////////////////////////////

	// Bind the target address to the socket we created. Do this using the bind function
	// The bind function associates a local address with a socket.
	if (bind(my_socket_, (struct sockaddr *)&scr_addr_info_, sizeof(struct sockaddr_in)) == -1) {
		ice::warningf("Could not bind to the socket. Source IP Address is: %s", scr_ip_address);
		result_ = closesocket(my_socket_);
		WSACleanup();
		return IC_FAIL_GEN;
	}

	return IC_OK;
#endif

	return IC_FAIL_GEN;
}

ice::result socket::set_dest_address(int         src_port_num [[maybe_unused]],
                                     const char *scr_ip_address [[maybe_unused]]) {
#ifdef WIN32
	client_port_num_ = src_port_num;
	for (int i = 0; i < 80; i++) {
		if ((client_ip_address_[i] = scr_ip_address[i]) == 0)
			break;
	}

	ZeroMemory(&client_addr_info_, sizeof(client_addr_info_));
	client_addr_info_.sin_family      = AF_INET;                      // address family Internet
	client_addr_info_.sin_port        = htons((u_short)src_port_num); // Port to connect on
	client_addr_info_.sin_addr.s_addr = inet_addr(scr_ip_address);    // Target IP

	return IC_OK;
#endif
	return IC_FAIL_GEN;
}

ice::result socket::send_packet(const char *send_buf [[maybe_unused]],
                                int         buf_len [[maybe_unused]]) {
#ifdef WIN32
	int server_length = sizeof(struct sockaddr_in);

	if (sendto(my_socket_, send_buf, (int)buf_len, 0, (struct sockaddr *)&client_addr_info_,
	           server_length) == -1) {
		ice::warning("Error transmitting data.");
		// TODO, this should try reconnect maybe?
		// result_ = closesocket(my_socket_);
		// WSACleanup();
		return IC_FAIL_GEN;
	}

	return IC_OK;
#endif
	return IC_FAIL_GEN;
} // End sendPacket

ice::result socket::read(char *recv_buf [[maybe_unused]], int *bytes_read [[maybe_unused]]) {
#ifdef WIN32
	*bytes_read = 0;

	addr_len_ = sizeof(struct sockaddr);

	// This has to be set every time since a timeout_ will cause it to reset
	FD_ZERO(&rset_);
	FD_SET(my_socket_, &rset_);

	// If the timeval is {0,0} then select will return immediately; this is used to "poll" the state
	// of the selected sockets.
	timeout_.tv_sec  = 0;
	timeout_.tv_usec = 0;

	select_test_ = select(0, &rset_, NULL, NULL, &timeout_);

	if (select_test_ == SOCKET_ERROR) {
		ice::warningf("There was an error with the socket: %i", WSAGetLastError());
	}
	if (select_test_ > 0) {
		if (FD_ISSET(my_socket_, &rset_)) {

			*bytes_read = recvfrom(my_socket_, recv_buf, 1023, 0,
			                       (struct sockaddr *)&client_addr_info_, &addr_len_);
			if (*bytes_read < 0)
				return IC_FAIL_GEN;
			recv_buf[*bytes_read] = '\0';
		}
	} else {
	}

	FD_ZERO(&rset_);
	FD_SET(my_socket_, &rset_);

	return IC_OK;
#endif
	return IC_FAIL_GEN;
}

ice::result socket::close(void) {
#ifdef WIN32
	result_ = closesocket(my_socket_);
	WSACleanup();

	return IC_OK;
#endif
	return IC_FAIL_GEN;
}

} // namespace ice::net
