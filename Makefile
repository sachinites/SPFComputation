CC=gcc
#GCOV=-fprofile-arcs -ftest-coverage
CFLAGS=-g -Wall -O0 ${GCOV}
INCLUDES=-I . -I ./gluethread -I ./Stack -I ./CommandParser -I ./LinkedList -I ./Heap -I ./Queue -I ./mpls -I ./BitOp -I ./Libtrace
USECLILIB=-lcli
TARGET:rpd
TARGET_NAME=rpd
DSOBJ=LinkedList/LinkedListApi.o Heap/heap.o Queue/Queue.o Stack/stack.o gluethread/glthread.o BitOp/bitarr.o Tree/redblack.o
OBJ=advert.o instance.o routes.o prefix.o rlfa.o spfdcm.o topo.o \
	spfclihandler.o spfcomputation.o spfutil.o spftrace.o 				   \
	./Libtrace/libtrace.o mpls/ldp.o mpls/rsvp.o igp_sr_ext.o 			\
	sr_tlv_api.o data_plane.o srms.o conflct_res.o ${DSOBJ}
${TARGET_NAME}:testapp.o ${OBJ}
	@echo "Building final executable : ${TARGET_NAME}"
	@echo "Linking with libcli.a(${USECLILIB})"
	@ ${CC} ${CFLAGS} ${INCLUDES} testapp.o ${OBJ} -o ${TARGET_NAME} -L ./CommandParser ${USECLILIB}
	@echo "Executable created : ${TARGET_NAME}. Finished."
conflct_res.o:conflct_res.c
	@echo "Building conflct_res.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} conflct_res.c -o conflct_res.o
testapp.o:testapp.c
	@echo "Building testapp.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} testapp.c -o testapp.o
data_plane.o:data_plane.c
	@echo "Building data_plane.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} data_plane.c  -o data_plane.o
srms.o:srms.c
	@echo "Building srms.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} srms.c -o srms.o
sr_tlv_api.o:sr_tlv_api.c
	@echo "Building sr_tlv_api.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} sr_tlv_api.c -o sr_tlv_api.o
igp_sr_ext.o:igp_sr_ext.c
	@echo "Building igp_sr_ext.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} igp_sr_ext.c -o igp_sr_ext.o
instance.o:instance.c
	@echo "Building instance.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} instance.c -o instance.o
mpls/ldp.o:mpls/ldp.c
	@echo "Building mpls/ldp.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} mpls/ldp.c -o mpls/ldp.o
mpls/rsvp.o:mpls/rsvp.c
	@echo "Building mpls/rsvp.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} mpls/rsvp.c -o mpls/rsvp.o
advert.o:advert.c
	@echo "Building advert.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} advert.c -o advert.o
rlfa.o:rlfa.c
	@echo "Building rlfa.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} rlfa.c -o rlfa.o
spftrace.o:spftrace.c
	@echo "Building spftrace.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} spftrace.c -o spftrace.o
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
libtrace.o:./Libtrace/libtrace.c
	@echo "Building libtrace.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} ./Libtrace/libtrace.c -o ./Libtrace/libtrace.o
${DSOBJ}:
	(cd LinkedList;  make)
	@echo "Building Queue/Queue.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Queue/Queue.c -o Queue/Queue.o
	@echo "Building Heap/heap.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Heap/heap.c -o Heap/heap.o
	@echo "Building Stack/stack.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Stack/stack.c -o Stack/stack.o
	@echo "Building gluethread/glthread.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} gluethread/glthread.c -o gluethread/glthread.o
	@echo "Building BitOp/bitarr.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} BitOp/bitarr.c -o BitOp/bitarr.o
	@echo "Building Tree/redblack.o"
	@ ${CC} ${CFLAGS} -c -I ./Tree Tree/redblack.c -o Tree/redblack.o
clean:
	rm -f *.o
	rm -f rpd
all:
	(cd CommandParser; make)
	make
cleanall:
	rm -f Heap/*.o
	rm -f Queue/*.o
	rm -f Libtrace/*.o
	rm -f mpls/*.o
	rm -f Stack/*.o
	rm -f gluethread/*.o
	rm -f BitOp/*.o
	rm -f Tree/*.o
	(cd LinkedList; make clean)
	(cd CommandParser; make clean)
	make clean
