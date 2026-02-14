# C-STOCK 프로젝트 (C & ProFrame C Study)

C 언어와 Oracle DB(Pro*C)를 이용한 금융 트랜잭션 및 계좌 관리 프로그램 학습 리포지토리입니다.

## 🚀 현재 진행 상황
- **[2026-02-14]** 출금(Withdrawal) 비즈니스 로직 및 예외 처리 구현 완료
- **[2026-02-14]** 빌드 환경 최적화 (로컬 include 폴더 구성)
- **[2026-02-13]** 기본 트랜잭션(`UPDATE`) 및 한글 인코딩 해결
- **[2026-02-12]** 리눅스(WSL2) 및 Oracle 21c DB 개발 환경 구축 완료

---

## 🛠 트러블슈팅 및 업데이트 히스토리

### 📂 [2026-02-14] 출금 로직 구현 및 빌드 환경 최적화
- **GCC 헤더 참조 에러 (sqlda.h, sqlcpr.h)**:
  - **현상**: `fatal error: sqlda.h: No such file or directory` 발생.
  - **원인**: 오라클 전용 헤더 파일들이 표준 include 경로에 없음.
  - **해결**: 오라클 엔진의 `precomp/public` 디렉토리를 로컬 `./include`로 전체 복사 후 `-I./include` 옵션으로 빌드 성공.
- **Docker 실행 오류**:
  - **현상**: `command 'docker' could not be found` 발생.
  - **원인**: Docker Desktop 미실행 및 WSL2 연동 해제.
  - **해결**: Docker Desktop 실행 및 Settings에서 WSL Integration 재활성화.
- **Pro*C 미정의 식별자 에러 (PCC-S-02322)**:
  - **원인**: 호스트 변수명 오타 (`user_kd` vs `user_id`).
  - **해결**: 선언부와 사용부의 변수명을 동일하게 수정.
- **C 언어 링크 에러 (undefined reference)**:
  - **현상**: `printf`를 `prinft`로 오타 입력하여 빌드 중단.
  - **해결**: 소스 코드 내 오타 수정.
- **한글 환경 변수 고정**:
  - **해결**: `export NLS_LANG=KOREAN_KOREA.AL32UTF8`를 `.bashrc`에 등록하여 한글 깨짐 영구 해결.

**✅ 완료 내역**
- [x] **트랜잭션 제어**: 조회(SELECT) 후 조건 검증을 통한 출금(UPDATE) 로직 구현
- [x] **예외 처리**: 잔액 부족 시 업데이트 방지 로직 검증
- [x] **환경 구축**: Docker 내부 오라클 헤더를 로컬 빌드 환경(`./include`)에 통합 완료

---

### 📂 [2026-02-13] 트랜잭션 기초 및 인코딩 설정
- **Pro*C 컴파일 에러 (PCC-S-02201)**: 
  - **원인**: Pro*C 번역기가 C 표준 라이브러리의 매크로 구조를 해석하지 못함.
  - **해결**: `proc` 옵션에 `parse=none`을 추가하여 해결.
- **DB 연결 지연 (Hanging)**: 
  - **원인**: WSL에서 TNS 별칭 인식 실패.
  - **해결**: **EZConnect** 방식(`127.0.0.1:1521/xepdb1`)으로 명시.
- **한글 깨짐 문제**: 
  - **해결**: `NLS_LANG` 설정 주입 및 `VARCHAR` 타입 선언으로 공백 문제 해결.
- **Bash 특수문자(`!`) 인식 오류**: 
  - **해결**: 접속 정보를 **작은따옴표(' ')**로 감싸서 해결.

**✅ 완료 내역**
- [x] `SELECT` -> `UPDATE` -> `COMMIT` 기본 로직 검증 완료
- [x] 테스트 계정 잔액 **5,000,000원** 도달 확인
- [x] Pro*C 프리컴파일부터 GCC 빌드까지의 원라인 커맨드 최적화

---

### 📂 [2026-02-12] 개발 환경 구축 (Initial Setup)
- **에러 ORA-12514**: 
  - **해결**: 서비스 이름을 `xepdb1`으로 수정하여 접속 성공.
- **파일 누락 (sqlcxt.h)**:
  - **해결**: 더미 헤더 파일을 직접 생성하여 컴파일 성공.
- **사용자 권한 문제**:
  - **해결**: `SYS` 계정 접속 시 `AS SYSDBA` 문구 명시.

**✅ 완료 내역**
- [x] Oracle 21c XE Docker 컨테이너 설치 및 실행
- [x] SQL*Plus를 통한 `ACCOUNT` 테이블 설계 및 데이터 인서트
- [x] Oracle Instant Client 21.13 SDK 설정 및 공유 라이브러리 링크 검증

---

## 📖 가이드 및 설정 정보

### 1. 필수 라이브러리 설치
```bash
sudo apt-get update
sudo apt-get install -y libaio1