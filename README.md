# **Toralizer**

A **LD_PRELOAD-based** solution to transparently route application traffic through the [Tor](https://www.torproject.org/) network using **SOCKS5**.

**Table of Contents**
1. [Introduction](#introduction)
2. [Features](#features)
3. [Architecture & How It Works](#architecture--how-it-works)
4. [What Is SOCKS5?](#what-is-socks5)
5. [Installation](#installation)
   - [Local Installation (No Global System Changes)](#local-installation-no-global-system-changes)
   - [Global Installation (System-Wide)](#global-installation-system-wide)
   - [Uninstallation](#uninstallation)
6. [Usage](#usage)
   - [Basic Usage](#basic-usage)
   - [Examples](#examples)
   - [Flags and Options](#flags-and-options)
7. [Security & Privacy Considerations](#security--privacy-considerations)
8. [Troubleshooting](#troubleshooting)
9. [Development & Building from Source](#development--building-from-source)
   - [Dependencies](#dependencies)
   - [Compilation](#compilation)
10. [Advanced Topics](#advanced-topics)
   - [DNS Leakage](#dns-leakage)
   - [IPv6 Details](#ipv6-details)
   - [Intercepting Domain Names (SOCKS5 ATYP=3)](#intercepting-domain-names-socks5-atyp3)
   - [Timeouts and Partial Reads/Writes](#timeouts-and-partial-readswrites)
11. [License](#license)
12. [Disclaimer](#disclaimer)
13. [References & Further Reading](#references--further-reading)

---

## Introduction

**Toralizer** aims to **“torify”** your networked applications by intercepting system calls (`connect()`) at runtime and funneling them through **Tor** via a **SOCKS5 proxy** (by default, `127.0.0.1:9050`).

**Why use Toralizer?**

- You don’t have to modify the source code of the application you want to tunnel through Tor.
- You can **transparently** wrap programs such as `curl`, `wget`, or any other dynamically-linked binaries, forcing them to route TCP connections via the Tor network.
- It’s an easy-to-use **script + `.so` library** approach with minimal overhead.

---

## Features

- **LD_PRELOAD Hooking**: Leverages the dynamic linker’s `LD_PRELOAD` mechanism to intercept the `connect()` system call.
- **SOCKS5** Implementation: Communicates with Tor’s SOCKS5 interface, enabling secure and anonymous outbound connections.
- **IPv4 & IPv6** Support: The hooking library checks the `sa_family` and handles both `AF_INET` (IPv4) and `AF_INET6` (IPv6).
- **Flexible Installation**:
  - **Local** usage (no system-wide changes).
  - **Global** installation (system-wide availability of `toralize` command).
- **Optional** command-line flags: `--help`, `--version`, `--install`, `--uninstall`.

---

## Architecture & How It Works

1. **LD_PRELOAD**:  
   When you set `LD_PRELOAD=/path/to/toralizer.so`, the dynamic loader injects our library **before** the standard C library. Our implementation of `connect()` overrides the system’s default connect function.

2. **Intercepting `connect()`**:  
   - Whenever an application attempts to establish a TCP connection, it calls `connect()`.
   - Our custom `connect()` function detects the target address and, instead, **opens a new socket** to your local Tor SOCKS5 proxy (`127.0.0.1:9050` by default).
   - We then perform the **SOCKS5 handshake** with Tor, instructing it to connect to the original target IP and port.
   - If successful, Tor returns a success code, and we `dup2()` the proxy socket onto the original file descriptor, so the application is none the wiser—it thinks it’s directly connected to the remote server.

3. **Result**:  
   All TCP traffic from that application flows **through Tor**. You get the anonymity benefits of Tor (though note [DNS Leakage](#dns-leakage) below).

---

## What Is SOCKS5?

**SOCKS5** is a network proxy protocol widely supported by Tor. Key features:
- **Client–Server** handshake:
  1. **Greeting**: The client advertises supported authentication methods (e.g., “no auth”).
  2. **Server Response**: The server picks an auth method.
  3. **Client Connect Request**: The client requests a connection to a specific hostname/IP + port.
  4. **Server Reply**: Indicates success or an error code.

- **DNS via Proxy**: SOCKS5 can handle domain names (ATYP=0x03).  
  However, if your application calls `getaddrinfo()` first (resolving the domain to an IP locally), that IP is used in the handshake. This can cause [DNS leakage](#dns-leakage).

- **No Additional Encryption**: SOCKS5 itself doesn’t encrypt data (beyond the TCP connection). The anonymity comes from the **Tor network**.

---

## Installation

Toralizer can be used **locally** (no special privileges, no system changes) or **globally** (requires `root` or `sudo`).

### Local Installation (No Global System Changes)

1. **Build** (if not pre-built). See [Development & Building from Source](#development--building-from-source).
2. **Keep** `toralize.sh` and `toralizer.so` in one folder.
3. **Run** `toralize.sh your_command [args...]` to torify your desired program.

Nothing is installed to system directories. Everything remains in that local folder.

### Global Installation (System-Wide)

1. **Build** or obtain the `.so` library and the `toralize.sh` script.
2. **Run**:
   ```bash
   sudo ./toralize.sh --install
   ```
3. **Follow the prompt**:
   - Press `G` to install globally:
     - The script copies `toralizer.so` into `/usr/local/share/toralizer/`.
     - Installs a new script in `/usr/local/bin/toralize` that sets `LD_PRELOAD` automatically.
   - Press `L` to do nothing (keep local).

After a successful global install, you can:
```bash
toralize curl http://check.torproject.org
```
No need to reference `toralize.sh` or set `LD_PRELOAD` manually anymore.

### Uninstallation

Run:
```bash
sudo ./toralize.sh --uninstall
```
This removes:
- `/usr/local/bin/toralize`
- `/usr/local/share/toralizer/`

---

## Usage

### Basic Usage

In **local mode**:
```bash
# Use the script in the same directory as toralizer.so
./toralize.sh curl http://check.torproject.org
```
Behind the scenes:
- `LD_PRELOAD` is set to `toralizer.so`.
- `curl`’s `connect()` calls are intercepted and routed through Tor.

### Examples

- **Check Tor**:
  ```bash
  ./toralize.sh curl -s https://check.torproject.org
  ```
  If the HTML states “Congratulations, this browser is configured to use Tor,” it worked.

- **Download a file**:
  ```bash
  ./toralize.sh wget https://example.com/somefile.zip
  ```

- **SSH over Tor**: Potentially:
  ```bash
  ./toralize.sh ssh user@host.example
  ```
  (Note: This may have other implications, such as DNS leakage if `host.example` is resolved locally.)

### Flags and Options

**`--help`**  
Displays a help message describing usage, flags, and examples.

**`--version`**  
Prints the current version (e.g., `1.0.0`).

**`--install`**  
Runs an **interactive install** process:
- Prompts whether to install **globally** or keep **local**.
- If globally installed, requires `sudo` or root, copies `.so` to `/usr/local/share/toralizer/` and a script to `/usr/local/bin/toralize`.

**`--uninstall`**  
Removes any previously installed global files.

**(No flags)**  
Any arguments after the script are treated as the **command to run** with `LD_PRELOAD` set.

---

## Security & Privacy Considerations

1. **DNS Leakage**  
   If the application resolves domain names **before** calling `connect()`, your system’s DNS resolver is used. This means DNS requests go **outside** Tor, potentially revealing your real IP or location (unless your system’s DNS also goes through Tor or is otherwise anonymized). See [DNS Leakage](#dns-leakage).

2. **Infinite Recursion**  
   The hooking library must detect when it’s connecting to the Tor SOCKS proxy itself (`127.0.0.1:9050` or `[::1]:9050`) to avoid recursively hooking that connection. Our code includes a check to skip hooking in that situation.

3. **Partial Coverage**  
   - Only **dynamically-linked** applications that use `connect()` from glibc (or similar) will be intercepted.  
   - **Statically-linked** binaries bypass `LD_PRELOAD`.  
   - Some applications may bypass standard library calls or have their own networking stack, in which case this hooking won’t work.

4. **System Configuration**  
   - Ensure Tor is running locally on `127.0.0.1:9050`.  
   - Alternatively, update `TOR_PROXY_IP` and `TOR_PROXY_PORT` in the code if your Tor is bound elsewhere.

---

## Troubleshooting

- **Tor not running**  
  If you see errors like `connect(tor-proxy): Connection refused`, ensure Tor is running and listening on `127.0.0.1:9050`.
  
- **SOCKS5 error codes** (`SOCKS5 CONNECT failed, REP=0xXX`)  
  Common issues:
  - Tor’s `SocksPolicy` might be rejecting direct IP connections or a particular address range.
  - The remote is unreachable or blocked by Tor exit policies.

- **Permission denied**  
  Installing globally requires root/sudo. If not, you’ll see permission errors.

- **Not connecting**  
  Try a verbose approach:
  ```bash
  TORALIZER_DEBUG=1 ./toralize.sh curl -v http://check.torproject.org
  ```
  (If you add debug prints to the C code, you can conditionally show them with an env variable.)

---

## Development & Building from Source

### Dependencies

- **gcc** or another C compiler
- **make**
- **libc6-dev** (standard libraries/headers)
- **Tor** (to actually route traffic via SOCKS5)

### Compilation

A minimal `Makefile` might look like this:

```make
CC = gcc
CFLAGS = -Wall -Wextra -fPIC -shared -ldl -D_GNU_SOURCE
SRCS = toralize.c
TARGET = toralizer.so

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
```

1. **Clone or copy** the `toralize.c` and `Makefile`.
2. **Run** `make`.
3. You should end up with `toralizer.so`.

Then you can test locally:
```bash
export LD_PRELOAD=/path/to/toralizer.so
curl http://check.torproject.org
unset LD_PRELOAD
```

For easier usage, use the provided **`toralize.sh`** script.

---

## Advanced Topics

### DNS Leakage

**DNS leakage** occurs when an application resolves a hostname **outside** Tor, typically by calling `getaddrinfo()`. By the time Toralizer intercepts `connect()`, the OS has already performed DNS resolution.  
- **Solution**: Use SOCKS5 domain-based resolution (ATYP=0x03) or intercept `getaddrinfo()` as well. This is more complex but ensures DNS is done via Tor.

### IPv6 Details

Toralizer checks `sa_family`:
- If `AF_INET6`, it uses **ATYP=0x04** in the SOCKS5 handshake.
- Tor must allow and handle IPv6 exits. Some Tor exit nodes don’t support IPv6, so your connection might fail if the remote site is IPv6-only and your chosen Tor exit doesn’t route IPv6.

### Intercepting Domain Names (SOCKS5 ATYP=3)

To pass **raw domain names** to Tor (so Tor does the DNS resolution):
- You’d need to catch the **domain** before the system resolver. Typically, that means hooking `getaddrinfo()` or rewriting library calls.  
- Then send a SOCKS5 handshake with `ATYP=0x03`, length of domain, etc.  
- This is **beyond** the minimal example we provide, but crucial for full anonymity.

### Timeouts and Partial Reads/Writes

The code in `toralize.c` uses simplistic `recv()` and `send()` loops. In production, you may want to:
- Implement **timeouts** to handle unresponsive or slow SOCKS5 servers.
- Handle partial reads/writes more robustly.

---

## License

This project is typically distributed under an open-source license (e.g., MIT, GPL, etc.).  
Replace this section with your actual license text. For example, the **MIT License**:

```
MIT License

Copyright (c) 2025 ...

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files...
```

---

## Disclaimer

1. **Legal Compliance**: The Tor network and usage of anonymizing software may be regulated or restricted in certain jurisdictions. Ensure you comply with local laws and regulations.
2. **Not Bulletproof**: Simply routing traffic through Tor does **not** guarantee perfect anonymity. Browser fingerprinting, DNS leaks, misconfigurations, or application-level data (e.g., cookies) can still deanonymize you.
3. **Use at Your Own Risk**: I am not liable for any misuse or damages arising from its usage.

---

## References & Further Reading

- [Tor Project – Official Documentation](https://support.torproject.org)
- [SOCKS5 RFC 1928](https://datatracker.ietf.org/doc/html/rfc1928)
- [LD_PRELOAD Tricks](https://blog.man7.org/linux/man-pages/man7/ld.so.7.html)
- [Preventing DNS Leaks with Tor](https://2019.www.torproject.org/docs/faq.html.en#DoesTorProtectIPTunneling)

**Happy Torifying!**  

