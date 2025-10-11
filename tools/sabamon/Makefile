EXE = sabamon

CC=gcc
CXX=g++
LEX = flex
YACC = bison 
GPERF = gperf
LIBS = -lncurses
ADD_ON = monitor.do cmzasm.ba

#CC=/app/purify-4.5.1-solaris2/purify -chain-length=20 gcc
#CC=/app/quantify-4.5.1-solaris2/quantify gcc
#CFLAGS := -Wall -DMODULE -DDEBUG -D__KERNEL__ -DLINUX -I/usr/src/linux/include
CFLAGS = -Wfatal-errors -Wall -Wno-pointer-sign -g3 
LEX_OPT = -f -i
YACC_OPT = -y -d
GPERF_OPT = -t -m50
#CFLAGS := -O3 -fomit-frame-pointer -funroll-loops
all: $(EXE)

L_FILES = $(wildcard *.l)
Y_FILES = $(wildcard *.y)
C_FILES = $(wildcard *.c)
CPP_FILES = $(wildcard *.cpp)
H_FILES = $(wildcard *.h)
G_FILES = $(wildcard *.g)

OBJ_FILES = $(Y_FILES:.y=.o) $(C_FILES:.c=.o) $(CPP_FILES:.cpp=.o) $(L_FILES:.l=.o) $(G_FILES:.g=.o)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<

.l.c:
	$(LEX) $(LEX_OPT) -t >$@ $<

.y.c:
	$(YACC) $(YACC_OPT) -o $@ $<

%.c: %.g
	$(GPERF) $(GPERF_OPT) >$@ $<
        
$(OBJ_FILES): $(H_FILES)

$(EXE): $(OBJ_FILES)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o $(Y_FILES:.y=.c) $(L_FILES:.l=.c) $(EXE)

tar: 
	if mkdir backup; then echo "insknie"; fi
	tar cvzf backup/$(EXE)_` date +%02d.%02m.%02y_%H_%M`.tar.gz \
	$(ADD_ON) $(L_FILES) $(Y_FILES) $(C_FILES) $(CPP_FILES) $(H_FILES) $(G_FILES) Makefile

