# C-STOCK 프로젝트

C 언어와 Oracle DB를 이용한 계좌 관리 프로그램

## 현재 진행 상황
 - 리눅스 개발 환경 구축 (WSL2)
 - Oracle 21c DB 서버 구축 (Docker)
 - 기본 계좌 테이블 (`ACCOUNT`) 생성 완료
 - C 언어 파일 입출력 로직 구현

## 환경 설정 로그
### 1. 필수 라이브러리 설치
C 언어와 Oracle의 통신을 위해 `libaio1` 패키지 설치.
```bash
sudo apt-get update
sudo apt-get install -y libaio1

### 2026-02-12 환경 구축 완료 내역
- [x] Oracle 21c XE Docker 컨테이너 설치 및 실행
- [x] SQL*Plus를 통한 `ACCOUNT` 테이블 설계 및 데이터 인서트 (`COMMIT` 완료)
- [x] Ubuntu `libaio1` 패키지 설치 및 `sudo` 권한 문제 해결
- [x] Oracle Instant Client 21.13 SDK/Basic 설정
- [x] `.bashrc` 환경 변수 최적화 및 공유 라이브러리(`so`) 링크 검증 완료