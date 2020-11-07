#!/bin/bash

echo "hello" > logtestfile1
echo "world" > logtestfile2
echo "meech" > logtestfile3
echo "hello" > aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
echo "hello" > a.out
echo "hello" > noperms
chmod -r noperms
chmod -w noperms

./httpserver 8080 -l log_file_mine &
sleep 0.5

# good requests
(curl -s http://0:8080/logtestfile1 & \
curl -s http://0:8080/logtestfile2 & \
curl -s http://0:8080/logtestfile3 & \
curl -s -I http://0:8080/logtestfile1 & \
curl -s -I http://0:8080/logtestfile2 & \
curl -s -I http://0:8080/logtestfile3 & \
curl -s -T logtestfile1 http://0:8080/logtestfile4 & \
curl -s -T logtestfile2 http://0:8080/logtestfile5 & \
curl -s -T logtestfile3 http://0:8080/logtestfile6 & \
wait)

# invalid filename bad requests
curl -s http://0:8080/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
curl -s -I http://0:8080/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
curl -s -T logtestfile1 http://0:8080/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

# invalid filename bad requests
curl -s http://0:8080/a.out
curl -s -I http://0:8080/a.out
curl -s -T logtestfile1 http://0:8080/a.out

# 404 bad requests
curl -s http://0:8080/dontexist
curl -s -I http://0:8080/notfound

# 403 bad requests
curl -s http://0:8080/noperms
curl -s -I http://0:8080/noperms


pkill httpserver

rm -f logtestfile1 logtestfile2 logtestfile3 logtestfile4 logtestfile5 logtestfile6 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa a.out noperms