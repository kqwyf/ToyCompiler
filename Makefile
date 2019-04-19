BUILD_PATH = ./build

all: $(BUILD_PATH)
	g++ -O2 main.cpp lex.cpp parser.cpp -o $(BUILD_PATH)/main

debug: $(BUILD_PATH)
	g++ -O2 -DDEBUG main.cpp lex.cpp parser.cpp -o $(BUILD_PATH)/main

$(BUILD_PATH):
	mkdir $(BUILD_PATH)

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
	./build/main -g ./tests/9.src | diff - ./tests/9.out
	./build/main -g ./tests/10.src | diff - ./tests/10.out
	./build/main -g ./tests/11.src | diff - ./tests/11.out
	./build/main -g ./tests/12.src | diff - ./tests/12.out
	./build/main -g ./tests/13.src | diff - ./tests/13.out
	./build/main -g ./tests/14.src | diff - ./tests/14.out
	./build/main -g ./tests/15.src | diff - ./tests/15.out
	./build/main -g ./tests/16.src | diff - ./tests/16.out
	python3 ./LR1.py ./tests/g1.grm -h | diff - ./tests/g1.out
	python3 ./LR1.py ./tests/g2.grm -h | diff - ./tests/g2.out
	python3 ./LR1.py ./tests/g3.grm -h -i | diff - ./tests/g3.out
