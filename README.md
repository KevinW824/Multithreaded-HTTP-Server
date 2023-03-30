# Multithreaded HTTP Server

This is a simple yet powerful multithreaded HTTP server capable of handling GET and PUT requests. The server is designed to work efficiently and thread-safe by utilizing multiple threads, which allows it to handle multiple requests simultaneously.

## Features

- Multithreaded for efficient and concurrent request handling
- Supports GET and PUT HTTP methods
- Error handling for unsupported HTTP methods and invalid URIs
- Customizable server settings (e.g. port number)
- Logging capabilities for tracking request and response data

## Getting Started

### Prerequisites

To run the server, you will need:

- `pthread.h` for multithreading functionality
- `TOML` for running test scripts

The server is intended to run under Linux environment (Ubuntu 22.04), running under other environments may cause unexpected errors.

### Installation

1. Clone the repository
2. Change into the project directory
3. Run `make` to compile the c file
4. Start the server using the following command:
`./httpserver <port> -t <number of threads>`
- Port number is required
- An optional argument -t to special the number of threads to use, default: 4

## Usage

Once the server is running, you can use any HTTP client (e.g., web browser, `curl`, `wget`, `netcat`) to send GET or PUT requests.

## Testing

In order to display the multithreaded functionality and prove that the server is thread-safe, a set of test scripts is proved. The test directory will contain a few examples of using them:
- `olivertwist` takes a sequence of request commands from a TOML file, passes these requests to a server, records the order in which it sent requests, and records the output of the requests.
The TOML file allows you to specify very specific (and interesting) interleavings of requests from clients.
- `sherlock` takes an audit log and request order from oliver and determines if the audit log is possible w.r.t the request order that was sent (in particular, it determines if the audit log is a total ordering of what was sent).
- `watson` identifies if the audit log and responses are consistent with each other.

To run the test, you will use the following command:
`make`
`./test_repo.sh`

Server should produce responses that are coherent and atomic with respect to the ordering specified in your audit log. That is, if an entry for a request R1, is earlier than an entry for a request, R2, in the log, then the server's response to R2 must be as though the processing for R occurred in its entirety before any of the processing for R2. We call this ordering a linearization because it creates a single linear ordering of all client requests. We say that this ordering is a total order because it provides an ordering for all elements (i.e., for any two unique requests, R1 or R2, your audit log will identify that either R1 happens-before R2 or that Rz happens-before R1).
Your server's linearization must conform to the order of requests that it received in the following way: if two requests, R1 and R2, arrive such that the start time of R2 is after the end time of R1, then your audit log should indicate the line for R2 after the log line for R1. However, if R1 and R2 overlap (i.e., the start time of R1 is before the start time of R2 and the start time of R2 is before the end time of R1), then your server's audit log could produce audit log entries in either order (i.e., R1 could be before R2 or R2 could be before R1).