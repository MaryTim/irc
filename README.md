*This project has been created as part of the 42 curriculum by mbudkevi, itykhono.*

# ft_irc

## Description

ft_irc is a simplified implementation of an Internet Relay Chat (IRC) server, written in C++ and compliant with the C++98 standard.

The goal of this project is to understand and implement a real-time, text-based communication server using low-level networking primitives.
The server allows multiple clients to connect simultaneously, authenticate, join channels, exchange private and public messages, and manage channels
using operator commands.

This project is based on RFC 1459 and focuses on client-to-server communication only. Server-to-server communication is intentionally not implemented.

## Features

- TCP/IP server (IPv4) using non-blocking sockets
- Single `poll()` loop handling all I/O operations
- Multiple simultaneous clients without forking
- User registration using PASS / NICK / USER
- Channel management:
  - JOIN
  - Operators and regular users
  - Invite-only channels
  - Channel keys (passwords)
  - Channel topics
  - User limits
- Operator commands:
  - KICK
  - INVITE
  - TOPIC
  - MODE (`i`, `t`, `k`, `o`, `l`)
- Private messages and channel messages
- Proper cleanup on client disconnect:
  - removal from channel members
  - operators
  - invited lists
- Graceful handling of partial reads and fragmented commands

## Supported IRC Commands

### Connection maintenance
- PING / PONG

### Connection and registration
- PASS
- NICK
- USER
- QUIT

### Channels
- JOIN
- MODE
- TOPIC
- INVITE
- KICK

### Messaging
- PRIVMSG

## Requirements Compliance

- Written in **C++98**
- Compiled with:
  - `-Wall -Wextra -Werror`
  - compatible with `-std=c++98`
- All sockets are non-blocking
- A single `poll()` call is used to handle:
  - accepting connections
  - reading
  - writing
- Communication over TCP/IP
- Tested with a real IRC client (Halloy)

## Build Instructions

Compile the project using:

make

Run the server with:

./ircserv <port> <password>

Example:

./ircserv 6667 pass

## Resources

- RFC 1459 — Internet Relay Chat Protocol

AI tools were used as a supportive learning aid during the project. They were mainly used for:
- clarifying protocol behavior and RFC terminology
- reviewing edge cases and error handling scenarios
- improving documentation and explanations

All code was written, reviewed, and fully understood by the author. Any AI-generated suggestions were manually verified, tested, and adapted to fit the project constraints.
