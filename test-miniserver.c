#include "miniserver.h"
#include "stdlib.h"
#include "stdio.h"
#include <string.h>

void first_fun(struct WRequest* a, struct WResponse* b){
	char temp[] = "<HTML><TITLE>Test</TITLE><BODY><P>This is a test.</BODY></HTML>";
	strcpy(b->res, temp);
}

void second_fun(struct WRequest* a, struct WResponse* b){
	printf("work\n");
}

int main(int agrc, char *agrv[]){
	struct urlhandle first;
	struct urlhandle second;
	struct urlhandle *temp;
	struct WRequest a;
	struct WResponse b;
	
	serverInit();
	urlhandle_add(&first);
	urlhandle_add(&second);
	urlhandle_set(&first, "/test", "GET", first_fun);
	urlhandle_set(&second, "/work", "POST", second_fun);	
	
	serverStartUp(18080);
	printf("the end.\n");
	return 0;
}
