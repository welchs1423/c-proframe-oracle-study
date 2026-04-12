# C-Stock ATM

![C](https://img.shields.io/badge/Language-C-blue?logo=c)
![Oracle](https://img.shields.io/badge/Database-Oracle%2021c%20XE-red?logo=oracle)
![ProC](https://img.shields.io/badge/Embedded%20SQL-Pro%2AC-orange)
![TCP/IP](https://img.shields.io/badge/Network-TCP%2FIP-green)
![Docker](https://img.shields.io/badge/Container-Docker-blue?logo=docker)
![Platform](https://img.shields.io/badge/Platform-WSL2%20%7C%20Ubuntu-blueviolet?logo=linux)
![License](https://img.shields.io/badge/License-MIT-lightgrey)

A full-featured, console-based ATM system built in **C with Oracle Pro\*C (Embedded SQL)**. The project evolved from a single-process monolith into a production-style **TCP/IP client-server architecture** with multi-process concurrency, XOR packet encryption, background daemon operation, and graceful shutdown — all backed by **Oracle Database 21c XE**.

---

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Key Features](#key-features)
4. [Project Structure](#project-structure)
5. [Database Schema](#database-schema)
6. [Protocol Design](#protocol-design)
7. [Getting Started](#getting-started)
8. [Build Reference](#build-reference)
9. [Development History](#development-history)

---

## Overview

C-Stock ATM was built as a systems-programming portfolio project that deliberately mirrors the architecture of a real banking backend:

| Concern | Approach |
|---|---|
| Persistence | Oracle 21c XE via Pro\*C Embedded SQL |
| Concurrency | `fork()`-per-connection multi-process model |
| Network | Custom binary protocol over TCP/IP sockets |
| Security | XOR packet encryption, account lock policy |
| Reliability | Graceful shutdown, zombie-process reaping, `COMMIT/ROLLBACK` atomicity |
| Observability | File-based audit logging (`logs/system.log`) |

The application supports two independent run modes:

- **Monolithic ATM** (`cstock_atm`) — single-process, directly embeds Oracle calls
- **TCP/IP Client-Server** (`cstock_server` + `cstock_client`) — network-separated architecture; the client requires zero Oracle dependencies

---

## System Architecture

```
┌──────────────────────────────────────────────────────────┐
│                   CLIENT (cstock_client)                  │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  Console UI  →  send_pkt()  /  recv_pkt()           │  │
│  │  ClientSession (acc_no, grade, credit_limit, role)  │  │
│  └─────────────────────────────────────────────────────┘  │
│                        TCP :9090                          │
└──────────────────────────┬───────────────────────────────┘
                           │  XOR-encrypted Packet structs
┌──────────────────────────▼───────────────────────────────┐
│               SERVER DAEMON (cstock_server)               │
│                                                          │
│  Parent (daemon)   Child per connection (fork)           │
│  ┌─────────────┐   ┌───────────────────────────────────┐ │
│  │ accept loop │──▶│ handle_client()                   │ │
│  │ SIGINT/TERM │   │  decrypt_packet()                 │ │
│  │ SIGCHLD     │   │  dispatch() → srv_* handlers      │ │
│  │ PID file    │   │  Oracle DB connection (per-child) │ │
│  └─────────────┘   └───────────────────────────────────┘ │
└────────────────────────────────┬─────────────────────────┘
                                 │ Pro*C / OCI
┌────────────────────────────────▼─────────────────────────┐
│              Oracle 21c XE  (Docker / WSL2)               │
│            ACCOUNT table  +  HISTORY table               │
└──────────────────────────────────────────────────────────┘
```

### Multi-Process Concurrency

Each accepted TCP connection is handed to a `fork()`-ed child process. The child:

1. Opens its **own** Oracle DB connection (`connect_db()`)
2. Decrypts incoming `Packet` structs with XOR, dispatches to the appropriate `srv_*` handler
3. Closes its DB connection and calls `exit(0)` on disconnect

The parent daemon never touches the DB — it only `accept()`s connections and reaps finished children via a `SIGCHLD` handler (`waitpid` with `WNOHANG`).

---

## Key Features

### Banking Business Logic

| Feature | Detail |
|---|---|
| Account registration | `INSERT` with ORA-00001 (duplicate PK) handling |
| Deposit / Withdrawal | Balance update with `COMMIT`; overdraft limit enforcement |
| Account Transfer | Atomic `COMMIT/ROLLBACK` across two accounts; grade-based transfer fee (NORMAL: ₩500, VIP: free) |
| Transaction History | Paginated display (5 rows/page) using `DECLARE-OPEN-FETCH` cursor; CSV export |
| Fixed-Term Savings | Separate `SAVINGS_BALANCE` column; bulk 5% annual interest payment (admin) |
| Overdraft (Minus Account) | Per-account `CREDIT_LIMIT`; balance allowed to go negative within the limit |
| Password Change | Current-password verification + double-entry confirmation |
| Account Closure | Blocked unless balance is exactly ₩0 (no positive or negative balance) |

### Security & Access Control

| Feature | Detail |
|---|---|
| Role-Based Access | `ROLE='U'` (customer) vs `ROLE='A'` (admin) — separate menu routing |
| Account Lock | 3 consecutive wrong PINs → `IS_LOCKED='Y'`; admin-only unlock |
| XOR Packet Encryption | Every `Packet` struct is XOR-encrypted in-flight with an 8-byte cyclic key (`"CSTK#ATM"`); symmetric so encrypt = decrypt |
| Grade System | `GRADE` column — `NORMAL` / `VIP`; admin can promote accounts |

### Server Infrastructure

| Feature | Detail |
|---|---|
| Daemon Mode | `daemon(1,0)` detaches from terminal; writes PID to `cstock_server.pid` |
| Graceful Shutdown | `SIGINT` / `SIGTERM` → closes listen socket, removes PID file, logs shutdown event before `_exit(0)` |
| Zombie Prevention | `SIGCHLD` handler calls `waitpid(-1, NULL, WNOHANG)` in a loop |
| Multi-row Streaming | History/admin queries stream `RES_MORE_DATA` packets per row, terminated by `RES_END_DATA`; client drains the stream even on early exit to prevent socket desync |
| Audit Logging | `write_log(LogLevel, msg)` appends timestamped entries to `logs/system.log` |
| Stress Test Verified | Server accepts backlog of 128; tested under 50 simultaneous connections |

### Admin Dashboard

| Option | Function |
|---|---|
| View all accounts | Full account list with grade, balance, lock status |
| Unlock account | Reset `FAIL_CNT=0`, `IS_LOCKED='N'` |
| Grant VIP | Set `GRADE='VIP'` |
| Grant overdraft limit | Set `CREDIT_LIMIT` |
| Bulk interest payment | 5% annual interest applied to all accounts with `SAVINGS_BALANCE > 0` |
| System statistics | Aggregate dashboard: total accounts, total deposits, total overdraft exposure, VIP count |

---

## Project Structure

```
c-stock/
├── include/
│   ├── protocol.h          # Packet struct, request/response constants, XOR crypto
│   ├── server_handlers.h   # srv_* function declarations
│   ├── cstock.h            # SessionState, monolith function prototypes
│   └── logger.h            # Logging interface
├── src/
│   ├── server_main.c       # TCP server: socket/bind/listen/accept/fork loop
│   ├── server_handlers.pc  # Pro*C: 18 DB handler functions (srv_login, srv_deposit, ...)
│   ├── client_main.c       # TCP client: console UI + send/recv (no Oracle)
│   ├── db_util.pc          # connect_db, disconnect_db, format_comma, trim_string
│   ├── logger.c            # File-based logging
│   ├── main.c              # Monolithic ATM entry point
│   ├── auth.pc             # Monolith: login, password change
│   ├── banking.pc          # Monolith: deposit, withdraw, transfer, history
│   └── admin.pc            # Monolith: admin menu handlers
├── Makefile
├── build.sh                # Docker container check + make wrapper
├── run.sh                  # Sets LD_LIBRARY_PATH and launches binary
└── logs/
    └── system.log          # Runtime audit log (auto-created)
```

> **Note:** Pro\*C-generated `.c` files (`src/server_handlers.c`, `src/db_util.c`, etc.) and compiled binaries are `.gitignore`d. Only `.pc` source files are version-controlled.

---

## Database Schema

### ACCOUNT

| Column | Type | Description |
|---|---|---|
| `ACC_NO` | `CHAR(20)` | Primary key |
| `USER_NAME` | `VARCHAR2(40)` | Account holder name |
| `BALANCE` | `NUMBER` | Current balance (may be negative within credit limit) |
| `PASSWD` | `CHAR(15)` | PIN code |
| `ROLE` | `CHAR(1)` | `'U'` = customer, `'A'` = admin |
| `GRADE` | `CHAR(10)` | `'NORMAL'` or `'VIP'` |
| `IS_LOCKED` | `CHAR(1)` | `'Y'` = locked |
| `FAIL_CNT` | `NUMBER` | Consecutive login failure count |
| `CREDIT_LIMIT` | `NUMBER` | Overdraft allowance (0 = none) |
| `SAVINGS_BALANCE` | `NUMBER` | Fixed-term savings balance |

### HISTORY

| Column | Type | Description |
|---|---|---|
| `SEQ` | `NUMBER` | Oracle IDENTITY auto-increment |
| `ACC_NO` | `CHAR(20)` | FK → ACCOUNT |
| `TRANS_TYPE` | `VARCHAR2(20)` | `입금`, `출금`, `이체출금`, `이체입금`, `이자지급`, `수수료` |
| `AMOUNT` | `NUMBER` | Transaction amount |
| `BALANCE_AFTER` | `NUMBER` | Running balance after transaction |
| `TRANS_DATE` | `TIMESTAMP` | Transaction timestamp |

---

## Protocol Design

All client-server communication uses a single fixed-size binary `Packet` struct, XOR-encrypted before sending and decrypted immediately after receiving.

```c
typedef struct {
    int  type;        // Request/response kind (PKT_LOGIN, PKT_DEPOSIT, ...)
    char acc_no[21];  // Primary account number
    char extra[64];   // Secondary payload: password, target account, etc.
    char extra2[64];  // Additional payload: new PIN, confirmation, etc.
    long amount;      // Numeric amount
    char data[512];   // Response message or '|'-delimited serialized row data
    int  result;      // Outcome code (RES_OK, RES_FAIL, RES_MORE_DATA, ...)
} Packet;
```

**Multi-row streaming pattern** (history, admin account list):
1. Server sends one `RES_MORE_DATA` packet per row
2. Server sends a single `RES_END_DATA` terminator
3. Client loops until `RES_END_DATA`, draining the stream even on early user exit

**Encryption**: cyclic XOR with key `"CSTK#ATM"` (8 bytes). Because XOR is self-inverse, `encrypt_packet()` and `decrypt_packet()` call the same function.

---

## Getting Started

### Prerequisites

| Requirement | Notes |
|---|---|
| GCC | `sudo apt install gcc make` |
| Docker | Oracle 21c XE container named `oracle21c` must be running for server builds |
| Oracle Instant Client 21 | Installed at `/opt/oracle/instantclient_21_13` on the host |
| WSL2 / Linux | Tested on Ubuntu via WSL2 |

### Start the Oracle Container

```bash
docker start oracle21c
```

### Build

```bash
# Build everything (server + client)
./build.sh all

# Or build individually
./build.sh server   # → cstock_server  (requires Oracle container)
./build.sh client   # → cstock_client  (no Oracle needed)
./build.sh          # → cstock_atm     (monolithic mode)
```

### Run — TCP/IP Mode (Recommended)

```bash
# Terminal 1: Start the server (daemonizes automatically)
./cstock_server
# Output: "Switching to background daemon..."
# PID written to: cstock_server.pid
# Logs written to: logs/system.log

# Terminal 2: Connect a client
./cstock_client

# Stop the server gracefully
kill $(cat cstock_server.pid)
```

### Run — Monolithic Mode

```bash
# Set Oracle library path and launch
./run.sh
# or manually:
export LD_LIBRARY_PATH=/opt/oracle/instantclient_21_13:$LD_LIBRARY_PATH
./cstock_atm
```

### Monitor Logs

```bash
tail -f logs/system.log
```

---

## Build Reference

| Command | Output Binary | Oracle Required |
|---|---|---|
| `make` | `cstock_atm` | Yes (Pro\*C + OCI) |
| `make server` | `cstock_server` | Yes (Pro\*C + OCI) |
| `make client` | `cstock_client` | No |
| `make clean` | — | — |

The `./build.sh` wrapper checks that the `oracle21c` Docker container is running before invoking `make clean && make`.

---

## Development History

| Phase | Date | Highlights |
|---|---|---|
| **Phase 4** | 2026-04-13 | Background daemon (`daemon()`), Graceful Shutdown (`SIGINT`/`SIGTERM`), stress test (50 concurrent clients) |
| **Phase 3** | 2026-04-13 | `fork()`-based multi-client concurrency, XOR packet encryption |
| **Phase 2** | 2026-04-13 | TCP/IP client-server split, binary `Packet` protocol, multi-row streaming |
| — | 2026-04-12 | Fixed-term savings, overdraft (minus account), VIP grade, transfer fee, CSV export, pagination, system stats dashboard |
| — | 2026-03-26 | Account registration (ORA-00001 handling), HISTORY table, account closure |
| — | 2026-03-07 | Role-based access control (admin vs customer), Pro\*C cursor scope fix |
| — | 2026-03-04 | PIN-based login security, password change |
| **Phase 1** | 2026-02-12 | Oracle 21c XE Docker setup, Pro\*C build pipeline, ATM core loop, deposit/withdraw, transfer, `format_comma` |
