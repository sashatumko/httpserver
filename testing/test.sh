#!/bin/bash

printf """PUT /t99 HTTP/1.1\r\nContent-Length: 6\r\n\r\nhellooo\n""" > nc-d11
ncat localhost 8080 < nc-d11
pkill -P $$

echo 'hello' > temp
diff temp t99 && echo "PASSED"
rm -f t99 temp nc-d11