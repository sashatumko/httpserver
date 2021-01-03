# Multi-threaded httpserver

## Building
To build `httpserver` just run `make` in the root of the directory.

## Running
To run `httpserver` enter `./httpserver <hostname:port>` into the command line.<br />
Optional flags are:<br />
`[-N num_threads]` specifies the number of threads to run <br />
`[-v verbose]` server will print extra info about threads<br />

### Supported request methods
`GET`, `HEAD`, and `PUT`. Examples:<br />
curl -s http://localhost:8080/filename<br />
curl -s -I http://localhost:8080/filename<br />
curl -s -T filename1 http://localhost:8080/filename2<br />