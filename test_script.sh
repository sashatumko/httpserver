#!/bin/bash

TESTS_PASSED=0
TESTS_RAN=0

GREEN='\033[0;32m'
RED='\033[0;31m'
MAGENTA='\033[0;35m'
NC='\033[0m'

trap ctrl_c INT  
ctrl_c() {
  kill_server
}

start_server() {
  ./httpserver 8080 &
  sleep 0.5
}

kill_server() {
  pkill httpserver
}

check_test() {
  RET=$?
  if [[ $RET -eq 0 ]]; then
    echo -e "${GREEN}PASS${NC}"
    TESTS_PASSED=$((TESTS_PASSED+1))
  else
    echo -e "${RED}FAIL${NC}"
  fi
  TESTS_RAN=$((TESTS_RAN+1))
}

print_banner() {
  CYAN='\033[0;36m'
  echo -e "${CYAN}$1${NC}"
}

# =========== start of test script functions ==================

# sends 8 huge binary files (abt 400 MB each)
fat_test() {
  print_banner "FAT TEST: ${test_str[0]}"
  
  head -c 419430400 /dev/urandom > FATFILE1
  head -c 419430400 /dev/urandom > FATFILE2
  head -c 419430400 /dev/urandom > FATFILE3
  head -c 419430400 /dev/urandom > FATFILE4
  head -c 419430400 /dev/urandom > FATFILE5
  head -c 419430400 /dev/urandom > FATFILE6
  head -c 419430400 /dev/urandom > FATFILE7
  head -c 419430400 /dev/urandom > FATFILE8

  start_server
  start_time=`date +%s`

  (curl -s http://0:8080/FATFILE1 -o one.out & \
  curl -s http://0:8080/FATFILE2 -o two.out & \
  curl -s http://0:8080/FATFILE3 -o three.out & \
  curl -s http://0:8080/FATFILE4 -o four.out & \
  curl -s http://0:8080/FATFILE5 -o five.out & \
  curl -s http://0:8080/FATFILE6 -o six.out & \
  curl -s http://0:8080/FATFILE7 -o seven.out & \
  curl -s http://0:8080/FATFILE8 -o eight.out & \
  wait)
  
  echo run time is $(expr `date +%s` - $start_time) s

  kill_server

  diff FATFILE1 one.out && \
  diff FATFILE2 two.out && \
  diff FATFILE3 three.out && \
  diff FATFILE4 four.out && \
  diff FATFILE5 five.out && \
  diff FATFILE6 six.out && \
  diff FATFILE7 seven.out && \
  diff FATFILE8 eight.out

  check_test

  rm -f *.out
}

# send 5 small GET requests
test_one() {
  print_banner "Test 1: ${test_str[1]}"
  start_server
  head -c 100 /dev/urandom > file1
  echo 'hsofnosdjnofds' > file2
  echo 'dosdoiojweoijjiojweiorwoei' > file3
  echo 'sadasfghsofnosdjnofdfffs' > file4
  echo 'dofdfseeeesdoiojweoijjiojweiorwoei' > file5
  (curl -s http://0:8080/file1 -o file1.out & \
  curl -s http://0:8080/file2 -o file2.out & \
  curl -s http://0:8080/file3 -o file3.out & \
  curl -s http://0:8080/file4 -o file4.out & \
  curl -s http://0:8080/file5 -o file5.out & \
  wait)
  kill_server
  diff file1 file1.out && \
  diff file2 file2.out && \
  diff file3 file3.out && \
  diff file4 file4.out && \
  diff file5 file5.out
  check_test
}

# Send a 63 KB PUT request
test_two() {
  print_banner "Test 2: ${test_str[2]}"
  start_server
  dd if=/dev/urandom of=myfile bs=1024 count=63 status=none # 63 KiB file
  curl -s http://localhost:8080/e901kollkad -T myfile
  kill_server
  diff myfile e901kollkad
  check_test
}

# Send multiple small GET requests
test_three() {
  print_banner "Test 3: ${test_str[3]}"
  start_server
  head -c 2 /dev/urandom > 12dsaDSAEFV
  head -c 20 /dev/urandom > WQD_-12
  head -c 200 /dev/urandom > a
  ( curl -s http://localhost:8080/12dsaDSAEFV -o test3-cl1.out & \
  curl -s http://localhost:8080/WQD_-12 -o test3-cl2.out & \
  curl -s http://localhost:8080/a -o test3-cl3.out & \
  wait)
  kill_server
  diff 12dsaDSAEFV test3-cl1.out && \
  diff WQD_-12 test3-cl2.out && \
  diff a test3-cl3.out
  check_test
}

# send a binary 16 KB PUT request (diced into multiple messages)
test_four() {
  print_banner "Test 4: ${test_str[4]}"
  start_server
  dd if=/dev/urandom of=d10 count=16 bs=1024 status=none
  printf """PUT /testFile10 HTTP/1.1\r\nContent-Length: 16384\r\n\r\n""" | cat - d10 > nc-d10
  ncat -d 1 localhost 8080 < nc-d10
  pkill -P $$
  kill_server
  diff d10 testFile10
  check_test
}

# GET request on a file without read permissions
test_five() {
  print_banner "Test 5: ${test_str[5]}"
  start_server
  echo 'blah blah' > ass
  chmod -r ass
  curl -s http://localhost:8080/ass --write-out "%{http_code}" > test5-server.out
  kill_server
  grep -q "403" test5-server.out
  check_test
}

# GET a large 400MB binary file
test_six() {
  print_banner "Test 6: ${test_str[6]}"
  start_server
  head -c 419430400 /dev/urandom > 400MiB_source
  curl -s http://localhost:8080/400MiB_source -o 400MiB_1.out
  kill_server
  diff 400MiB_source 400MiB_1.out
  check_test
}

# GET, HEAD, PUT an empty file
test_seven() {
  print_banner "Test 7: ${test_str[7]}"
  start_server
  touch t12
  touch t13
  touch t14
  ( curl -s http://localhost:8080/t12 -o t12.out & \
  curl -s -T t13 http://localhost:8080/t133 & \
  curl -s -I http://localhost:8080/t14 -o t14.out & \
  wait)
  kill_server
  printf """HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n""" > t144
  diff t12 t12.out && \
  diff t13 t133 && \
  diff t14.out t144
  check_test
}

# GET, HEAD, PUT a file with name that is not one of the valid 27 characters (bad request)
test_eight() {
  print_banner "Test 8: ${test_str[8]}"
  start_server
  touch aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa t82.mp3 t83.txt
  echo 'hello' > t84
  ( curl -s http://localhost:8080/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa --write-out "%{http_code}" > test81-server.out & \
  curl -s -I http://localhost:8080/t82.mp3 --write-out "%{http_code}" > test82-server.out & \
  curl -s -T t84 http://localhost:8080/t83.txt --write-out "%{http_code}" > test83-server.out & \
  wait)
  kill_server
  grep -q "400" test81-server.out && \
  grep -q "400" test82-server.out && \
  grep -q "400" test83-server.out
  check_test
}

# GET request, small file, with netcat
test_nine() {
  print_banner "Test 9: ${test_str[9]}"
  start_server
  echo 'hello' > t99
  printf """GET /t99 HTTP/1.1\r\n\r\n""" > nc-d11
  printf """GET /t99 HTTP/1.1\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\nhello\n""" > t9.out
  ncat -d 1 localhost 8080 < nc-d11 -o temp
  pkill -P $$
  kill_server
  diff temp t9.out
  check_test
}

# GET request with typo in the header
test_ten() {
  print_banner "Test 10: ${test_str[10]}"
  start_server
  echo 'hello' > t101
  printf """GGET /t101 HTTP/1.1\r\n\r\n""" > nc-d12
  printf """GGET /t101 HTTP/1.1\r\n\r\nHTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n""" > t101.out
  ncat -d 1 localhost 8080 < nc-d12 -o temp1
  pkill -P $$
  kill_server
  diff temp1 t101.out
  check_test
}

# PUT with content length X and actual body is X+20 bytes (check if server will read OVER the amount of bytes needed)
test_eleven() {
  print_banner "Test 11: ${test_str[11]}"
  start_server

  echo 'hellohello' > t11s #31 bytes
  printf """PUT /t11out HTTP/1.1\r\nContent-Length: 11\r\n\r\nhellohello\nhellohellohellohell\n""" > nc-d11s
  ncat -d 1 localhost 8080 < nc-d11s
  pkill -P $$
  kill_server
  diff t11out t11s 
  check_test
}

# GET a file with HTTP version wrong HTTP/0.9
# if the client sends anything besides HTTP/1.1 (like HTTP/0.9) respond with a bad request. 
# Don't try to match your HTTP/X.Y string in your response, just send:
# “HTTP/1.1 400 Bad Request\r\n\r\n”
test_twelve() {
  print_banner "Test 12: ${test_str[12]}"
  start_server
  printf """GET /t12 HTTP/0.9\r\n\r\nHTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n""" > t12expected
  printf """GET /t12 HTTP/0.9\r\n\r\n""" > nc-d12s
  ncat -d 1 localhost 8080 < nc-d12s -o temp120
  pkill -P $$

  kill_server
  diff t12expected temp120
  check_test
}

# GET, HEAD "/".
# example: “GET / HTTP/1.1\r\n\r\n” (BAD REQUEST)
test_thirteen() {
  print_banner "Test 13: ${test_str[13]}"
  start_server
  printf """HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n""" > t13expected
  # STILL NEED GET
  curl -s -I http://0:8080/ -o t13o2
  kill_server
  diff t13expected t13o2
  check_test
}

# Speed test, 8 GET requests of varying sizes
test_fourteen() {
  print_banner "Test 14: ${test_str[14]}"

  head -c 419430400 /dev/urandom > t14s1_source
  head -c 20971520 /dev/urandom > t14s2_source
  head -c 10485760 /dev/urandom > t14s3_source
  head -c 5242880 /dev/urandom > t14s4_source
  head -c 419430400 /dev/urandom > t14s5_source
  head -c 20971520 /dev/urandom > t14s6_source
  head -c 10485760 /dev/urandom > t14s7_source
  head -c 5242880 /dev/urandom > t14s8_source

  start_server
  start_time=`date +%s`

  (curl -s http://0:8080/t14s1_source -o t14s1.out & \
  curl -s http://0:8080/t14s2_source -o t14s2.out & \
  curl -s http://0:8080/t14s3_source -o t14s3.out & \
  curl -s http://0:8080/t14s4_source -o t14s4.out & \
  curl -s http://0:8080/t14s5_source -o t14s5.out & \
  curl -s http://0:8080/t14s6_source -o t14s6.out & \
  curl -s http://0:8080/t14s7_source -o t14s7.out & \
  curl -s http://0:8080/t14s8_source -o t14s8.out & \
  wait)

  echo run time is $(expr `date +%s` - $start_time) s

  kill_server
  diff t14s1_source t14s1.out && \
  diff t14s2_source t14s2.out && \
  diff t14s3_source t14s3.out && \
  diff t14s4_source t14s4.out && \
  diff t14s5_source t14s5.out && \
  diff t14s6_source t14s6.out && \
  diff t14s7_source t14s7.out && \
  diff t14s8_source t14s8.out
  check_test

}

# GET request on "/" filename (bad) and netcat diced request
test_fifteen() {
  print_banner "Test 15: ${test_str[15]}"
  start_server

  printf """HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n""" > expected10
  {
  printf """PUT / HTTP/1.1\r\n"""; 
  sleep 0.1; 
  printf """Content-Length: 6\r\n\r\n"""; 
  sleep 0.1; printf """hello\n"""; 
  } | nc localhost 8080 > FILE10
  pkill -P $$
  kill_server

  start_server
  sleep 1.5
  echo 'hello' > expected11
  {
  printf """PUT /d1 HTTP/1.1\r\n"""; 
  sleep 0.1; 
  printf """Content-Length: 6\r\n\r\n"""; 
  sleep 0.1; printf """hello\n"""; 
  } | nc localhost 8080
  
  pkill -P $$
  kill server
  diff expected10 FILE10 && \
  diff expected11 d1
  check_test
}

# PUT REQUEST - WHOLE REQUEST IN ONE MESSAGE
# REQUEST WHERE THE FIRST "\r\n\r\n" IS NOT IN THE FIRST 4096 BYTES (BAD REQUEST)
# Send a get request such that:
    #msg1: "GET /file6666 " 
    #msg2: "HTTP/1.1\r\n\r\n"
test_sixteen() {
  print_banner "Test 16: ${test_str[16]}"
  start_server

  echo 'hellohello' > expected12
  {
  printf """PUT /FILE12 HTTP/1.1\r\nContent-Length: 11\r\n\r\nhellohello\n"""; 
  } | nc localhost 8080

  printf """HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n""" > expected14
  dd if=/dev/urandom of=d66 count=4 bs=1024 status=none
  printf """PUT /testFile66 HTTP/1.1\r\nContent-Length: 16384\r\n""" | cat - d66 > 66_source
  (ncat -d 1 localhost 8080 < 66_source) > crlf.out

  echo 'helloworldhellobuuuhhh' > file6666
  printf """HTTP/1.1 200 OK\r\nContent-Length: 23\r\n\r\nhelloworldhellobuuuhhh\n""" > expected6666
  {
  printf """GET /file6666 """;
  sleep 0.1;
  printf """HTTP/1.1\r\n\r\n""";
  } | nc localhost 8080 > FILE6666.out

  pkill -P $$
  kill_server
  diff expected12 FILE12 && \
  diff crlf.out expected14 && \
  diff expected6666 FILE6666.out
  check_test
}

# GET request with one of the irrevelent headers messed up (400)
#failing
test_seventeen() {
  print_banner "Test 17: ${test_str[17]}"
  start_server

  printf """HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n""" > expected13
  {
  printf """GET /FILE666 HTTP/1.1\r\nHost: localhost:8080\r\nthisisbad\r\n\r\n""";
  } | nc localhost 8080 > FILE667

  sleep 1.5
  pkill -P $$
  kill_server
  diff expected13 FILE667
  check_test
}

# CLARKS TEST
test_eighteen() {
  print_banner "Test 18: ${test_str[18]}"
  start_server

  # Create the source files:
  head -c 1000 < /dev/urandom > source_small
  head -c 10000 < /dev/urandom > source_medium
  head -c 100000 < /dev/urandom > source_large
  head -c 2222 < /dev/urandom > source_4
  head -c 22222 < /dev/urandom > source_5
  head -c 222222 < /dev/urandom > source_6
  head -c 150000 < /dev/urandom > client_large.bin

  (curl -sI http://localhost:8080/source_large > out_h0 & \
  curl -s http://localhost:8080/TO_PUT -T client_large.bin > /dev/null & \
  curl -s http://localhost:8080/source_small --output out1 & \
  curl -s http://localhost:8080/source_medium --output out2 & \
  curl -s http://localhost:8080/source_large --output out3 & \
  curl -s http://localhost:8080/source_4 --output out4 & \
  curl -s http://localhost:8080/source_5 --output out5 & \
  curl -s http://localhost:8080/source_6 --output out6 & \
  wait)

  kill_server

  # Diff the files, if there's any printout before "test done!!!", then a test "failed"
  diff out1 source_small && \
  diff out2 source_medium && \
  diff out3 source_large && \
  diff out4 source_4 && \
  diff out5 source_5 && \
  diff out6 source_6 && \
  diff client_large.bin TO_PUT && \

  check_test
  rm -f client_large.bin
}

# PUT a file named "X" such that "X" already exists (and permissions allow)
test_nineteen() {
  print_banner "Test 19: ${test_str[19]}"
  
  echo "this file exists and has perms" > existsperms_source
  echo "overwritten to this" > t19_source
  
  start_server
  curl -s -T t19_source http://0:8080/existsperms_source
  kill_server

  diff t19_source existsperms_source
  check_test
}

# PUT a file named "X" such that "X" already exists (and permissions DENIED) (forbidden 403)
test_twenty() {
  print_banner "Test 20: ${test_str[20]}"
  start_server

  echo "this file exists and does not have perms" > existsnoperms_source
  echo "this file exists and does not have perms" > t20_expected
  chmod -w existsnoperms_source
  echo "overwritten to this" > t20_source
  
  start_server
  curl -s -T t20_source http://0:8080/existsnoperms_source
  kill_server

  diff t20_expected existsnoperms_source
  check_test
}

# =========== main to select test ==================
test_str=("GET 8 huge binary files" \
    "GET 5 small files" \
	  "PUT a large binary file" \
	  "GET multiple small binary files" \
	  "PUT request 16 KB (diced into multiple messages)" \
	  "GET request on a file without read permissions" \
    "GET a large 400MB binary file" \
    "GET, HEAD, PUT an empty file" \
    "GET, HEAD, PUT a file with invalid name" \
    "GET request, small file, with netcat" \
    "GET request, typo in header" \
    "PUT with content length X and actual body is X+20 bytes" \
    "GET request, http version 0.9" \
    "GET, HEAD '/'" \
    "Speed test, 8 GET requests of varying sizes" \
    "GET request on '/' filename (bad) and netcat diced request" \
    "Three netcat tests, diced, missing CRLF" \
    "GET request with irrelevant headers in wrong format (expected 400)" \
    "CLARKS TEST" \
    "PUT a file onto server that already exists (perms allow)" \
    "PUT a file onto server that already exists (perms denied)")
    
len_test_str=${#test_str[@]}
echo "Select test to run:"
printf "\t$((0))) All tests\n"
printf "\t$((1))) Fat test\n"
printf "\t$((2))) Chunky test\n"
printf "\t$((3))) Speedup test (use 4 threads)\n"
printf "\t$((4))) last test\n"
printf "Enter number: "

read TEST

echo -e "${MAGENTA}\n===== Build httpserver =====\n${NC}"
make clean
make

echo -e "${MAGENTA}\n====== START of TESTS ======\n${NC}"
case $TEST in
  0)
    test_one
    test_two
    test_three
    test_four
    test_five
    test_six
    test_seven
    test_eight
    test_nine
    test_ten
    test_eleven
    test_twelve
    test_thirteen
    test_fourteen
    test_fifteen
    test_sixteen
    test_seventeen
    test_eighteen
    test_nineteen
    test_twenty
    ;;

  1)
    fat_test
    ;;
  2)
    test_four
    ;;
  3)
    test_fourteen
    ;;
  4)
    test_nineteen
    test_twenty
    ;;

  *)
    echo -n "unknown"
    ;;
esac
echo -e "${MAGENTA}======= END of TESTS ======="
echo -e "SCORE: ${TESTS_PASSED}/${TESTS_RAN}"
echo -e "============================${NC}"
echo -e "${RED}Delete test files?${NC}"
echo -ne "${RED}y/n?${NC} "
read CLEAN
if [[ $CLEAN == 'y' ]]; then
  rm *.out
  rm -f t81.s t83.txt t82.mp3
  ls | grep -P '^(?!Makefile)(?!httpserver)([a-zA-Z0-9_-]+){1,27}$' | xargs rm &> /dev/null
  make clean
fi