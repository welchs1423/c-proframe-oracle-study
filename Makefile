# ==============================================================
# C-Stock ATM - Makefile
# 타겟:
#   make          → 기존 모놀리식 ATM (cstock_atm)
#   make server   → TCP/IP 서버 (cstock_server)
#   make client   → TCP/IP 클라이언트 (cstock_client, Oracle 불필요)
# Pro*C 프리컴파일: Docker(oracle21c) 내부 proc 사용
# GCC 컴파일/링크: 호스트의 Oracle Instant Client 사용
# ==============================================================

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

SRC_DIR := src
OBJ_DIR := obj

# ==============================================================
# [1] 모놀리식 ATM (기존 빌드, 하위 호환)
# ==============================================================
TARGET   := cstock_atm

PC_SRCS  := $(SRC_DIR)/db_util.pc \
            $(SRC_DIR)/auth.pc    \
            $(SRC_DIR)/banking.pc \
            $(SRC_DIR)/admin.pc

PC_GEN_C := $(PC_SRCS:.pc=.c)
ALL_C    := $(SRC_DIR)/main.c $(SRC_DIR)/logger.c $(PC_GEN_C)
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(ALL_C))

# ==============================================================
# [2] TCP/IP 서버 (cstock_server)
# ==============================================================
SERVER_TARGET   := cstock_server
SERVER_PC_SRCS  := $(SRC_DIR)/db_util.pc $(SRC_DIR)/server_handlers.pc
SERVER_PC_GEN_C := $(SERVER_PC_SRCS:.pc=.c)
SERVER_ALL_C    := $(SRC_DIR)/server_main.c $(SRC_DIR)/logger.c $(SERVER_PC_GEN_C)
SERVER_OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SERVER_ALL_C))

# ==============================================================
# [3] TCP/IP 클라이언트 (cstock_client, Oracle 링크 불필요)
# ==============================================================
CLIENT_TARGET := cstock_client
CLIENT_ALL_C  := $(SRC_DIR)/client_main.c
CLIENT_OBJS   := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CLIENT_ALL_C))

# ──────────────────────────────────────────────────────────────
.PHONY: all server client clean

all: $(OBJ_DIR) $(TARGET)

server: $(OBJ_DIR) $(SERVER_TARGET)

client: $(OBJ_DIR) $(CLIENT_TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# ── .pc → .c : Docker 내부 proc로 프리컴파일 ──
# protocol.h 도 함께 복사 (server_handlers.pc 가 include)
$(SRC_DIR)/%.c: $(SRC_DIR)/%.pc include/cstock.h include/logger.h include/protocol.h include/server_handlers.h
	@echo ""
	@echo ">>> [PROC] 프리컴파일: $< → $@"
	docker cp $<                       $(ORACLE_CTR):/tmp/$(notdir $<)
	docker cp include/cstock.h         $(ORACLE_CTR):/tmp/cstock.h
	docker cp include/logger.h         $(ORACLE_CTR):/tmp/logger.h
	docker cp include/protocol.h       $(ORACLE_CTR):/tmp/protocol.h
	docker cp include/server_handlers.h $(ORACLE_CTR):/tmp/server_handlers.h
	docker exec $(ORACLE_CTR) bash -c \
		"export LD_LIBRARY_PATH=$(ORACLE_HOME_CTR)/lib && \
		 $(PROC_BIN) \
		   iname=/tmp/$(notdir $<) \
		   oname=/tmp/$(notdir $@) \
		   include=/tmp \
		   include=$(PROC_INC_CTR)"
	docker cp $(ORACLE_CTR):/tmp/$(notdir $@) $@
	docker exec --user root $(ORACLE_CTR) bash -c \
		"rm -f /tmp/$(notdir $<) /tmp/$(notdir $@) \
		        /tmp/cstock.h /tmp/logger.h /tmp/protocol.h /tmp/server_handlers.h"
	@echo ">>> [PROC] 완료: $@"

# ── .c → .o : GCC 컴파일 ──
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo ">>> [CC]   컴파일: $< → $@"
	$(CC) $(CFLAGS) -c $< -o $@

# ── [1] 모놀리식 ATM 링킹 ──
$(TARGET): $(OBJS)
	@echo ""
	@echo ">>> [LD]   링킹: $@"
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "======================================================"
	@echo "  빌드 완료! 실행: ./$(TARGET)"
	@echo "======================================================"

# ── [2] 서버 링킹 (Oracle Instant Client 필요) ──
$(SERVER_TARGET): $(SERVER_OBJS)
	@echo ""
	@echo ">>> [LD]   링킹: $@"
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "======================================================"
	@echo "  서버 빌드 완료! 실행: ./$(SERVER_TARGET)"
	@echo "======================================================"

# ── [3] 클라이언트 링킹 (Oracle 링크 불필요) ──
$(CLIENT_TARGET): $(CLIENT_OBJS)
	@echo ""
	@echo ">>> [LD]   링킹: $@"
	$(CC) -o $@ $^
	@echo ""
	@echo "======================================================"
	@echo "  클라이언트 빌드 완료! 실행: ./$(CLIENT_TARGET)"
	@echo "======================================================"

# ── 빌드 산출물 정리 ──
clean:
	rm -f $(PC_GEN_C) $(SERVER_PC_GEN_C)
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET) $(SERVER_TARGET) $(CLIENT_TARGET)
	@echo "[CLEAN] 빌드 산출물 정리 완료."
