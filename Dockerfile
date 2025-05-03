# Use a base image with GCC (includes g++) pre-installed
#FROM ubuntu:latest then RUN apt-get update && apt-get install -y build-essential
FROM gcc:latest 

# Set the working directory inside the container
WORKDIR /usr/src/redis

RUN apt-get update && apt-get install -y \
    strace  \
    && rm -rf /var/lib/apt/lists/* # Clean up

# Default command (useful for interactive sessions)
CMD ["/bin/bash"]
