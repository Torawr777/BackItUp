CPP = gcc
FLAGS = -pthread -Wall -Werror
EXEC = BackItUp

default:
	gcc -o BackItUp BackItUp.c -pthread -Wall -Werror

clean:
	rm -f ${EXEC}

run: ${EXEC}
	./${EXEC}

${EXEC}:${OBJS}
	${CPP} ${FLAGS} -o ${EXEC} ${OBJS}

