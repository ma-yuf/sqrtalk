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
## API
### response
#### success
```json
{
  "type":"e.g. login"
  "status":"success",
  "message":"e.g. Login successful.",
}
```
#### error
```json
{
  "type":"e.g. command"
  "status":"error",
  "message":"e.g. User not found.",
}
```
### login
```json
{
  "type":"login",
  "username":"your_username",
  "password":"your_password"
}
```
### message
```json
{
  "type":"message",
  "message":"hello"
}
```
### receive message
```json
{
  "type":"message",
  "username":"sender_username"
  "content":"hello",
}
```
### private_message
```json
{
  "type":"private_message",
  "to":"someone",
  "content":"hello"
}
```
### receive private_message
```json
{
  "type":"private_message",
  "from":"from_user",
  "to":"to_user",
  "content":"hello"
}
```
### command
#### list
```json
{
  "type":"command",
  "command":"/list"
}
```
#### receive list command
```json
{
  "type":"user_list",
  "users":["user1","user2"]
}
```
### admin command
#### kick
```json
{
  "type":"command",
  "command":"/kick asd",
  "password":"your_password"
}
```
#### ban
```json
{
  "type":"command",
  "command":"/ban asd",
  "password":"your_password"
}
```
