all:
	g++ -O2 main.cpp parser.cpp -o build/main

test: testl

testl:
	./build/main -l ./tests/1.src | diff - ./tests/1.out
	./build/main -l ./tests/2.src | diff - ./tests/2.out
	./build/main -l ./tests/3.src | diff - ./tests/3.out
	./build/main -l ./tests/4.src | diff - ./tests/4.out
	./build/main -l ./tests/5.src | diff - ./tests/5.out
	./build/main -l ./tests/6.src | diff - ./tests/6.out
	./build/main -l ./tests/7.src | diff - ./tests/7.out
	./build/main -l ./tests/8.src | diff - ./tests/8.out
