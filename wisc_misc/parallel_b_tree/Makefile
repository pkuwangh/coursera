
CC = g++
LD = g++

OPATH = .generated
BINDIR = bin

# list your binaries here
BINARIES = $(BINDIR)/cTree 

# don't modify this line
CCFLAGS = -c  
LDFLAGS = -lm -pthread  

# add other CFLAGS here
CCFLAGS += -O2 -std=c++11
CCFLAGS += -g -g3 -ggdb

LINK_MESSAGE = "\033[32;1m\n***Making binary \033[33m%s\033[32m...\n\033[m"
COMPILE_MESSAGE = "\033[32;1m\n*Compiling \033[33m%s\033[32m...\n\033[m"

# list OBJFILES here for each binary
# for files with header dependencies, include build rule below,
# otherwise default build rule is sufficient
OBJFILES_COMMON = $(OPATH)/fatals.o  
OBJFILES_CTREE  = $(OBJFILES_COMMON) \
			      $(OPATH)/main.o \
				  $(OPATH)/Barrier.o \
				  $(OPATH)/ProcMap.o \
				  $(OPATH)/CTree.o \
				  $(OPATH)/Tests.o \
				  $(OPATH)/Transactions.o \
				  $(OPATH)/Stats.o


all: $(OPATH) $(BINDIR) $(BINARIES)
	@printf "\033[34;1m\nMy work here is done.\n\033[m"

$(OPATH):
	@printf "\033[32;1m\nMaking \033[33m$(OPATH)/ \033[32mpath for object files...\n\033[m"
	mkdir $(OPATH)

$(BINDIR):
	@printf "\033[32;1m\nMaking \033[33m$(BINDIR)/ \033[32mpath for binaries...\n\033[m"
	mkdir $(BINDIR)

# binary targets are like this
$(BINDIR)/cTree: $(OBJFILES_CTREE)
	@printf $(LINK_MESSAGE) "cTree"
	$(LD) $(LDFLAGS) $(OBJFILES_CTREE) -o $@

$(OPATH)/fatals.o: fatals.cpp fatals.h
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@
	
$(OPATH)/ProcMap.o: ProcMap.C ProcMap.h
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@

$(OPATH)/Barrier.o: Barrier.C Barrier.h fatals.h
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@

$(OPATH)/CTree.o: CTree.C CTree.h
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@

$(OPATH)/main.o: main.C fatals.h ProcMap.h Barrier.h CTree.h Tests.h Transactions.h Stats.h
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@

$(OPATH)/Transactions.o: Transactions.C Transactions.h CTree.h Tests.h Stats.h
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@

$(OPATH)/Tests.o: Tests.C Tests.h CTree.h Barrier.h Transactions.h Stats.h
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@

$(OPATH)/Stats.o: Stats.C Stats.h
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@

# default build rule
$(OPATH)/%.o: %.c $(OPATH)
	@printf $(COMPILE_MESSAGE) "$<"
	$(CC) $(CCFLAGS) $< -o $@

almost_clean:
	@printf "\033[34;1m\nMaking \033[31;1malmost\033[34;1m clean ... (saving binaries)\n\033[m"
	-rm -rf core* *.o
	-rm -rf $(OPATH)

clean:
	@printf "\033[34;1m\nCleaning up...\n\033[m"
	-rm -rf core* $(BINARIES) *.o
	-rm -rf $(BINDIR)
	-rm -rf $(OPATH)
	
