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

#ifndef _MINISERVER_H_
#define _MINISERVER_H_

#include "./missing/queue/sys/queue.h"
#include "miniconfig.h"

struct WQUERYSTRING{
	char key[QUERYSTRING_KEY];
	char value[QUERYSTRING_VALUE];
};

struct WRequest{
	char server_name[REQUEST_PARAM_LENGTH];
	char server_port[REQUEST_PARAM_LENGTH];
	char server_method[REQUEST_PARAM_LENGTH];
	char server_url[REQUEST_PARAM_LENGTH];
	char server_path[REQUEST_PARAM_LENGTH];
	char http_accept[REQUEST_PARAM_LENGTH];
	char http_response[REQUEST_PARAM_LENGTH];
	struct WQUERYSTRING query_string[QUERYSTRING_LENGTH];
	char query_string_raw[QUERYSTRING_RAW];
};

struct WResponse{
	char res[RESPONSE_LENGTH];
};

typedef void (*pURLHANDLE)(struct WRequest*, struct WResponse*);

struct urlhandle{
	TAILQ_ENTRY (urlhandle) next;

	char url[100];
	char method[20];
	pURLHANDLE url_callback;
};

TAILQ_HEAD (urlhandle_list, urlhandle);

void serverStartUp(unsigned short port);
void serverInit();

void urlhandle_set(struct urlhandle*, char _url[], char _method[], pURLHANDLE);
void urlhandle_add(struct urlhandle*);
void urlhandle_del(struct urlhandle*);
struct urlhandle* urlhandle_dispatch(char _url[], char _method[]);

#endif