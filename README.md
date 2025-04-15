# redis

Network Message Transfer Protocol: 4 byte includes the size of the msg

Compile Server:
```
g++ -Wall -Wextra -O2 -g server.cpp helper.cpp -o executables/server
```

Compile Client:
```
g++ -Wall -Wextra -O2 -g client.cpp helper.cpp -o executables/client
```