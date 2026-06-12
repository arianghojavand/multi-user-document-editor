# Multi-User Document Editor

A concurrent client-server Markdown document editor written in **C** using **POSIX signals**, **named pipes**, **pthreads**, and a custom in-memory Markdown document model.

The project allows multiple clients to connect to a central server, submit document editing commands, receive versioned update broadcasts, and interact with the shared Markdown document according to role-based permissions.

## Overview

This project implements a simplified collaborative Markdown editing system. A server process owns the shared document and accepts connections from multiple client processes. Clients connect through a signal-based handshake, communicate with the server through named FIFOs, and send commands such as inserting text, deleting text, applying Markdown formatting, viewing permissions, and disconnecting.

The server manages:

- Client authentication through `roles.txt`
- One thread per connected client
- A shared Markdown document protected by mutexes
- Periodic document version updates
- Edit logging and broadcast messages
- Graceful shutdown once all clients disconnect

The client manages:

- Connecting to the server using the server PID
- Sending the username for permission checking
- Reading the initial document state
- Sending editing commands through standard input
- Receiving server acknowledgements and version broadcasts
- Local commands for viewing the document, permissions, and logs

## Tech Stack

| Technology | Purpose |
|---|---|
| C | Main implementation language |
| GCC | Compilation |
| POSIX signals | Client-server handshake using `SIGRTMIN` and `SIGRTMIN + 1` |
| Named pipes / FIFOs | Bidirectional client-server communication |
| pthreads | Concurrent client handling and background server tasks |
| Mutexes | Shared document, logs, and client stream synchronisation |
| AddressSanitizer | Runtime memory debugging during development |
| Makefile | Build automation |

## Features

### Client-Server Architecture

The server starts first and prints its process ID. Clients then connect by sending `SIGRTMIN` to the server. The server creates two named pipes for each client:

- `FIFO_C2S_<pid>` for client-to-server commands
- `FIFO_S2C_<pid>` for server-to-client responses

After creating the FIFOs, the server sends `SIGRTMIN + 1` back to the client so the client knows the connection is ready.

### Role-Based Permissions

User permissions are stored in `roles.txt`.

Example:

```text
ryan read
yao read
daniel write
```

Users with `write` permission can edit the document. Users with `read` permission can connect and view information, but edit commands are rejected as unauthorised.

### Multi-Client Concurrency

Each connected client is handled by a separate pthread on the server. The server uses mutexes to protect shared state, including the Markdown document, client output streams, per-client writes, log storage, and shutdown state.

### Versioned Updates

The server receives a time interval as a command-line argument. After edits are submitted, a background thread periodically increments the document version, applies queued Markdown commands, writes log entries, and broadcasts updates to connected clients.

Broadcasts follow this structure:

```text
VERSION <version_number>
EDIT <username> <command> <status>
END
```

### Markdown Editing Support

The editor supports:

- Insert text
- Delete text
- Newline
- Headings
- Bold
- Italic
- Blockquotes
- Ordered lists
- Unordered lists
- Inline code
- Horizontal rules
- Links

### Custom Document Model

The document is represented as a doubly linked list of character chunks. Editing commands are queued first, then applied during version increments. The implementation also includes logic for sorting commands by timestamp and adjusting command positions around deleted ranges.

## Project Structure

```text
multi-user-document-editor-master/
├── libs/
│   ├── document.c
│   ├── document.h
│   └── markdown.h
├── source/
│   ├── client.c
│   ├── markdown.c
│   └── server.c
├── Makefile
├── roles.txt
└── .gitignore
```

## Getting Started

### Prerequisites

This project is designed for a Unix-like environment such as Linux or WSL.

You need:

- GCC
- make
- POSIX-compatible system support
- pthread support

On Ubuntu or WSL, install the required tools with:

```bash
sudo apt update
sudo apt install build-essential make gcc
```

### Build

From the project root directory, run:

```bash
make
```

This builds two executables:

```text
server
client
```

To remove generated files, run:

```bash
make clean
```

## Running the Project

### 1. Start the Server

Run the server with a version update interval in seconds:

```bash
./server 2
```

The server will print its PID:

```text
Server PID: <pid>
```

Keep this terminal open.

### 2. Start a Client

In a second terminal, run:

```bash
./client <server_pid> <username>
```

Example:

```bash
./client 12345 daniel
```

The username must exist in `roles.txt`.

### 3. Use Client Commands

Once connected, the client can send commands through the terminal.

Local client commands:

| Command | Description |
|---|---|
| `DOC?` | Print the client’s current cached document |
| `PERM?` | Show the client’s permission level |
| `LOG?` | Print the client’s received version/edit log |
| `DISCONNECT` | Disconnect from the server |

Editing commands sent to the server:

| Command | Description |
|---|---|
| `INSERT <pos> <content>` | Insert text at a position |
| `DEL <pos> <len>` | Delete a range of text |
| `NEWLINE <pos>` | Insert a newline |
| `HEADING <level> <pos>` | Insert a Markdown heading marker |
| `BOLD <start> <end>` | Apply bold formatting |
| `ITALIC <start> <end>` | Apply italic formatting |
| `BLOCKQUOTE <pos>` | Insert a blockquote marker |
| `ORDERED_LIST <pos>` | Insert an ordered list marker |
| `UNORDERED_LIST <pos>` | Insert an unordered list marker |
| `CODE <start> <end>` | Apply inline code formatting |
| `HORIZONTAL_RULE <pos>` | Insert a horizontal rule |
| `LINK <start> <end> <url>` | Convert text into a Markdown link |

Example commands:

```text
INSERT 0 Hello world
BOLD 0 5
NEWLINE 11
INSERT 12 This is collaborative Markdown.
LINK 12 16 https://example.com
DISCONNECT
```

## Server Commands

While the server is running, the server terminal supports:

| Command | Description |
|---|---|
| `DOC?` | Print the current server document |
| `LOG?` | Print `log.txt` |
| `QUIT` | Shut down the server if no clients are connected |

The server rejects `QUIT` while clients are still connected.

## Output Files

During execution, the project may generate:

| File | Description |
|---|---|
| `log.txt` | Server-side edit and version log |
| `client_log_<username>_<id>.txt` | Per-client log of received broadcasts |
| `doc.md` | Final Markdown document saved when the server exits |
| `FIFO_C2S_<pid>` | Temporary client-to-server FIFO |
| `FIFO_S2C_<pid>` | Temporary server-to-client FIFO |

## Notes

- This project assumes a Linux or WSL environment.
- The Makefile uses `-fsanitize=address`, which is useful for debugging memory errors but can be removed for a cleaner release build.
- Client permissions are based on `roles.txt`.
- The server must be started before any clients.
- The server only shuts down once all clients have disconnected.
