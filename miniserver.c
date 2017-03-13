/* Sambeau's webserver 
 * This is a mini webserver.
 * Created March 2017 by Sambeau.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <strings.h>
#include <sys/wait.h>

#include "whttpparse.h"
#include "event.h"

#ifdef USE_LOG
#include "log.h"
#else
#define LOG_DBG(x)
#define log_error(x)	perror(x)
#endif

#include "miniserver.h"

struct urlhandle_list urlhandlequeue;

void serverInit(){
	TAILQ_INIT(&urlhandlequeue);
}

void urlhandle_add(struct urlhandle* handle){
	TAILQ_INSERT_TAIL(&urlhandlequeue, handle, next);
}

void urlhandle_set(struct urlhandle* _urlhandler, char _url[], char _method[], pURLHANDLE _handle){
	// _urlhandler->url = _url;
	strcpy(_urlhandler->url, _url);
	// _urlhandler->method = _method;
	strcpy(_urlhandler->method, _method);
	_urlhandler->url_callback = _handle;
}

void urlhandle_del(struct urlhandle* handle){
	TAILQ_REMOVE(&urlhandlequeue, handle, next);
}

struct urlhandle* urlhandle_dispatch(char _url[], char _method[]){
	struct urlhandle* tmp;
	
	for (tmp = TAILQ_FIRST(&urlhandlequeue); tmp;
		     tmp = TAILQ_NEXT(tmp, next)) {
		     if (strcmp(tmp->url, _url) == 0)
			     return tmp;
		}
		
	return NULL;
}

void client_request(int fd, short event, void*arg){
	struct event *ev = arg;
	struct WRequest request;
	struct WResponse response;
	struct urlhandle *handle;
	
	accept_request(fd, &request);
	handle = urlhandle_dispatch(request.server_url, request.server_method);
	if(handle == NULL){
		// fprintf(stderr, "the url:%s; the path: %s never found the define callback\n", request.server_url, request.server_path);
		default_handle_request(fd, &request);
	}else{
		// fprintf(stderr, "found the define callback\n");
		handle->url_callback(&request, &response);
		handle_response(fd, &response);
	}
	
	close(fd);
}

void server_accept(int fd, short event, void *arg){
	struct event *ev = arg;
	int client_sock = -1;
	struct sockaddr_in client_name;
	int client_name_len = sizeof(client_name);
	struct event *evclient = malloc(sizeof(struct event));

	memset(&client_name, 0, sizeof(client_name));
	event_add(ev, NULL);

	client_sock = accept(fd, (struct sockaddr *)&client_name, &client_name_len);
	fprintf(stderr, "the server receive a request from client %d\n", client_sock);
	// if (client_sock == -1)
	// 	error_die("accept");
	// fprintf(stderr, "%d\n", ntohs(client_name.sin_port));
	// accept_request(client_sock);
	// close(client_sock);

	event_set(evclient, client_sock, EV_READ, client_request, &evclient);
	event_add(evclient, NULL);
	// fprintf(stderr, "%d\n", client_sock);
	// perror("check if add event");
}

void serverStartUp(unsigned short port){
	int server_sock = -1;
	// u_short port = 18080;
	struct event evfifo;
	
	if(port <= 0)
		port = 18080;
	server_sock = startup(&port);

	/* Initalize the event library */
	event_init();

	/* Initalize one event */
	event_set(&evfifo, server_sock, EV_READ | EV_WRITE, server_accept, &evfifo);

	/* Add it to the active events, without a timeout */
	event_add(&evfifo, NULL);
	
	event_dispatch();

	close(server_sock);
}
