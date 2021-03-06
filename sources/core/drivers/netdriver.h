#ifndef __NETDRIVER_H
#define __NETDRIVER_H

#ifdef WIN32
	#pragma comment( lib, "SDL2_net" )
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h> 
#include <string.h>
#include <SDL.h>

// net2
#include "../../net2/fastevents.h"
#include "../../net2/net2.h"
#include "../../net2/SDLUtils.h"

//#include <SDL_net.h>
    
#include "../dkernel.h"

typedef struct {
	IPaddress server_ip, *client_ip; 
	TCPsocket server, client;  
	Uint32 ipaddr; 
	Uint16 port;
} network_connection;


void network_init();
char * process_message(char *);
void network_close();

#ifdef __cplusplus
}
#endif

#endif
