//
// non-parallel
//   gcc -O3 -Wall -Wno-unknown-pragmas -lm mandel_dd_aa.c -o mandel_dd_aa
// parallel via OpenMP
//   gcc -O3 -Wall -fopenmp -lm mandel_dd_aa.c -o mandel_dd_aa_omp
//
// by default, OpenMP consumes all logical cores
//   time ./mandel_dd_aa_omp 1920 1080 -0.75 0.0 1.0 > out.ppm
//   time OMP_NUM_THREADS=4 ./mandel_dd_aa_omp 1920 1080 -0.75 0.0 1.0 > out.ppm
//
#if defined(_OPENMP)
    #include <omp.h>
#else
    inline int omp_get_thread_num() { return 0; }
#endif

#include "colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "doubledouble.h"

DoubleDouble x2, y2, x0d, y1d;
double Q1LOG2 = 1.44269504088896340735992468100189213742664595415299;
double LOG2 = 0.69314718055994530941723212145817656807550013436026;
double bailout = 128; // with a smaller value there are lines on magn=1
double eps = 1e-17;
double logLogBailout;
unsigned long maxiter;

inline int getcoloridx(unsigned long lastit, double zxd, double zyd) {
    double r, c;
    r = sqrt(zxd*zxd + zyd*zyd);
    c = lastit - 1.28 + (logLogBailout - log(log(r))) * Q1LOG2;
    return fmod((log(c/64+1)/LOG2+0.45), 1)*GRADIENTLENGTH + 0.5;
}

inline void calculate_pixel(double x, double y, unsigned long *lastit, double *zxd, double *zyd, bool *inside) {
    DoubleDouble px, py, zx, zy, xx, yy;
    //px = x*x0d + x2;
    px = dd_mul_d(x0d, x);
    px = dd_add(px, x2);
    //py = y*y1d + y2;
    py = dd_mul_d(y1d, y);
    py = dd_add(py, y2);
    // no Main bulb or Cardoid check to be faster
    zx = dd_new(px.hi, px.lo);
    zy = dd_new(py.hi, py.lo);
    unsigned long i;
    *inside = true;
    int check = 3;
    int whenupdate = 10;
    double hx, hy, d;
    hx = 0;
    hy = 0;
    //for (i = 1; i <= maxiter; i++) {
    for (i = 1; i <= 50000; i++) {
        //xx = zx * zx;
        xx = dd_sqr(zx);
        //yy = zy * zy;
        yy = dd_sqr(zy);
        //if (xx + yy > bailout) {
        if (xx.hi + yy.hi > bailout) {
            *inside = false;
            break;
        }
        // iterate
        //zy = 2 * zx * zy + py;
        //zx = dd_mul_ui(zx, 2);
        //zy = dd_mul(zx, zy);
        zy = dd_add(dd_mul2(zx, zy), py);
        //zx = xx - yy + px;
        zx = dd_add(dd_sub(xx, yy), px);

        // period checking
        d = zx.hi - hx;
        if (d > 0.0 ? d < eps : d > -eps) {
            d = zy.hi - hy;
            if (d > 0.0 ? d < eps : d > -eps) {
                // Period found.
                break;
            }
        }
        if ((i & check) == 0) {
            if (--whenupdate == 0) {
                whenupdate = 10;
                check <<= 1;
                check++;
            }
            // period = 0;
            hx = zx.hi;
            hy = zy.hi;
        }
    }

    *lastit = i;
    *zxd = zx.hi;
    *zyd = zy.hi;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "usage: %s width height centerx centery magnification", argv[0]);
        return 1;
    }
    DoubleDouble temp1;
    unsigned int width = atoi(argv[1]);
    unsigned int height = atoi(argv[2]);
    unsigned char* tmpimage = malloc(width*height*3);
    unsigned char* finalimage = malloc(width*height*3);
    DoubleDouble centerx, centery;
    // centerx = dd_new(-0.7436438870371587, -3.628952515063387E-17);
    // centery = dd_new(0.13182590420531198, -1.2892807754956678E-17);
    centerx = dd_new(strtod(argv[3], NULL), 0);
    centery = dd_new(strtod(argv[4], NULL), 0);
    logLogBailout = log(log(bailout));
    DoubleDouble magn = dd_new(strtod(argv[5], NULL), 0);
    // maxiter = width * sqrt(magn);
    temp1 = dd_sqrt(magn);
    maxiter = width * dd_get_ui(temp1);
    // x0d = 4 / magn / width;
    x0d = dd_ui_div(4, magn);
    x0d = dd_div_ui(x0d, width);
    // x2 = -2 / magn + centerx;
    x2 = dd_si_div(-2, magn);
    x2 = dd_add(x2, centerx);
    // y1d = -4 / magn / width;
    y1d = dd_si_div(-4, magn);
    y1d = dd_div_ui(y1d, width);
    // y2 = 2 / magn * height / width + centery;
    y2 = dd_ui_div(2, magn);
    temp1 = dd_new(height, 0);
    temp1 = dd_div_ui(temp1, width);
    y2 = dd_mul(y2, temp1);
    y2 = dd_add(y2, centery);

    #pragma omp parallel for schedule(static, 1)
    for (unsigned int y = 0; y < height; y++) {
        unsigned int idx;
        unsigned int imgidx = y*width*3;
        unsigned long lastit;
        double zxd, zyd;
        bool inside;

        // have one worker output progress
        if (omp_get_thread_num() == 0)
            fprintf(stderr, "\rR: %f %%", (float)imgidx/(width*height*3)*100);

        for (unsigned int x = 0; x < width; x++) {
            calculate_pixel(x, y, &lastit, &zxd, &zyd, &inside);

            if (inside) {
                tmpimage[imgidx++] = 0;
                tmpimage[imgidx++] = 0;
                tmpimage[imgidx++] = 0;
            } else {
                idx = getcoloridx(lastit, zxd, zyd);
                tmpimage[imgidx++] = colors[idx][0];
                tmpimage[imgidx++] = colors[idx][1];
                tmpimage[imgidx++] = colors[idx][2];
            }
        }
    }

    int aafactor = 5;
    int aareach = aafactor / 2;
    int aaarea = aafactor * aafactor;
    double aafactorinv = 1.0/aafactor;

    #pragma omp parallel for schedule(static, 1)
    for (unsigned int y = 0; y < height; y++) {
        unsigned int idx;
        unsigned int imgidx = y*width*3;
        unsigned int finalidx = imgidx;
        unsigned long lastit;
        double zxd, zyd;
        bool inside;
        int xi, yi;
        unsigned int val1, val2, val3;
        double dx, dy;

        // have one worker output progress
        if (omp_get_thread_num() == 0)
            fprintf(stderr, "\rAA: %f %%", (float)imgidx/(width*height*3)*100);

        for (unsigned int x = 0; x < width; x++) {
            val1 = tmpimage[imgidx++];
            val2 = tmpimage[imgidx++];
            val3 = tmpimage[imgidx++];
            // if pixel is neither at the border nor are its four neighbors
            // different, copy value and continue without antialiasing it
            if (x != 0 && y != 0 && x != width -1 && y != height -1
             && tmpimage[(y+1)*width*3+x*3+0] == val1
             && tmpimage[(y+1)*width*3+x*3+1] == val2
             && tmpimage[(y+1)*width*3+x*3+2] == val3
             && tmpimage[(y-1)*width*3+x*3+0] == val1
             && tmpimage[(y-1)*width*3+x*3+1] == val2
             && tmpimage[(y-1)*width*3+x*3+2] == val3
             && tmpimage[y*width*3+(x+1)*3+0] == val1
             && tmpimage[y*width*3+(x+1)*3+1] == val2
             && tmpimage[y*width*3+(x+1)*3+2] == val3
             && tmpimage[y*width*3+(x-1)*3+0] == val1
             && tmpimage[y*width*3+(x-1)*3+1] == val2
             && tmpimage[y*width*3+(x-1)*3+2] == val3) {
                finalimage[finalidx++] = val1;
                finalimage[finalidx++] = val2;
                finalimage[finalidx++] = val3;
                continue;
            }

            // otherwise do antialiasing
            for (xi = -aareach; xi <= aareach; xi++) {
                dx = xi*aafactorinv;
                for (yi = -aareach; yi <= aareach; yi++) {
                    dy = yi*aafactorinv;
                    if ((xi | yi) != 0) {
                        calculate_pixel(x+dx, y+dy, &lastit, &zxd, &zyd, &inside);
                        if (!inside) {
                            idx = getcoloridx(lastit, zxd, zyd);
                            val1 += colors[idx][0];
                            val2 += colors[idx][1];
                            val3 += colors[idx][2];
                        }
                    }
                }
            }
            finalimage[finalidx++] = val1/aaarea;
            finalimage[finalidx++] = val2/aaarea;
            finalimage[finalidx++] = val3/aaarea;
        }
    }

    // write out image
    printf("P6 %d %d 255\n", width, height);
    fwrite(finalimage, 1, width*height*3, stdout);
    fprintf(stderr, "\n");

    return 0;
}
