#
# Cplex with FBBT fix point - Makefile
#
# (C) Pietro Belotti 2013. This code is released under the Eclipse
# Public License.
#

CC := gcc

OPTFLAGS := -O3

CPPFLAGS := ${OPTFLAGS} -Wall -I${HOME}/.usr/share/cplex125/cplex/include/ilcplex

LDFLAGS := ${OPTFLAGS} -L${HOME}/.usr/share/cplex125/cplex/lib/x86-64_sles10_4.1/static_pic -lcplex -lpthread -lm

HOMEBIN=${HOME}/.usr/bin

OBJ = cpxfbbt_main.o cpxfbbt_callback.o cpxfbbt_createrow.o cmdline.o

all: ${HOMEBIN}/cpxfpfbbt

${HOMEBIN}/cpxfpfbbt: ${OBJ}
	@echo Linking $(@F)
	@$(CC) -o ${HOMEBIN}/cpxfpfbbt $(OBJ) $(LDFLAGS)

%.o: %.c Makefile
	@echo [${CC}] $< 
	@$(CC) ${CPPFLAGS} -c $< 

clean:
	@echo Cleaning up
	@rm -f $(OBJ) 
