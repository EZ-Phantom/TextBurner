SOURCES =\
    main.cpp

INCLUDES = -I/usr/include/freetype2

LIBS = `pkg-config --cflags --libs opencv` -lfreetype 

all:
	g++ -std=c++17 $(INCLUDES) $(SOURCES) -o testing $(LIBS)

clean:
	rm -rf *.o testing
