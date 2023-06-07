# Mini SSH

## Build

```
mkdir build
cd build
cmake ..
make
```

## Components

Contains two programs:
- Client
- Server

Currently, only spawn command is supported

### Server

To start:
```
./server <port>
```

Special behavior:
- Send SIGTERM to server to terminate it

### Client

To launch a command:
```
./client <server address (ip:port)> spawn <command> <arg1> <arg2> ... <argn>
```
This will switch into interactive mode that allowes you to send input to the launched command and returns whatever is printed by it to the client.

Special behaviour
- CTRL+D will send EOF to the command
- CTRL+C will capture the SIGINT and tell server to send the SIGINT to the running command

