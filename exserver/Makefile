
vpath %.cpp src

RM := rm -f

CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Weffc++ -g3 -Og -ggdb
CPPFLAGS := 
TARGET_ARCH := 
LDFLAGS := -Wl,-O1,--sort-common,--as-needed,-z,relro,-z,now
LIBS := -pthread -lboost_program_options

ifeq ($(CC),gcc)
CXXFLAGS := $(CXXFLAGS) -fanalyzer
endif

SRCS := $(notdir $(wildcard src/*.cpp))
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

TARGET := exserver

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)
