#ifndef MY_FLOAT_H
#define MY_FLOAT_H

typedef int myfloat;

/*my q*/
#define myf_q 16
/*my f*/
#define myf_f (1<<(myf_q))
/*convert int to myfloat*/
#define convert_to_f(n) ((n)*(myf_f))
/*convert myfloat to int rounding toward zero*/
#define convert_to_int_to_zero(x) ((x)/(myf_f))
/*convert myfloat to int rounding to nearest*/
#define convert_to_int_to_near(x) x>=0?(((x)+(myf_f)/2)/(myf_f)):(((x)-(myf_f)/2)/myf_f)
/*floats add*/
#define f_add(x,y) ((x)+(y)) 
/*floats subtract*/
#define f_sub(x,y) ((x)-(y)) 
/*float add int*/
#define n_add(x,n) ((x)+(n)*(myf_f))
/*float substract int*/
#define n_sub(x,n) ((x)-(n)*myf_f)
/*float multiply*/
#define f_mul(x,y) (((int64_t)(x))*(y)/myf_f)
/*float multiply int*/
#define n_mul(x,n) ((x)*(n))
/*floats divide*/
#define f_div(x,y) (((int64_t)(x))*myf_f/(y))
/*float divide int*/
#define n_div(x,n) ((x)/(n))



#endif