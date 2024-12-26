#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "helper.h"

void arg_checker(int argc, char* argv[]);



int main(int argc, char* argv[]) {
       	arg_checker(argc, argv);

	return 0;
}



void arg_checker(int argc, char* argv[]) {
        if (argc != 5) {
                fprintf(stderr, "Bledna liczba argumentow. Poprawne uzycie ./manager <X1> <X2> <X3> <X4>");
                exit(1);
        }

        int x1 = atoi(argv[1]);
        int x2 = atoi(argv[2]);
        int x3 = atoi(argv[3]);
        int x4 = atoi(argv[4]);

        if ((x1 < 0 || x2 < 0 || x3 < 0 || x4 < 0) || (x1 == 0 && x2 == 0 && x3 == 0 && x4 == 0)) {
                fprintf(stderr, "Niepoprawne argumenty.\n1. Kazdy z argumentow musi byc nieujemny!\n2. Co najmniej jeden argument musi byc rozny od zera!");
                exit(1);
        }
}
