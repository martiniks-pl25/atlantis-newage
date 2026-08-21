GAME ?= neworigins

# ccache, when installed, makes a repeated cold build (branch switch, all-clean)
# nearly free. Absent, it expands to nothing and the build works unchanged.
CCACHE := $(shell command -v ccache 2>/dev/null)
CPLUS = $(CCACHE) g++
CC = $(CCACHE) gcc

# Linker, best available first. mold and gold both beat the default bfd on a
# binary this size; gold is deprecated upstream, so mold wins when present.
MOLD := $(shell command -v mold 2>/dev/null)
GOLD := $(shell command -v ld.gold 2>/dev/null)
LINKER ?= $(if $(MOLD),mold,$(if $(GOLD),gold,bfd))

# Debug info. -gsplit-dwarf leaves the DWARF in .dwo files beside each object
# instead of copying it into the linked binary, so the link stays small and
# fast while gdb still sees locals and types. `make DEBUG=-g0` for none.
DEBUG ?= -g -gsplit-dwarf

CFLAGS = $(DEBUG) -I. -I.. -Wall -Werror -std=c++20 -MMD -MP -fuse-ld=$(LINKER)

JOBS ?= $(shell nproc 2>/dev/null || echo 4)
# Parallelize by default; use `make JOBS=1` for a fully serial build
# (`make -j1` only serializes the top-level make, not the sub-makes).
# A sub-make must inherit the parent's jobserver instead of forcing its own -j:
# the parent's -j/--jobserver-auth live in the environment, not in the
# $(MAKEFLAGS) variable at parse time, so the test has to read the environment.
ifeq (,$(findstring jobserver,$(shell echo "$$MAKEFLAGS")))
MAKEFLAGS += -j$(JOBS)
endif

# Objects depend on the flags themselves, not only on the Makefile: a command
# line such as `make DEBUG=-g0` must rebuild whatever those flags apply to,
# otherwise half the binary keeps the previous ones. Rewritten at parse time
# only when CFLAGS actually change, so it does not churn.
FLAGS_STAMP := obj/.cflags
$(shell mkdir -p obj; [ "$$(cat $(FLAGS_STAMP) 2>/dev/null)" = "$(CFLAGS)" ] || printf '%s' "$(CFLAGS)" > $(FLAGS_STAMP))

RULESET_OBJECTS = extra.o map.o monsters.o rules.o world.o quest_setup.o

ENGINE_OBJECTS = aregion.o army.o astring.o battle.o economy.o \
  edit.o faction.o game.o gamedata.o gamedefs.o \
  genrules.o items.o main.o market.o modify.o monthorders.o \
  npc.o object.o orders.o parseorders.o production.o quests.o quest_data.o quest_generator.o runorders.o \
  skills.o skillshows.o specials.o spells.o unit.o \
  events.o events-battle.o events-assassination.o mapgen.o simplex.o namegen.o \
  indenter.o text_report_generator.o simulate.o dungeon.o nexus_entry.o

UNITTEST_SRC = unittest/main.cpp unittest/testhelper.cpp $(wildcard unittest/*_test.cpp)
UNITTEST_OBJECTS = $(patsubst unittest/%.cpp,unittest/obj/%.o,$(UNITTEST_SRC))

OBJECTS =  $(patsubst %.o,obj/%.o,$(ENGINE_OBJECTS)) $(patsubst %.o,$(GAME)/obj/%.o,$(RULESET_OBJECTS))

# Header dependency files emitted by -MMD -MP: make recompiles exactly the
# objects affected by any .cpp or .h change. Objects also depend on Makefile,
# so the first build after this change regenerates every object (and its .d);
# after that, incremental builds never need `all-clean`.
-include $(wildcard obj/*.d $(GAME)/obj/*.d unittest/obj/*.d)

# Rules from the included .d files would otherwise claim the default goal.
.DEFAULT_GOAL := $(GAME)-m

$(GAME)-m: objdir $(OBJECTS)
	$(CPLUS) $(CFLAGS) -o $(GAME)/$(GAME) $(OBJECTS)

# obj/ is shared between GAME=neworigins and GAME=unittest, so the two submakes
# must run sequentially: in parallel they would compile the same engine objects.
all:
	$(MAKE) neworigins
	$(MAKE) unittest

neworigins: FORCE
	$(MAKE) GAME=neworigins

# For a real game variant this force-rebuilds $(GAME)/$(GAME); skip it for
# GAME=unittest, where it would collide with the unittest/unittest target below.
ifneq ($(GAME),unittest)
$(GAME)/$(GAME): FORCE
	$(MAKE) GAME=$(GAME)
endif

# Same shared-obj/ reason: run the two cleans sequentially, not in parallel.
all-clean:
	$(MAKE) neworigins-clean
	$(MAKE) unittest-clean

neworigins-clean:
	$(MAKE) GAME=neworigins clean

unittest-clean:
	$(MAKE) GAME=unittest clean

clean:
	if [ -d obj ]; then rm -rf obj; fi
	if [ -d $(GAME)/obj ]; then rm -rf $(GAME)/obj; fi
	rm -f $(GAME)/html/$(GAME).html
	rm -f $(GAME)/$(GAME)

all-rules: neworigins-rules

neworigins-rules:
	$(MAKE) GAME=neworigins rules

rules: $(GAME)/$(GAME)
	(cd $(GAME); \
	 ./$(GAME) genrules $(GAME)_intro.html $(GAME).css html/$(GAME).html \
	)

.PHONY: unittest
unittest:
	$(MAKE) GAME=unittest unittest-build

.PHONY: test-fast
test-fast: unittest
	./unittest/unittest

unittest-build: unittest/unittest

unittest/unittest: $(filter-out obj/main.o,$(OBJECTS)) $(UNITTEST_OBJECTS)
	$(CPLUS) $(CFLAGS) -o $@ $^

# Battle test executable
.PHONY: test_armor_battle
test_armor_battle:
	$(MAKE) GAME=neworigins test_armor_battle-build

test_armor_battle-build: objdir $(filter-out obj/main.o,$(OBJECTS))
	$(CPLUS) $(CFLAGS) -o test_armor_battle test_armor_battle.cpp $(filter-out obj/main.o,$(OBJECTS))

FORCE:

unittest-objdir: objdir
	if [ ! -d unittest/obj ]; then mkdir unittest/obj; fi

objdir:
	if [ ! -d obj ]; then mkdir obj; fi
	if [ ! -d $(GAME)/obj ]; then mkdir $(GAME)/obj; fi


$(patsubst %.o,$(GAME)/obj/%.o,$(RULESET_OBJECTS)): $(GAME)/obj/%.o: $(GAME)/%.cpp Makefile $(FLAGS_STAMP) | objdir
	$(CPLUS) $(CFLAGS) -c -o $@ $<

$(patsubst %.o,obj/%.o,$(ENGINE_OBJECTS)): obj/%.o: %.cpp Makefile $(FLAGS_STAMP) | objdir
	$(CPLUS) $(CFLAGS) -c -o $@ $<

# If the boost.hpp file is updated, we need to rebuild the unit test files that include it.
$(UNITTEST_OBJECTS): unittest/obj/%.o: unittest/%.cpp external/boost/ut.hpp Makefile $(FLAGS_STAMP) | unittest-objdir
	$(CPLUS) $(CFLAGS) -c -o $@ $<

# Some utility tasks to keep the external header libraries up to date if needed.
EXTERNAL_DIR := external
UT_DIR := $(EXTERNAL_DIR)/boost
JSON_DIR := $(EXTERNAL_DIR)/nlohmann

UT_RELEASE_URL := https://github.com/boost-ext/ut/archive/refs/tags
JSON_RELEASE_URL := https://github.com/nlohmann/json/releases/download

.PHONY: check-libraries
check-libraries: check-ut check-json

.PHONY: check-ut
check-ut:
	@NEEDS_UPDATE=false; \
	if [ ! -f "$(UT_DIR)/ut.hpp" ]; then \
		echo "UT library not found. Preparing to download..."; \
		NEEDS_UPDATE=true; \
	else \
		CURRENT_VERSION=$$(cd $(UT_DIR) && grep -m 1 'BOOST_UT_VERSION' ut.hpp | awk '{print $$3}' | sed "s/'/./g"); \
		CURRENT_VERSION=v$$CURRENT_VERSION; \
		LATEST_TAG=$$(curl -s https://api.github.com/repos/boost-ext/ut/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'); \
		if [ "$$CURRENT_VERSION" != "$$LATEST_TAG" ]; then \
			echo "UT library is outdated (current: $$CURRENT_VERSION, latest: $$LATEST_TAG). Preparing to update..."; \
			NEEDS_UPDATE=true; \
		fi; \
	fi; \
	if $$NEEDS_UPDATE; then \
		LATEST_TAG=$$(curl -s https://api.github.com/repos/boost-ext/ut/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'); \
		TEMP_DIR=$$(mktemp -d); \
		curl -L $(UT_RELEASE_URL)/$$LATEST_TAG.tar.gz | tar -xz -C $$TEMP_DIR; \
		rm -rf $(UT_DIR); \
		mkdir -p $(UT_DIR); \
		cp -r $$TEMP_DIR/*/include/boost/ut.hpp $(UT_DIR); \
		rm -rf $$TEMP_DIR; \
		echo "UT library updated from $$CURRENT_VERSION to $$LATEST_TAG."; \
	else \
		echo "UT library is up-to-date at version $$CURRENT_VERSION."; \
	fi

.PHONY: check-json
check-json:
	@NEEDS_UPDATE=false; \
	if [ ! -f "$(JSON_DIR)/json.hpp" ]; then \
		echo "JSON library not found. Preparing to download..."; \
		CURRENT_VERSION="uninstalled"; \
		NEEDS_UPDATE=true; \
	else \
		CURRENT_VERSION=$$(grep -E '^#define NLOHMANN_JSON_VERSION_(MAJOR|MINOR|PATCH)' $(JSON_DIR)/json.hpp | awk '{print $$3}' | tr '\n' '.'); \
		CURRENT_VERSION=v$${CURRENT_VERSION%?}; \
		LATEST_TAG=$$(curl -s https://api.github.com/repos/nlohmann/json/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'); \
		if [ "$$CURRENT_VERSION" != "$$LATEST_TAG" ]; then \
			echo "JSON library is outdated (current: $$CURRENT_VERSION, latest: $$LATEST_TAG). Preparing to update..."; \
			NEEDS_UPDATE=true; \
		fi; \
	fi; \
	if $$NEEDS_UPDATE; then \
		LATEST_TAG=$$(curl -s https://api.github.com/repos/nlohmann/json/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'); \
		mkdir -p $(JSON_DIR); \
		curl -L $(JSON_RELEASE_URL)/$$LATEST_TAG/json.hpp -o $(JSON_DIR)/json.hpp; \
		echo "JSON library updated from $$CURRENT_VERSION to $$LATEST_TAG."; \
	else \
		echo "JSON library is up-to-date at version $$CURRENT_VERSION."; \
	fi
