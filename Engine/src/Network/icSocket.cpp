
#include "Network/icSocket.h"

#include <iostream>

//CONSTRUCTOR


/*! default c'tor
**/
icSocket::icSocket(void)
{
#ifdef WIN32
    FD_ZERO(&rset);

    bytes_read = 0;

    ZeroMemory( &sourceIPAddress, sizeof(sourceIPAddress) );
    //Clear out the memory in the structure
    ZeroMemory( &clientAddrInfo, sizeof(clientAddrInfo) );

    //STEP 1 //////////////////////////
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0)
    {
        icWarning("WSAStartup failed");
    } /////////////////////////
    
#endif
    
}// END FUNCTION icSocket(void)


ICRESULT icSocket::setHomeAddress(int srcPortNum,const char* scrIPAddress)
{
#ifdef WIN32
    //STEP 2 ///////////////////////
    //Fill out the information needed to initialize a socket and clear out the memory in the structure
    ZeroMemory(&scrAddrInfo, sizeof(scrAddrInfo) );
    icSocket::scrAddrInfo.sin_family = AF_INET; // address family Internet
    icSocket::scrAddrInfo.sin_port = htons ((u_short)srcPortNum); //Port to connect on
    //If no error occurs, the inet_addr function returns an unsigned long value containing a suitable binary representation
    // of the Internet address given.
    //If an application does not care what local address is assigned, specify the constant value INADDR_ANY for an IPv4 local address
    // or the constant value in6addr_any for an IPv6 local address in the sa_data member of the name parameter.
    scrAddrInfo.sin_addr.s_addr = inet_addr (scrIPAddress); //Target IP

    //Double check that the address was stored correctly
    if (scrAddrInfo.sin_addr.s_addr == INADDR_NONE)
    {
        icWarning("An error occured converting the char pointer to IP address");
        return IC_FAIL_GEN;
    } ///////////////////////////////////////////////////////////////////


    //STEP 3 //////////////////
    //Create a generice UDP socket that has no information with it.
    mySocket = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (mySocket == INVALID_SOCKET)
    {
        icWarning("Couldn't create generic socket");
        return IC_FAIL_GEN;
    } //////////////////////////////////////////////////////////////

    //Bind the target address to the socket we created. Do this using the bind function
    //The bind function associates a local address with a socket.
    if (bind(mySocket, (struct sockaddr *)&scrAddrInfo, sizeof(struct sockaddr_in)) == -1)
    {
        icWarningf("Could not bind to the socket. Source IP Address is: %s",scrIPAddress);
        iResult = closesocket(mySocket);
        WSACleanup();
        return IC_FAIL_GEN;
    }

    return IC_OK;
#endif
    
    return IC_FAIL_GEN;
} //END FUNCTION setHomeAddress(int srcPortNum,const char* scrIPAddress)


ICRESULT icSocket::setDestAddress(int destPortNum,const char* destIPAddress)
{
#ifdef WIN32
    clientPortNum = destPortNum;
    for (int i = 0; i < 80; i ++)
    {
        clientIPAddress[i] = destIPAddress[i];
    }


    ZeroMemory( &clientAddrInfo, sizeof(clientAddrInfo) );     
    clientAddrInfo.sin_family = AF_INET; // address family Internet
    clientAddrInfo.sin_port = htons ((u_short)destPortNum); //Port to connect on
    clientAddrInfo.sin_addr.s_addr = inet_addr (destIPAddress); //Target IP

    return IC_OK;
#endif
    return IC_FAIL_GEN;
}


ICRESULT icSocket::sendPacket(const char* sendBuf, int bufLength)
{
#ifdef WIN32
    int server_length = sizeof(struct sockaddr_in);

    if (sendto(mySocket, sendBuf, (int)bufLength, 0, (struct sockaddr *)&clientAddrInfo, server_length) == -1)
    {
        icWarning("Error transmitting data.");
        // TODO, this should try reconnect maybe?
        //iResult = closesocket(mySocket);
        //WSACleanup();
        return IC_FAIL_GEN;
    }

    return IC_OK;
#endif
    return IC_FAIL_GEN;
} //End sendPacket


ICRESULT icSocket::read(char* recv_data, int* bytes_read)
{
#ifdef WIN32
    *bytes_read = 0;

    addr_len = sizeof(struct sockaddr);

    //This has to be set every time since a timeout will cause it to reset
    FD_ZERO(&rset);
    FD_SET(mySocket,&rset);

    //If the timeval is {0,0} then select will return immediately; this is used to "poll" the state of the selected sockets.
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    selectTest = select(0, &rset, NULL, NULL, &timeout);

    if (selectTest == SOCKET_ERROR)
    {
        icWarningf("There was an error with the socket: %i",WSAGetLastError());
    }
    if(selectTest >0)
    {
        if (FD_ISSET(mySocket,&rset) )
        {

            *bytes_read = recvfrom(mySocket,recv_data,1024,0,(struct sockaddr *)&clientAddrInfo, &addr_len);
            recv_data[*bytes_read] = '\0';
        }
    }
    else
    {
    }

    FD_ZERO(&rset);
    FD_SET(mySocket,&rset);

    return IC_OK;
#endif
    return IC_FAIL_GEN;
}

ICRESULT icSocket::Close(void)
{
#ifdef WIN32
    iResult = closesocket(mySocket);
    WSACleanup();

    return IC_OK;
#endif
    return IC_FAIL_GEN;
}// END FUNCTION Close(void)
