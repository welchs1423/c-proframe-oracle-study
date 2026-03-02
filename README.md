# C-Stock ATM
오라클 데이터베이스와 Pro*C를 활용하여 코어 뱅킹(Core Banking) 시스템의 필수 로직을 구현한 콘솔 ATM 애플리케이션입니다.

## 🛠️ 기술 스택 및 환경 (Tech Stack & Environment)
- **Language**: C (Oracle Pro*C)
- **Database**: Oracle Database 21c (XE)
- **Infrastructure**: Docker (Oracle Container), WSL2 (Ubuntu)
- **Compiler**: GCC, `proc` (Oracle Precompiler)
- **Version Control**: Git, GitHub

## 🧠 핵심 구현 기술 (Key Technologies)
- **Embedded SQL**: C 코드 내부에 SQL을 직접 삽입하고 호스트 변수를 바인딩하여 DB와 실시간으로 통신하는 로직 구현.
- **Transaction Management**: 계좌 이체 시 출금과 입금 중 하나라도 실패하면 전체 작업을 취소(`ROLLBACK`)하고, 모두 성공 시에만 반영(`COMMIT`)하는 데이터 무결성(Atomicity) 제어.
- **Cursor Data Fetching**: `DECLARE`, `OPEN`, `FETCH`, `CLOSE` 사이클과 C 언어 루프 제어를 결합하여, 다수의 거래 내역을 안전하게 가져오는 다중 행(Multi-row) 조회 기능 구현.
- **Data Type Conversion**: 오라클 전용 `VARCHAR` 구조체 타입과 C 언어의 `char*` 문자열 간의 형변환 및 널문자(`\0`) 수동 제어를 통한 데이터 잘림(Truncation) 방지.
- **Modular Architecture**: 단일 `main()` 함수에 몰려있던 스파게티 코드를 기능별(로그인, 조회, 입금, 출금, 이체 등)로 분리하여 유지보수성 극대화.

---

## 📖 공통 가이드 및 환경 설정

### 1. 필수 라이브러리 설치
C 언어와 Oracle의 통신을 위해 `libaio1` 패키지 설치.
```bash
sudo apt-get update
sudo apt-get install -y libaio1
```

### 2. Pro*C 컴파일 및 실행 가이드
- **컴파일 순서**:
  1. `.pc` 파일 작성 후 Docker 컨테이너로 전송 (`docker cp`)
  2. 컨테이너 내부에서 `proc` 명령어로 `.c` 파일 생성 (`parse=none` 옵션 필수)
  3. 생성된 `.c` 파일을 다시 호스트(WSL)로 가져와 `gcc`로 빌드
- **주요 컴파일 옵션**:
  - `-I./include`: 오라클 공용 헤더 파일 참조
  - `-L$ORACLE_HOME -lclntsh`: 오라클 클라이언트 공유 라이브러리 링크

---

## 📅 업데이트 내역 (Changelog)

### [2026-03-03]
- **거래 내역 조회**: Pro*C 커서를 활용한 최근 거래 내역 5건 출력 기능 구현.
- **데이터 안정성**: 전역 호스트 변수를 `VARCHAR` 구조체로 변경하여 문자열 처리 안정성 강화.

### [2026-02-25]
- **계좌 이체 시스템**: 출금/입금/내역 저장을 하나의 트랜잭션으로 묶은 이체 로직 구현.
- **UI 개선**: 금액 출력 시 천 단위 콤마(,) 표시를 위한 `format_comma` 함수 도입.

### [2026-02-23]
- **보안 및 세션**: 동적 계좌 로그인 시스템 및 `TRANSACTION_HISTORY` 테이블 설계/연동.
- **리팩토링**: `main` 로직 기능별 함수화 및 코드 구조 최적화.

### [2026-02-12 ~ 02-19]
- **기초 구축**: Docker 기반 Oracle 21c 환경 설정 및 ATM 기본 무한 루프(`switch-case`) 구현.
- **트랜잭션 기초**: 입출금 로직(`SELECT` -> `UPDATE`) 및 한글 인코딩 설정 완료.

---

## 🛠️ 주요 트러블슈팅 (Troubleshooting)

| 날짜 | 이슈 | 원인 및 해결 |
|:---:|:---|:---|
| **03-03** | **ORA-01002** | Pro*C 구형 파서의 최신 `FETCH` 문법 인식 오류 → 기본 쿼리와 C 카운터 변수 조합으로 해결. |
| **02-25** | **ORA-12899** | 한글 바이트 수(UTF-8) 계산 착오로 인한 컬럼 크기 부족 → `VARCHAR2(30)`으로 확장. |
| **02-23** | **PCC-S-02021** | 쿼리 내 한글 리터럴 파싱 오류 → C 변수에 `strcpy` 후 호스트 변수로 바인딩하여 전달. |
| **02-19** | **Syntax Error** | `parse=none` 시 여러 줄 쿼리 파싱 실패 → `EXEC SQL` 문장을 줄바꿈 없이 한 줄로 작성. |
| **02-13** | **Hanging** | TNS 별칭 인식 실패 → **EZConnect**(`127.0.0.1:1521/xepdb1`) 방식으로 즉시 해결. |