CC=../build/bin/clang
dis=../build/bin/llvm-dis
opt=../build/bin/opt
target=x86_64-unknown-linux-gnu

all: bin ll bc bin/main

bin:
	mkdir bin

ll:
	mkdir ll

bc:
	mkdir bc

bin/main: ll/main-opt.ll
	$(CC) -target $(target) ll/main-opt.ll -o bin/main

ll/main-opt.ll: bc/main-opt.bc
	$(dis) < bc/main-opt.bc > ll/main-opt.ll

bc/main-opt.bc: ll/main-sched-pass.ll
	$(CC) -target $(target) -O1 -emit-llvm ll/main-sched-pass.ll -c -o bc/main-opt.bc

ll/main-sched-pass.ll: ll/main-unopt.ll
	$(opt) ll/main-unopt.ll -passes=sched | $(dis) > ll/main-sched-pass.ll

ll/main-unopt.ll: bc/main-unopt.bc
	$(dis) < bc/main-unopt.bc > ll/main-unopt.ll

bc/main-unopt.bc: src/main.c
	$(CC) -target $(target) -Xclang -disable-llvm-passes -O1 -emit-llvm src/main.c -c -o bc/main-unopt.bc

run: all
	./bin/main | less

clean:
	rm -rf bin ll bc

.PHONY: clean run all
