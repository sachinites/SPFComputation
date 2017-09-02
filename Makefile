CC=gcc
CFLAGS=-g -Wall
INCLUDES=-I . -I ./CommandParser -I ./LinkedList -I ./Heap -I ./BitOp -I ./logging
USECLILIB=-lcli
TARGET:exe
DSOBJ=LinkedList/LinkedListApi.o Heap/heap.o
OBJ=graph.o prefix.o spfdcm.o topo.o spfcomputation.o spfutil.o ./logging/logging.o ${DSOBJ}
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
prefix.o:prefix.c
	${CC} ${CFLAGS} -c ${INCLUDES} prefix.c -o prefix.o
spfcomputation.o:spfcomputation.c
	${CC} ${CFLAGS} -c ${INCLUDES} spfcomputation.c -o spfcomputation.o
spfutil.o:spfutil.c
	${CC} ${CFLAGS} -c ${INCLUDES} spfutil.c -o spfutil.o
logging.o:./logging/logging.c
	${CC} ${CFLAGS} -c ${INCLUDES} ./logging/logging.c -o ./logging/logging.o
${DSOBJ}:
	(cd LinkedList;  make)
	${CC} ${CFLAGS} -c ${INCLUDES} Heap/heap.c -o Heap/heap.o
clean:
	rm exe
	rm *.o
all:
	(cd LinkedList; make)
	(cd CommandParser; make)
	make
cleanall:
	(cd LinkedList; make clean)
	(cd CommandParser; make clean)
	rm Heap/*.o
	rm BitOp/*.o
	rm logging/*.o
	make clean
