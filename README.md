# C-Stock ATM
A console-based ATM application implementing core banking logic using Oracle DB and Pro*C.

## 🛠️ Tech Stack & Environment
- **Language & DB**: C (Oracle Pro*C), Oracle Database 21c (XE)
- **Environment**: Docker, WSL2 (Ubuntu), GCC Compiler
- **Version Control**: Git, GitHub

## 🧠 Core Implementation
- **Embedded SQL & Transactions**: Host variable binding with `COMMIT/ROLLBACK`-based transfer integrity control.
- **Cursor Data Fetching**: Multi-row data processing using the `DECLARE-OPEN-FETCH` cycle.
- **Modular Architecture**: Function-level separation by feature (login, deposit/withdraw, transfer, history).
- **Account Lock Policy**: Tracks consecutive password failures (FAIL_CNT) and automatically locks accounts (IS_LOCKED) upon exceeding the threshold.
- **Grade-based Fee System**: Differential transfer fee logic — NORMAL accounts incur a 500 KRW fee per transfer; VIP accounts are exempt.

---

## 📅 Feature History

### [2026-04-13] TCP/IP Client-Server Architecture

Converted the application from a single-process monolith into two independent programs communicating over TCP sockets.

#### Protocol Design (`include/protocol.h`)
- Defined a fixed-size `Packet` struct exchanged between client and server:
  - `type` — request/response kind (`PKT_LOGIN`, `PKT_DEPOSIT`, …)
  - `acc_no[21]` — primary account number
  - `extra[64]` / `extra2[64]` — secondary payloads (password, target account, new PIN, etc.)
  - `amount` — numeric amount (deposit, withdrawal, credit limit, …)
  - `data[512]` — response message or `|`-delimited serialized row data
  - `result` — outcome code (`RES_OK`, `RES_FAIL`, `RES_LOCKED`, `RES_MORE_DATA`, `RES_END_DATA`, `RES_ADMIN`, …)
- Defined `ClientSession` struct to hold post-login state (acc_no, user_name, db_grade, credit_limit, role) on the client side.
- Server port: **9090** (`SERVER_PORT`).

#### Server (`src/server_main.c` + `src/server_handlers.pc`)
- `server_main.c`: creates a TCP socket, sets `SO_REUSEADDR`, binds to `INADDR_ANY:9090`, listens, and loops on `accept()`. Each accepted connection is handed to `handle_client()`, which reads `Packet` structs with `MSG_WAITALL` and dispatches to the appropriate handler. `SIGINT` triggers a graceful shutdown (closes socket, calls `disconnect_db()`).
- `server_handlers.pc`: Pro*C translation unit containing 18 handler functions (`srv_login`, `srv_deposit`, `srv_withdraw`, `srv_transfer`, `srv_view_history`, `srv_admin_view_accounts`, `srv_pay_interest`, …). Each function reads inputs from the request `Packet`, executes Oracle SQL (reusing the same host-variable + cursor patterns from the original modules), and calls `send_pkt()` to return a response `Packet`.
- **Multi-row streaming**: `srv_view_history`, `srv_download_csv`, and `srv_admin_view_accounts` send one `RES_MORE_DATA` packet per row followed by a single `RES_END_DATA` terminator, keeping the socket buffer consistent regardless of whether the client reads all rows.
- `sqlca` shared with `db_util.pc` via `#define SQLCA_STORAGE_CLASS extern`.

#### Client (`src/client_main.c`)
- Pure C — no Oracle headers or libraries required; links with no `LDFLAGS`.
- Connects to the server via `connect()` and provides the same user menus as the original monolith.
- All Oracle calls are replaced with `send_pkt()` / `recv_pkt(MSG_WAITALL)` round-trips.
- **Pagination safety**: after displaying 5 history rows and the user selects "N", the client continues reading all remaining `RES_MORE_DATA` packets until `RES_END_DATA` to prevent socket buffer desynchronization.
- `ClientSession` is populated from the login response `data` field (`"user_name|grade|credit_limit"` pipe-delimited string).
- CSV download receives streamed row packets and writes them to a local `history_<acc_no>.csv` file.

#### Build System
- `make server` → `cstock_server` (links Oracle Instant Client)
- `make client` → `cstock_client` (no Oracle dependency)
- `make` → `cstock_atm` (original monolith, unchanged)
- `./build.sh server|client|all` — wraps the above with Docker container check and `make clean`.
- `.gitignore` updated: `cstock_server`, `cstock_client`, `src/server_handlers.c` excluded.

---

### [2026-04-12] Fixed-Term Savings, Overdraft, VIP Grade & Transfer Fee

#### Fixed-Term Savings Deposit & Admin Bulk Interest Payment
- Added `SAVINGS_BALANCE` column (`NUMBER`, default `0`, `NOT NULL`) to the ACCOUNT table via `ALTER TABLE`.
- Added customer menu option **"8. 정기 예금 가입"** (`deposit_savings` function): transfers a user-specified amount from `BALANCE` to `SAVINGS_BALANCE`; records a `'예금입금'` entry in HISTORY.
- Balance view (`check_balance`) now displays `SAVINGS_BALANCE` alongside the regular balance.
- Added admin menu option **"6. 일괄 이자 지급 (연 5%)"** (`pay_interest` function): iterates all accounts with `SAVINGS_BALANCE > 0` via an Oracle cursor, applies a 5% annual rate, credits the interest to each account's `BALANCE`, and records a `'이자지급'` entry in HISTORY per account.
- Key Pro*C fix: `trim_string(a_acc_no, 20)` must be called **before** using `a_acc_no` in `WHERE` clauses inside a cursor loop; `CHAR(20)` values fetched into a `char[20]` buffer retain trailing spaces that break Oracle VARCHAR2 equality comparisons if not trimmed first.
- Cursor scope safety maintained: `EXEC SQL WHENEVER NOT FOUND CONTINUE` restored immediately after `CLOSE interest_cursor`.

#### Overdraft (Minus Account) Feature
- Added `CREDIT_LIMIT` column (`NUMBER`, default `0`) to the ACCOUNT table via `ALTER TABLE`.
- On login, `CREDIT_LIMIT` is fetched and stored in a global host variable (`credit_limit`).
- Withdrawal/transfer balance check updated: blocks only when `(current_balance + credit_limit) < requested_amount`, allowing the actual `BALANCE` to go negative within the limit.
- Balance view (`check_balance`) now displays the overdraft limit and remaining available amount alongside the current balance.
- `format_comma` updated to correctly format negative amounts with a leading minus sign.
- Account closure (`delete_account`) now blocks if balance is non-zero in either direction (positive or negative).
- Added admin menu option **"5. 마이너스 한도 부여"** (`grant_credit` function): accepts an account number and a limit amount, then updates `CREDIT_LIMIT` via `UPDATE`.

#### Customer Grade System (VIP/NORMAL) & Transfer Fee Logic
- Added `GRADE` column (`VARCHAR2(10)`, default `'NORMAL'`) to the ACCOUNT table via `ALTER TABLE`.
- On login, `GRADE` is fetched from DB and stored in a global host variable; displayed in the welcome message and balance view.
- Transfer fee rules by grade:
  - **NORMAL**: requires `balance >= amount + 500`; deducts both and records a separate `'수수료'` (fee) entry in HISTORY.
  - **VIP**: no fee — transfer amount only, confirmed with a "(VIP — 수수료 없음)" message.
- Added admin menu option **"4. VIP 등급 부여"** (`grant_vip` function): accepts an account number and updates its GRADE to `'VIP'`.
- Admin account list now shows the GRADE column for all accounts.

#### Admin System Statistics Dashboard
- Added admin menu option **"7. 시스템 통계 대시보드"** (`system_stats_dashboard` function): queries the DB with aggregate functions (`COUNT`, `SUM`, `NVL`, `ABS`) and displays a formatted ASCII table.
- Dashboard metrics: total account count, total bank deposits (`BALANCE + SAVINGS_BALANCE`), total overdraft loan amount (absolute value of negative `BALANCE` sum), and VIP customer count.
- SQL safety: all `SUM()` expressions wrapped in `NVL(..., 0)` to prevent NULL host variable binding errors on empty result sets.
- Pro*C scope safety: `EXEC SQL WHENEVER NOT FOUND CONTINUE` set at the start of the function to prevent cursor-scope leakage from other functions.
#### Transaction History CSV Export
- Added customer menu option **"9. 거래 내역 다운로드 (CSV)"** (`download_history_csv` function): opens an Oracle cursor over the full HISTORY table for the account ordered by SEQ DESC, writes each row to a local CSV file named `history_<acc_no>.csv` with a Korean header row.
- Uses a separate `csv_cursor` declaration alongside the existing `hist_cursor` to avoid cursor-scope conflicts within the same `.pc` translation unit.

#### Transaction History Pagination (5 records per page)
- The history inquiry feature (`view_history`) was updated from a simple full dump to a 5-records-per-page display.
- After every 5 rows fetched from the cursor, the user is prompted **"다음 페이지를 보시겠습니까? (Y/N)"**; entering `N` breaks the display loop while still closing the cursor cleanly.
- Implemented using a `page_count` counter and a `user_quit` flag to distinguish between user-initiated stop and natural end-of-data, each producing a different status message.

#### Modular Refactor & Makefile Build System
- Refactored from a single `db_test.pc` monolith into four Pro*C modules: `db_util.pc`, `auth.pc`, `banking.pc`, `admin.pc`, plus a pure-C `main.c` and `logger.c`.
- Introduced a `Makefile` with Docker-based Pro*C precompilation: each `.pc` file is copied into the `oracle21c` container, compiled with `proc`, and the resulting `.c` is pulled back to the host for GCC compilation.
- `sqlca` is declared once in `db_util.pc` (no `extern` there) and referenced with `#define SQLCA_STORAGE_CLASS extern` in all other `.pc` modules.
- `include/cstock.h` acts as the single shared header: `SessionState` struct and all cross-module function prototypes.

#### File-Based Audit Logging & DevOps Scripts
- Added `include/logger.h` and `src/logger.c`: `write_log(LogLevel, message)` appends timestamped entries to `logs/system.log`, auto-creating the `logs/` directory if absent.
- Log levels: `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` — applied throughout login, transfers, locks, account management, and admin actions.
- Added `build.sh`: checks that the `oracle21c` Docker container is running, calls `make clean` then `make`, and prints a status summary.
- Added `run.sh`: sets `LD_LIBRARY_PATH` for the Oracle Instant Client and launches the binary.

### Account Lock on Password Failures & Admin Unlock
- Added `FAIL_CNT` (failure count) and `IS_LOCKED` (lock flag) columns to the ACCOUNT table for persistent lock state management in the DB.
- On login, `IS_LOCKED = 'Y'` is checked before prompting for a password, blocking access to locked accounts.
- Each wrong password increments `FAIL_CNT` by 1; reaching 3 failures automatically sets `IS_LOCKED = 'Y'`.
- Successful login resets `FAIL_CNT` to 0, restoring normal access.
- Added `unlock_account` to the admin menu — unlocks any account by resetting `FAIL_CNT = 0` and `IS_LOCKED = 'N'` with just an account number.

### [2026-03-26] New Account Registration (Sign Up)
- Implemented new account creation logic using `INSERT INTO` on the ACCOUNT table.
- Added ORA-00001 (duplicate primary key) error catching and handling to maintain data integrity.

### Transaction History Ledger (HISTORY)
- Created the `HISTORY` table with Oracle `IDENTITY`-based auto-incrementing sequence (SEQ).
- Strengthened transaction integrity by automatically recording history on every deposit, withdrawal, and transfer.
- Added a history query feature using an `ORDER BY SEQ DESC` cursor to retrieve the most recent transactions.

### Account Closure
- Implemented account closure business logic using a `DELETE` query.
- Applied a real-world banking rule: closure is only allowed when the account balance is zero.
- Improved the main routing loop to automatically end the session (logout) upon successful closure.

### [2026-03-07] Role-Based Access Control (RBAC) & Feature Integration
- Implemented admin/customer session routing based on the ROLE column in the ACCOUNT table.
- Debugged and resolved a Pro*C global-scope (`WHENEVER NOT FOUND`) cursor bug.
- Fully merged existing banking features (deposit/withdraw, transfer, balance inquiry, etc.) into the role-based structure.

### [2026-03-04] Security Enhancements & Password Change
- **Login Security**: Added PIN (password) verification in addition to account number, strengthening session security.
- **Password Change**: Implemented `change_password` — verifies the current password and requires double entry of the new PIN.

### [2026-03-03] Transaction History & Stability Improvements
- **History Inquiry**: Implemented a feature to display the 5 most recent transactions in table format using a Pro*C cursor.
- **Data Optimization**: Applied Oracle's native `VARCHAR` type for improved string handling stability.

### [2026-02-25] Account Transfer & Output Formatting
- **Transfer**: Built transfer logic that handles withdrawal, deposit, and history recording as a single atomic transaction.
- **Formatting**: Introduced the `format_comma` function to display amounts with thousand-separator commas.

### [2026-02-23] Session Management & Transaction Tracking
- **Dynamic Login**: Implemented a login system that validates the entered account number against the DB in real time.
- **Transaction Tracking**: Integrated the `TRANSACTION_HISTORY` table to automatically record all deposit and withdrawal activity.

### [2026-02-14 ~ 02-19] ATM Core Logic
- **Infinite Loop System**: Built an ATM main menu loop environment using `switch-case`.
- **Basic Transactions**: Implemented essential financial logic for deposits and withdrawals (select then update).

### [2026-02-12 ~ 02-13] Infrastructure & Environment Setup
- **DB Server**: Installed Docker-based Oracle 21c XE container and designed the table schema.
- **Build Pipeline**: Optimized the end-to-end build command from Pro*C precompilation through GCC compilation.
