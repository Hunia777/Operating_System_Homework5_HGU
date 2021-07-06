all : tfind

tfind : tfind.c
	gcc tfind.c -o tfind -lpthread
clean :
	rm tfind

