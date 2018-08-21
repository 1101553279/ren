#include <stdio.h>

#define NUM     8
#define SIZE    20

unsigned char base[ NUM * SIZE ];

int main( void )
{
    void *link;
    int i = 0;
    void *pb;

    pb = ( void *)base;
    link = 0;
    for( i = 0; i < NUM; i++ )
    {
       *(void **)pb = link;         // ( void **)pb for operate 4 bytes
       link = pb;
       pb = (void *)( (unsigned char *)pb + SIZE );
    }

    for( i = 0; i < NUM; i++ )
    {
        printf( "%d : %#x\r\n", i, *(void **)(base+i*SIZE) );
    }

    printf( "\r\n" );
    i = 0; 
    pb = link;
    while ( 0 != pb )
    {
        i++;
        printf( "%d : %#x\r\n", i, pb );
        pb = *(void **)pb;
    }



    return 0;
}

