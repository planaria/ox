CXX ?= clang++

CFLAGS = -std=c++14 -MMD -MP -Wall -Wextra
CFLAGS_DEBUG = -g -O0
CFLAGS_RELEASE = -O3
LDFLAGS = -lpthread -lboost_system

buildtype := release

ifeq ($(buildtype), debug)
	CFLAGS += $(CFLAGS_DEBUG)
else ifeq ($(buildtype), release)
	CFLAGS += $(CFLAGS_RELEASE)
else
	$(error buildtype must be debug or release)
endif

LIBS = 
INCLUDE = -I./include -I./ext/cereal/include -I./ext/Catch/include

TARGETDIR = ./bin/$(buildtype)
TARGET = $(TARGETDIR)/ox_test
SRCDIR = ./tests

SOURCES = $(shell find $(SRCDIR) -name *.cpp)
OBJDIR = ./obj/$(buildtype)
OBJECTS = $(addprefix $(OBJDIR)/, $(SOURCES:$(SRCDIR)/%.cpp=%.o))
DEPENDS = $(OBJECTS:.o=.d)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS) $(LIBS)
	-mkdir -p $(@D)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	-mkdir -p $(@D)
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ -c $<

.PHONY: clean
clean:
	-rm -f $(OBJECTS) $(DEPENDS) $(TARGET)

-include $(DEPENDS)
