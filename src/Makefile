#  MOAS - Mouse Orientiertes Adventure System
#  Copyright (C) 2000 bei Thomas Holz
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

SRC=digi_io.c   grafix.c   if.c    io.c    moas.c  mouse.c	  print.c \
    chk.c  digicore.c  hercbak.c  init.c  misc.c  moas.h  powerbld.c

OBJECTS=digi_io.o grafix.o if.o io.o moas.o mouse.o print.o \
        chk.o digicore.o hercbak.o init.o misc.o powerbld.o

all: $(OBJECTS)
	$(CC) $(CFLAGS) -o moas $(OBJECTS)

.cxx.o:
	$(CC) $(CFLAGS) -c -o $@ $<

# EOF #