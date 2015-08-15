#ifndef __IC_SOCKET_H__
#define __IC_SOCKET_H__

/*!
 *
 *  This is derived from code by David Shelley
 *  You can email me for the original source joel.larsen@gmail.com
 *
**/

//////////////////////////////////////////////////////////////////////////////
// SYSTEM INCLUDES
#ifdef WIN32
#include <winsock.h>
#include <windows.h>
#else
#include <sys/wait.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#endif

#include <stdio.h>

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Core/icGlobals.h"


/*! Provides an interface for sockets
**/
class icSocket
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icSocket(void);

    ICRESULT setHomeAddress(int srcPortNum,const char* scrIPAddress);
    ICRESULT setDestAddress(int srcPortNum,const char* scrIPAddress);

    ICRESULT Close(void);


    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT sendPacket(const char* sendBuf, int bufLen);
    ICRESULT read(char* recv_buf, int* bytes_read);

protected:
#ifdef WIN32
    WSADATA wsaData;    //!data memeber used to initialize the socket 
    SOCKET mySocket;        //! socket used to read write data
#else
    int mySocket;
#endif

    sockaddr_in scrAddrInfo; //Socket address information
    sockaddr_in clientAddrInfo; //Socket address information

    int sourcePortNum;
    int iResult;
    int clientPortNum;

    char sourceIPAddress[80];    
    char clientIPAddress[80];

    //This is a set that can be used to mark the file descriptors we want to check for I/O
    fd_set rset;  // required for select function

    struct timeval timeout;

    int bytes_read;
    int addr_len;
    int selectTest;
}; //class icSocket


#endif //__IC_SOCKET_H__
