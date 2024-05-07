# Distributed Client-Server File Management System

## Overview

This project implements a distributed client-server file management system, facilitating seamless file retrieval and management across multiple clients and servers. The system enables clients to request files based on various criteria such as file name, size, creation date, and extensions. The server processes handle concurrent client requests through forking child processes, ensuring efficient handling of requests and optimal load distribution.

## Features

- **Concurrent Processing**: Enhanced server processes allow for concurrent handling of client requests through forking child processes.
- **Advanced File Retrieval**: Clients can request files based on criteria such as file name, size, creation date, and extensions.
- **File Unzipping**: The system supports file unzipping functionality, enabling clients to retrieve compressed files.
- **Robust Filtering Options**: Clients can apply complex filtering choices to refine file retrieval based on specific criteria.
- **Optimized Load Distribution**: Alternating client connections between primary and mirror servers ensure optimized load distribution for efficient system performance.

## Usage

### Server Processes

- Start the main server process `serverw24`, along with mirror servers `mirror1` and `mirror2`.
- The server processes will wait for client connections and handle requests concurrently.

### Client Process

- Run the client process `clientw24` on a separate machine or terminal.
- Use the provided client commands to request files from the server based on specific criteria.
- Files retrieved from the server will be stored in a designated folder (`w24project`) on the client-side.

## Installation

1. Clone this repository

2. Navigate to the project directory:
    => cd distributed-file-management-system

3. Compile and run the server processes and client process as per the provided instructions.

## Contributors

- [Anand Vadukul](https://github.com/anandvadukul28)

## License

This project is licensed under the [MIT License](LICENSE).

