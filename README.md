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

Run Server in Docker Container:

```
docker build -t redis . 
docker network create redis-net
docker run --rm -it \
--name redis-server \
--network redis-net \
-p 1234:1234 \
-v "$(pwd):/usr/src/redis" \
redis
```

Run Client Container:

```
docker run --rm -it \
--network redis-net \
-v "$(pwd):/usr/src/redis" \
redis
```
