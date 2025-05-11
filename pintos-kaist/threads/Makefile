include ../Makefile.kernel

# make thread 명령어로 test 가능
.PHONY: thread
thread:
	make clean && \
	make -j && \
	cd ./build && \
	make check