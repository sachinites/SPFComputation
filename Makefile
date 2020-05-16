CC=gcc
#GCOV=-fprofile-arcs -ftest-coverage
CFLAGS=-g -Wall -O0 ${GCOV}
INCLUDES=-I . -I ./gluethread -I ./Stack -I ./CommandParser -I ./LinkedList -I ./Queue -I ./mpls -I ./BitOp -I ./Libtrace -I ./LinuxMemoryManager
USECLILIB=-lcli
TARGET:rpd
TARGET_NAME=rpd
DSOBJ=LinkedList/LinkedListApi.o Queue/Queue.o Stack/stack.o gluethread/glthread.o BitOp/bitarr.o Tree/redblack.o LinuxMemoryManager/mm.o
OBJ=advert.o \
	instance.o \
	routes.o \
	prefix.o \
	rlfa.o \
	spfdcm.o \
	topo.o \
	spfclihandler.o \
	spfcomputation.o \
	spfutil.o \
	spftrace.o \
	./Libtrace/libtrace.o \
	mpls/ldp.o \
	mpls/rsvp.o \
	mpls/mpls_label_mgr.o \
	igp_sr_ext.o \
	sr_tlv_api.o \
	data_plane.o \
	srms.o \
	conflct_res.o \
	complete_spf_path.o \
	glevel.o \
	spring_adjsid.o \
	flex_algo.o	\
	tilfa.o	\
	mem_init.o \
	srte_dcm.o
${TARGET_NAME}:testapp.o ${OBJ} ${DSOBJ}
	@echo "Building final executable : ${TARGET_NAME}"
	@echo "Linking with libcli.a(${USECLILIB})"
	@ ${CC} ${CFLAGS} ${INCLUDES} testapp.o ${OBJ} ${DSOBJ} -o ${TARGET_NAME} -L ./CommandParser ${USECLILIB}
	@echo "Executable created : ${TARGET_NAME}. Finished."
conflct_res.o:conflct_res.c
	@echo "Building conflct_res.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} conflct_res.c -o conflct_res.o
glevel.o:glevel.c
	@echo "Building glevel.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} glevel.c -o glevel.o
srte_dcm.o:srte_dcm.c
	@echo "Building srte_dcm.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} srte_dcm.c -o srte_dcm.o
mem_init.o:mem_init.c
	@echo "Building mem_init.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} mem_init.c -o mem_init.o
testapp.o:testapp.c
	@echo "Building testapp.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} testapp.c -o testapp.o
flex_algo.o:flex_algo.c
	@echo "Building flex_algo.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} flex_algo.c -o flex_algo.o
tilfa.o:tilfa.c
	@echo "Building tilfa.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} tilfa.c -o tilfa.o
data_plane.o:data_plane.c
	@echo "Building data_plane.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} data_plane.c  -o data_plane.o
complete_spf_path.o:complete_spf_path.c
	@echo "Building complete_spf_path.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} complete_spf_path.c -o complete_spf_path.o
srms.o:srms.c
	@echo "Building srms.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} srms.c -o srms.o
sr_tlv_api.o:sr_tlv_api.c
	@echo "Building sr_tlv_api.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} sr_tlv_api.c -o sr_tlv_api.o
igp_sr_ext.o:igp_sr_ext.c
	@echo "Building igp_sr_ext.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} igp_sr_ext.c -o igp_sr_ext.o
spring_adjsid.o:spring_adjsid.c
	@echo "Building spring_adjsid.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} spring_adjsid.c -o spring_adjsid.o
instance.o:instance.c
	@echo "Building instance.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} instance.c -o instance.o
mpls/mpls_label_mgr.o:mpls/mpls_label_mgr.c
	@echo "Building mpls/mpls_label_mgr.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} mpls/mpls_label_mgr.c -o mpls/mpls_label_mgr.o
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
Libtrace/libtrace.o:Libtrace/libtrace.c
	@echo "Building Libtrace/libtrace.o" 
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Libtrace/libtrace.c -o Libtrace/libtrace.o
${DSOBJ}:
	(cd LinkedList;  make)
	@echo "Building Queue/Queue.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Queue/Queue.c -o Queue/Queue.o
	@echo "Building Stack/stack.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} Stack/stack.c -o Stack/stack.o
	@echo "Building gluethread/glthread.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} gluethread/glthread.c -o gluethread/glthread.o
	@echo "Building BitOp/bitarr.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} BitOp/bitarr.c -o BitOp/bitarr.o
	@echo "Building Tree/redblack.o"
	@ ${CC} ${CFLAGS} -c -I ./Tree Tree/redblack.c -o Tree/redblack.o
	@echo "Building Linux Memory Manager LinuxMemoryManager/mm.o"
	@ ${CC} ${CFLAGS} -c -I ./LinuxMemoryManager LinuxMemoryManager/mm.c -o LinuxMemoryManager/mm.o
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
	rm -f LinuxMemoryManager/*.o
	(cd LinkedList; make clean)
	(cd CommandParser; make clean)
	make clean
