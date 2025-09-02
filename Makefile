main: main.cpp
	g++ -g main.cpp -o main -lGL -lGLU -lglut

clear:
	rm -f main
