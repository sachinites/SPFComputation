CC=gcc
CFLAGS=-g -Wall
INCLUDES=-I . -I ./CommandParser
USECLILIB=-lcli
TARGET:exe
OBJ=graph.o spfdcm.o topo.o
exe:testapp.o ${OBJ} ${CLILIB}
	${CC} ${CFLAGS} ${INCLUDES} testapp.o ${OBJ} -o exe -L ./CommandParser ${USECLILIB}
testapp.o:testapp.c
	${CC} ${CFLAGS} -c ${INCLUDES} testapp.c -o testapp.o
graph.o:graph.c
	${CC} ${CFLAGS} -c ${INCLUDES} graph.c -o graph.o
spfdcm.o:spfdcm.c
	${CC} ${CFLAGS} -c ${INCLUDES} spfdcm.c -o spfdcm.o
topo.o:topo.c
	${CC} ${CFLAGS} -c ${INCLUDES} topo.c -o topo.o
clean:
	rm exe
	rm *.o
