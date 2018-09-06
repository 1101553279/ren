#include <stdio.h>


typedef unsigned short u16;
typedef unsigned int u32;

u16 con_32_to_16( u32 va_32 )
{
    u16 va_16;
    u16 i = 0;

    for( i = 0; i < 16; i++ )
    {
        if( 1 == (0x1 & ( va_32 >> (2*i+1) ) ) )
            va_16 |= 1<<i;
        else
            va_16 &= ~(1<<i);
    }

    return va_16;
}


int main( void )
{ 
    u16 va_16;
    u32 va_32 = 0b11111111111101010000000000000000;

    va_16 = con_32_to_16( va_32 );

    printf( " va_16 = %x\n", va_16 );

    return 0;
}
