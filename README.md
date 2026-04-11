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

### [2026-04-12] Customer Grade System (VIP/NORMAL) & Transfer Fee Logic
- Added `GRADE` column (`VARCHAR2(10)`, default `'NORMAL'`) to the ACCOUNT table via `ALTER TABLE`.
- On login, `GRADE` is fetched from DB and stored in a global host variable; displayed in the welcome message and balance view.
- Transfer fee rules by grade:
  - **NORMAL**: requires `balance >= amount + 500`; deducts both and records a separate `'수수료'` (fee) entry in HISTORY.
  - **VIP**: no fee — transfer amount only, confirmed with a "(VIP — 수수료 없음)" message.
- Added admin menu option **"4. VIP 등급 부여"** (`grant_vip` function): accepts an account number and updates its GRADE to `'VIP'`.
- Admin account list now shows the GRADE column for all accounts.

### [2026-04-12] Account Lock on Password Failures & Admin Unlock
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
