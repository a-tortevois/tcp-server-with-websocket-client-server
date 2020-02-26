# tcp-server-with-websocket-client-server
A simple TCP socket server in C with a Websocket Client/Server in Node.JS and a final client in HTML/Javascript

# Getting Started

Compile the nonblocking server
```
gcc nonblocking_server.c -o server -lpthread
```

Launch the server 

```
./server &
```

Launch the Node.JS TCP / Websocket bridge (wss.js)

```
node wss.js &
```

Open the ws_client.html on your computer, then connect to the server IP with port 8888

Send a message, it should respond with the current time (depends on your configuration)

# Example

```
root@raspberrypi:~# gcc nonblocking_server.c -o server -lpthread
root@raspberrypi:~# ./server &
[1] 16624
root@raspberrypi:~# Server thread is running on port 1998
Waiting for a new client

root@raspberrypi:~# node wss.js
[2] 16706
root@raspberrypi:~# New client incoming connection: 4
2020-2-24 13:48:39: TCP Socket: connection request
2020-2-24 13:48:39: TCP Socket: connected
2020-2-24 13:48:48: WebSocket.Server: New connexion from ::ffff:192.168.1.64:60407
2020-2-24 13:48:52: WebSocket message: test
Message received from the client: test
Message send to the client: {"payload":"13:48:52"}
2020-2-24 13:48:52: TCP Socket: data received from the server: {"payload":"13:48:52"}
```

![Alt text](./example.png?raw=true "Client screen capture example")

