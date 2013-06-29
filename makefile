## COMP 310 - PRogramming Assignement1
## Amjad Al-Rikabi
## ID: 260143211

my-test : my-test.o mythreads.o mythreads.h
	gcc -o my-test my-test.o mythreads.o -DHAVE_PTHREAD_RWLOCK=1 -lslack -lrt -lm 

my-test.o : my-test.c mythreads.h
	gcc -c my-test.c -DHAVE_PTHREAD_RWLOCK=1 -lslack -lrt -lm 

mythreads.o : mythreads.c
	gcc -c mythreads.c mythreads.h -DHAVE_PTHREAD_RWLOCK=1 -lslack -lrt -lm 

clean:
	rm *.o 

