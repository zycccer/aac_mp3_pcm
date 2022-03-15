//
// Created by zyc on 2022/3/15.
//
#include <stdio.h>
#include <stdarg.h>
#define PRINTLNF(format, ...) printf("("__FILE__":%d) %s : "format"\n",__LINE__, __FUNCTION__, ##__VA_ARGS__)
#define PRINT_INT(VALUE) PRINTLNF(#VALUE": %d",VALUE)

void  main(){

    int a_er_errr = 3;
    PRINT_INT(a_er_errr);
}