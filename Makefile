CC=gcc
CFLAGS=-g -Wall
INCLUDES=-I . -I ./CommandParser -I ./LinkedList -I ./Heap -I ./BitOp -I ./logging
USECLILIB=-lcli
TARGET:exe
DSOBJ=LinkedList/LinkedListApi.o Heap/heap.o
OBJ=instance.o routes.o prefix.o rlfa.o lfa.o spfdcm.o topo.o spfclihandler.o spfcomputation.o spfutil.o ./logging/logging.o ${DSOBJ}
exe:testapp.o ${OBJ} ${CLILIB}
	@echo "Building final executable"
	@echo "Linking ..."
	@ ${CC} ${CFLAGS} ${INCLUDES} testapp.o ${OBJ} -o exe -L ./CommandParser ${USECLILIB}
	@echo "Executable created. Finished Success"
testapp.o:testapp.c
	@echo "Building testapp.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} testapp.c -o testapp.o
instance.o:instance.c
	@echo "Building instance.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} instance.c -o instance.o
lfa.o:lfa.c
	@echo "Building lfa.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} lfa.c -o lfa.o
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
logging.o:./logging/logging.c
	@echo "Building logging.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} ./logging/logging.c -o ./logging/logging.o
${DSOBJ}:
	(cd LinkedList;  make)
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Heap/heap.c -o Heap/heap.o
clean:
	@ rm exe
	@ rm *.o
	@echo "cleaned compiled/build files"
all:
	(cd LinkedList; make)
	(cd CommandParser; make)
	make
cleanall:
	(cd LinkedList; make clean)
	(cd CommandParser; make clean)
	rm Heap/*.o
	rm logging/*.o
	make clean
