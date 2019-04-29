DEBUG ?= 1
CXX = g++
CXXFLAGS = -g -O0 -std=c++0x
SHELL = /bin/bash
VERSION = 1.0
EXTRAFLAGS  =  -I../ -I../../xtra_rhelz.x/include -I../../xtra_rhelz.x/include/google -L../../xtra_rhelz.x/libs/boost -L../../xtra_rhelz.x/libs/google
EXTRAFLAGS += -lzookeeper_mt -lboost_system -lboost_thread -lpthread -lgtest -lrt

OBJDIR = ./obj
TARGET_DIR=bin

vpath %.cpp ./ ../

srcs = $(filter-out main.cpp, $(notdir $(wildcard ../*.cpp)))
objs = $(srcs:%.cpp=$(OBJDIR)/%.o)
esrcs= $(filter-out main.cpp, $(notdir $(wildcard *.cpp)))
exes = $(esrcs:%.cpp=$(TARGET_DIR)/%)

all : $(exes)
.PHONY : all

$(objs) : $(OBJDIR)/%.o: %.cpp
	@test -d $(OBJDIR) || mkdir $(OBJDIR)
	$(CXX) -MMD -c $(CXXFLAGS) $< -o $@  $(EXTRAFLAGS) 

$(exes) : $(TARGET_DIR)/%: %.cpp $(objs)
	@test -d $(TARGET_DIR) || mkdir $(TARGET_DIR)
	$(CXX) -MMD $(CXXFLAGS) $< $(objs) -o $@  $(EXTRAFLAGS) 

#check header for obj reconstruction
-include $(OBJDIR)/*.d

.PHONY : clean 
clean :	
	-rm -fr $(OBJDIR)
	-rm -fr $(TARGET_DIR)
