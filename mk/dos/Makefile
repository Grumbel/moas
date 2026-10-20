# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Open Watcom cross-build of MOAS for real MS-DOS (DOS/4GW 32-bit).
#
# Requires WATCOM set (nix: open-watcom-bin / open-watcom-v2 wrapper).
#   export WATCOM=...
#   export PATH=$WATCOM/binl:$PATH   # or use the wrapped bin/
#   make -f mk/dos/Makefile
#
# Output: build-dos/moas.exe  (MZ + DOS/4G stub)

ifndef WATCOM
$(error WATCOM is not set — point it at an Open Watcom root)
endif

WCC386 ?= wcc386
WLINK  ?= wlink

# Host tools may live in bin/ (wrapper) or binl/
export PATH := $(WATCOM)/bin:$(WATCOM)/binl:$(PATH)
export INCLUDE := $(WATCOM)/h

SRCDIR := src
OBJDIR := build-dos
SRCS := chk datapath digicore digi_io grafix hercbak if init io lang misc moas moas_globals mouse powerbld print
OBJS := $(addprefix $(OBJDIR)/,$(addsuffix .obj,$(SRCS)))

# -bt=dos   target DOS
# -za99     allow C99 (mixed decls, // comments already ok)
# -d2       full debug info (optional; drop for smaller EXE)
# -w3       warnings
# -zq       quiet
# -dDOS     original hardware paths
CFLAGS := -bt=dos -za99 -d2 -w3 -zq -dDOS -i=$(SRCDIR)

WSTUB := $(firstword $(wildcard $(WATCOM)/binw/wstub.exe) $(wildcard $(WATCOM)/binnt/wstub.exe))

.PHONY: all clean

all: $(OBJDIR)/moas.exe

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.obj: $(SRCDIR)/%.c | $(OBJDIR)
	$(WCC386) $(CFLAGS) -fo=$@ $<

$(OBJDIR)/moas.exe: $(OBJS)
	$(WLINK) name $@ system dos4g \
	  $(if $(WSTUB),option stub=$(WSTUB),) \
	  file { $(OBJS) }

clean:
	rm -rf $(OBJDIR)
