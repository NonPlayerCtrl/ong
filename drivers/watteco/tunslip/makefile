################################################################################
include $(ROOTACT)/rtbase/base/makefile.def
include $(ROOTACT)/watteco/base/makefile.def

################################################################################
#LIB=$(PRJLIB)
#INC=-I../include -I$(PRJINC)


################################################################################
EXE=tunslip6

################################################################################
all	: $(EXE).x

$(EXE).x : $(EXE).o $(LIB)
	$(CC) $(LDFLAGS) -o $@ $^

%.o	: %.c
	$(CC) $(CCFLAGS) $(INC) -c $<

clean	:
	@(rm *.x *.o *.a core 2>/dev/null || exit 0)

info	:
	@(echo $(EXE).x)
