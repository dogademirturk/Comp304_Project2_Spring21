
all: install

install:
	g++ -pthread main.cpp -o main

clean:
	rm main

test: 
	 ./main
