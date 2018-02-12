CC=gcc
#GCOV=-fprofile-arcs -ftest-coverage
CFLAGS=-g -Wall -O0 ${GCOV}
INCLUDES=-I . -I ./CommandParser -I ./LinkedList -I ./Heap -I ./Queue -I ./BitOp -I ./logging
USECLILIB=-lcli
TARGET:rpd
TARGET_NAME=rpd
DSOBJ=LinkedList/LinkedListApi.o Heap/heap.o Queue/Queue.o
OBJ=advert.o rttable.o instance.o routes.o prefix.o rlfa.o spfdcm.o topo.o spfclihandler.o spfcomputation.o spfutil.o ./logging/logging.o ${DSOBJ}
${TARGET_NAME}:testapp.o ${OBJ}
	@echo "Building final executable : ${TARGET_NAME}"
	@echo "Linking with libcli.a(${USECLILIB})"
	@ ${CC} ${CFLAGS} ${INCLUDES} testapp.o ${OBJ} -o ${TARGET_NAME} -L ./CommandParser ${USECLILIB}
	@echo "Executable created : ${TARGET_NAME}. Finished."
testapp.o:testapp.c
	@echo "Building testapp.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} testapp.c -o testapp.o
instance.o:instance.c
	@echo "Building instance.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} instance.c -o instance.o
advert.o:advert.c
	@echo "Building advert.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} advert.c -o advert.o
rlfa.o:rlfa.c
	@echo "Building rlfa.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} rlfa.c -o rlfa.o
spfdcm.o:spfdcm.c
	@echo "Building spfdcm.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} spfdcm.c -o spfdcm.o
topo.o:topo.c
	@echo "Building topo.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} topo.c -o topo.o
prefix.o:prefix.c
	@echo "Building prefix.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} prefix.c -o prefix.o
routes.o:routes.c
	@echo "Building routes.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} routes.c -o routes.o
spfcomputation.o:spfcomputation.c
	@echo "Building spfcomputation.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} spfcomputation.c -o spfcomputation.o
spfutil.o:spfutil.c
	@echo "Building spfutil.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} spfutil.c -o spfutil.o
spfclihandler.o:spfclihandler.c
	@echo "Building spfclihandler.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} spfclihandler.c -o spfclihandler.o
rttable.o:rttable.c
	@echo "Building rttable.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} rttable.c -o rttable.o
logging.o:./logging/logging.c
	@echo "Building logging.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} ./logging/logging.c -o ./logging/logging.o
${DSOBJ}:
	(cd LinkedList;  make)
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Queue/Queue.c -o Queue/Queue.o
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Heap/heap.c -o Heap/heap.o
clean:
	rm rpd
	rm *.o
all:
	(cd CommandParser; make)
	make
cleanall:
	rm Heap/*.o
	rm Queue/*.o
	rm logging/*.o
	(cd LinkedList; make clean)
	(cd CommandParser; make clean)
	make clean
