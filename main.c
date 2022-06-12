#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define LUTN 10000

const double c1 = 0.8359375;
const double c2 = 18.8515625;
const double c3 = 18.6875;
const double m1 = 0.1593017578125;
const double m2 = 78.84375;

typedef struct {
    uint16_t *Y;
    uint16_t *Cb;
    uint16_t *Cr;
} frame;

typedef struct {
    double Rd;
    double Gd;
    double Bd;
} RdGdBd;

typedef struct {
    double Rp;
    double Gp;
    double Bp;
} RpGpBp;

typedef struct {
    double Y;
    double Cb;
    double Cr;
} YCbCr;

typedef struct {
    uint16_t DY;
    uint16_t DCb;
    uint16_t DCr;
} DYDCbDCr;

double clamp(double x) {
    if(x < 0) return 0;
    if(x > 1) return 1;
    return x;
}

double max(double x, double y) {
    return x > y ? x : y;
}

int readframe(frame fr, FILE *fi) {
    int one = fread(fr.Y,2,3840*2160,fi);
    int two = fread(fr.Cb,2,1920*1080,fi);
    int three = fread(fr.Cr,2,1920*1080,fi);
    return one;
}
YCbCr dequantize(DYDCbDCr value) {
    YCbCr ret;
    ret.Y = ((double)(value.DY) - 64.0)/(876.0);
    ret.Cb = ((double)(value.DCb) - 512.0)/(896.0);
    ret.Cr = ((double)(value.DCr) - 512.0)/(896.0);
    return ret;
}
double EOTFind(double Ep) {
    double num = fmaxf(powf(Ep,1/m2)-c1, 0);
    double den = c2 - c3*powf(Ep,1/m2);
    double Y = powf(num/den,1/m1);
    return 10000*Y;
}
double EOTFindlut(double Ep,double *lut) {
    return lut[(unsigned int)(clamp(Ep)*LUTN)];
}
RdGdBd EOTFlut(RpGpBp value,double *lut) {
    RdGdBd ret;
    ret.Rd = EOTFindlut(value.Rp,lut);
    ret.Gd = EOTFindlut(value.Gp,lut);
    ret.Bd = EOTFindlut(value.Bp,lut);
    return ret;
}
RdGdBd yccToRdGdBd(YCbCr value,double *lut) {
    RpGpBp temp;
    temp.Rp = value.Y + 1.4746*value.Cr;
    temp.Gp = value.Y - 0.1645553*value.Cb - 0.57135*value.Cr;
    temp.Bp = value.Y + 1.8814*value.Cb;
    return EOTFlut(temp,lut);
}

void fillarray(double *lut) {
    for(int i = 0; i < LUTN; i++) {
        lut[i] = EOTFind((double)i/(double)LUTN);
    }
}

int main() {
    frame frame;
    double lut[LUTN];
    fillarray(lut);
    frame.Y = malloc(2*3840*2160);
    frame.Cb = malloc(2*1920*1080);
    frame.Cr = malloc(2*1920*1080);
    FILE *hdr = popen("ffmpeg -ss 01:00:00 -i ~/Videos/hdr22.mkv -f rawvideo  -", "r");
    double maxcll =0;
    double maxfall = 0;
    while(readframe(frame,hdr)) {
        double tempfall = 0;
        //printf("yeet\n");
        for(int y = 140; y < 940; y++) {
            for(int x = 0; x < 1920; x++) {
                DYDCbDCr quant = {.DY = frame.Y[y*3840 + x*2], .DCb = frame.Cb[y*1920 + x], .DCr = frame.Cr[y*1920 + x]};
                RdGdBd clls = yccToRdGdBd(dequantize(quant),lut);
                maxcll = max(maxcll, clls.Bd + clls.Gd + clls.Rd);
                tempfall += clls.Bd + clls.Gd + clls.Rd;
            }
        }
        maxfall = max(maxfall, tempfall/(1920*1080));
    }
    fclose(hdr);
    printf("%f %f \n", maxcll, maxfall);
}