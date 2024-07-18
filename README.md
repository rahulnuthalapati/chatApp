### Text Chat Application

This is a C/C++ based text chat application built from scratch with networking concepts that facilitates communication between multiple clients through a central server. It leverages TCP sockets for reliable data transmission and the `select()` system call for concurrent handling of multiple connections.

### Features

- **Client-Server Model:** Employs a classic client-server architecture where clients connect to a central server for message exchange.
- **Shell-like Interface:** Provides a user-friendly command-line interface for interacting with the chat application.
- **Commands:** Supports commands like `LOGIN`, `LIST`, `SEND`, `BROADCAST`, `BLOCK`, `UNBLOCK`, `LOGOUT`, and `EXIT`.
- **Message Relaying:** The server acts as an intermediary, relaying messages between clients.
- **Message Buffering:** Stores messages for offline clients to be delivered upon their login.
- **Blocking:** Allows clients to block messages from specific users.

### How to Run

1.  **Compilation:** Compile the code using a C/C++ compiler like G++:
    ```bash
    g++ -o chatApp chatApp.c
    ```

2.  **Server:** Start the server on a specified port (e.g., 4322):
    ```bash
    ./chatApp s 4322
    ```

3.  **Clients:** Launch multiple client instances, each connecting to the server's IP address and port:
    ```bash
    ./chatApp c <server-ip> 4322
    ```

### Working Process

1.  **Client Login:** Clients initiate a TCP connection with the server using the `LOGIN` command, providing the server's IP and port.
2.  **Client List:** Upon successful login, the server sends the client a list of all currently logged-in clients.
3.  **Message Exchange:** Clients can send messages to other clients using the `SEND` (unicast) or `BROADCAST` (multicast) commands.
4.  **Server Relay:** The server receives messages from clients and forwards them to the intended recipients.
5.  **Message Buffering:** If a recipient is offline, the server stores the message in a buffer.
6.  **Blocking:** Clients can block messages from specific users using the `BLOCK` command.
7.  **Logout/Exit:** Clients can temporarily disconnect using `LOGOUT` or permanently leave using `EXIT`.

### Additional Notes

-   The application adheres to a specific output format for commands and events, as detailed in the original assignment instructions.
-   Error handling is implemented for invalid commands, IP addresses, and port numbers.
-   The server maintains statistics on messages sent and received by each client.

Please note that this project was developed as part of a university course and is intended for educational purposes.
