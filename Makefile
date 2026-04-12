# ==============================================================
# C-Stock ATM - Makefile
# Pro*C 프리컴파일: Docker(oracle21c) 내부 proc 사용
# GCC 컴파일/링크: 호스트의 Oracle Instant Client 사용
# ==============================================================

TARGET   := cstock_atm
SRC_DIR  := src
OBJ_DIR  := obj

# ── Docker 내 Oracle 경로 ──
ORACLE_CTR          := oracle21c
ORACLE_HOME_CTR     := /opt/oracle/product/21c/dbhomeXE
PROC_BIN            := $(ORACLE_HOME_CTR)/bin/proc
PROC_INC_CTR        := $(ORACLE_HOME_CTR)/precomp/public

# ── 호스트 Oracle Instant Client 경로 ──
ORACLE_HOME_HOST    := /opt/oracle/instantclient_21_13

# ── GCC 설정 ──
CC      := gcc
CFLAGS  := -I./include
LDFLAGS := -L$(ORACLE_HOME_HOST) -lclntsh -Wl,-rpath,$(ORACLE_HOME_HOST)

# ── 소스 파일 목록 ──
PC_SRCS  := $(SRC_DIR)/db_util.pc \
            $(SRC_DIR)/auth.pc    \
            $(SRC_DIR)/banking.pc \
            $(SRC_DIR)/admin.pc

# Pro*C 프리컴파일로 생성될 .c 파일
PC_GEN_C := $(PC_SRCS:.pc=.c)

# 전체 .c 파일 (main.c + 생성된 .c)
ALL_C    := $(SRC_DIR)/main.c $(PC_GEN_C)

# 오브젝트 파일
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(ALL_C))

# ──────────────────────────────────────────────
.PHONY: all clean

all: $(OBJ_DIR) $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# ── .pc → .c : Docker 내부 proc로 프리컴파일 ──
$(SRC_DIR)/%.c: $(SRC_DIR)/%.pc include/cstock.h
	@echo ""
	@echo ">>> [PROC] 프리컴파일: $< → $@"
	@# 1) .pc 파일과 커스텀 헤더를 컨테이너 /tmp 로 복사
	docker cp $<              $(ORACLE_CTR):/tmp/$(notdir $<)
	docker cp include/cstock.h $(ORACLE_CTR):/tmp/cstock.h
	@# 2) proc 실행 (출력 파일명 명시)
	docker exec $(ORACLE_CTR) bash -c \
		"export LD_LIBRARY_PATH=$(ORACLE_HOME_CTR)/lib && \
		 $(PROC_BIN) \
		   iname=/tmp/$(notdir $<) \
		   oname=/tmp/$(notdir $@) \
		   include=/tmp \
		   include=$(PROC_INC_CTR)"
	@# 3) 생성된 .c 를 호스트로 복사, 컨테이너 임시 파일 정리
	docker cp $(ORACLE_CTR):/tmp/$(notdir $@) $@
	docker exec --user root $(ORACLE_CTR) bash -c \
		"rm -f /tmp/$(notdir $<) /tmp/$(notdir $@) /tmp/cstock.h"
	@echo ">>> [PROC] 완료: $@"

# ── .c → .o : GCC 컴파일 ──
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo ">>> [CC]   컴파일: $< → $@"
	$(CC) $(CFLAGS) -c $< -o $@

# ── 링킹 → 최종 실행 파일 ──
$(TARGET): $(OBJS)
	@echo ""
	@echo ">>> [LD]   링킹: $@"
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "======================================================"
	@echo "  빌드 완료! 실행: ./$(TARGET)"
	@echo "======================================================"

# ── 빌드 산출물 정리 ──
clean:
	rm -f $(PC_GEN_C)
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET)
	@echo "[CLEAN] 빌드 산출물 정리 완료."
