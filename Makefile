ifeq "${CC}" ""
	CC = gcc
endif

all: mandel_quad mandel_mpfr mandel mandel_dd mandel_dd_aa

mandel_quad: mandel_quad.c
	${CC} -O3 -Wall -lquadmath mandel_quad.c -o mandel_quad

mandel_mpfr: mandel_mpfr.c
	${CC} -O3 -Wall -lmpfr -lm mandel_mpfr.c -o mandel_mpfr

mandel: mandel.c
	${CC} -O3 -Wall -lm mandel.c -o mandel

mandel_dd: mandel_dd.c
	${CC} -O3 -Wall -lm mandel_dd.c -o mandel_dd

mandel_dd_aa: mandel_dd_aa.c
	${CC} -O3 -Wall -Wno-unknown-pragmas -lm mandel_dd_aa.c -o mandel_dd_aa

mandel_dd_aa_omp: mandel_dd_aa.c
	${CC} -O3 -Wall -fopenmp -lm mandel_dd_aa.c -o mandel_dd_aa_omp
