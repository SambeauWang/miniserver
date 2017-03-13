### Introduction
I am a beginer in background development. After I take look into¡¶TCP/IP Illustrated Volume 1¡·, I decide to create a mini http server. This is version 0.1, so that There is only little server.In this project, I use some other library, such as libevent, tinyhttp.There project have two function. First, You can use API to register [url and callback] and the library get http message from socket and parse it. Then It return a REQUEST object(which include url, method and so on) to callback function. The callback just only return a RESPONSE(include raw character). Second, If the library never find url in list, It will search html file or cgi program.

### Build
```
cd miniserver
make
```
It will build a static library(libminiserver.a) and a miniserver.h file.

### API
+ serverInit: init the sever.
+ urlhandle_add: add the [url, callback].
+ urlhandle_del: delete the [url, callback].
+ urlhandle_set: set the parameter of [url, callback].
+ serverStartUp: start the server.

### TODO
+ add the JSON function
+ support more function of HTTP protocol

### Credits
- [libevent](http://libevent.org)
- [tinyhttp]