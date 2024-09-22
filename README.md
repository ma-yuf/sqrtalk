# sqrtalk
A chatroom based on c++ and websocket
## server
### platform
windows only
### required
websocketpp
nlohmann/json
### compile command
```
g++ -Ilibs/websocketpp -Ilibs/json -o server.exe main.cpp server.cpp -lboost_system-mgw14-mt-x64-1_86 -lboost_thread-mgw14-mt-x64-1_86 -lboost_chrono-mgw14-mt-x64-1_86 -lws2_32 -lmswsock
```
