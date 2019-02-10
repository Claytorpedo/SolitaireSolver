PROG := batch_runner
SRCDIR := .

THPOOL := threadpool/threadpool

SRCS := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/$(THPOOL)/*.cpp)

# Set up the build directory.
ODIR := $(SRCDIR)/build
ifeq ($(filter debug,$(MAKECMDGOALS)),debug)
 ODIR := $(ODIR)/debug
 PROG := $(PROG)_d
else ifeq ($(filter release,$(MAKECMDGOALS)),release)
 ODIR := $(ODIR)/release
 PROG := $(PROG)_r
endif
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(ODIR)/%.o,$(SRCS))

MKDIRS := $(ODIR) $(ODIR)/$(THPOOL)

CC := g++
COMP_FLAGS := -std=c++17 -Wall -Wextra -pedantic
LINK_FLAGS := -pthread
INCL_DIRS :=

DEBUG_FLAGS := -DDEBUG -g
RELEASE_FLAGS := -O2

.PHONY: all debug release clean help

all:            ## Build the solver.
all: $(MKDIRS) $(PROG)

$(PROG): $(OBJS)
	$(CC) $^ $(LINK_FLAGS) -o $@

$(OBJS): $(ODIR)/%.o : $(SRCDIR)/%.cpp
	$(CC) -c $(INCL_DIRS) $(COMP_FLAGS) $< -o $@

debug:          ## Make debug build.
debug: COMP_FLAGS += $(DEBUG_FLAGS)
debug: all

release:        ## Make release build.
release: COMP_FLAGS += $(RELEASE_FLAGS)
release: all

$(MKDIRS):
	@mkdir -p $@

clean:          ## Clean this project.
	rm -rf $(ODIR) $(PROG) $(PROG)_d $(PROG)_r

help:           ## Display this help.
	@fgrep -h "##" $(MAKEFILE_LIST) | fgrep -v fgrep | sed -e 's/\\$$//' | sed -e 's/##//'
