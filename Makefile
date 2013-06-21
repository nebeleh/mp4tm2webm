CXX=g++
RM=rm -f

SRCS=converter.cpp
OBJS=$(subst .cc,.o,$(SRCS))

all: converter

converter: $(OBJS)
	$(CXX) -o converter $(OBJS)
    
converter.o: converter.cpp

clean:
	$(RM) converter

dist-clean: clean
	$(RM) converter
