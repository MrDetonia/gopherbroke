# gopherbroke
A simple Internet Gopher Protocol server for Linux written in C.

---

## About
The Internet Gopher Protocol is a simple protocol designed for document retrieval via the internet.
It has a simple client/server model, and presents the client with an interface much like an ordinary filesystem.

More information can be found here: https://tools.ietf.org/html/rfc1436 and here: https://en.wikipedia.org/wiki/Gopher_%28protocol%29

gopherbroke is a simple gopher server for the Linux platform written in C. It is free and open source software, licensed under the BSD 3-clause license.
See LICENSE for more information.

## Building
### Requirements
- C compiler
- automake

### Build Process
- clone this repo using `git clone https://gitla.in/MrDetonia/gopherbroke.git`.
- Inspect Makefile and make any required adjustments.
- run `make` to compile.

## Running
Simply run the executable with root permissions, and the server will listen on port 70 by default.

Without root permissions, the server will be unable to chroot into the given directory.
By default, this directory is /var/gopher.
Be sure that the user running the server has read permissions for the given directory.

gopherbroke accepts the following arguments:
- `-d <dir>` which directory to use as the server root (default: /var/gopher)
- `-p <port>` what port to listen on (default: 70)
