#include <stdio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

u8 con_16_to_8( u16 va_16 )
{
    u8 va_8;
    u8 i = 0;

    for( i = 0; i < 8; i++ )
    {
        if( 1 == (0x1 & ( va_16 >> (2*i+1) ) ) )
            va_8 |= 1<<i;
        else
            va_8 &= ~(1<<i);
    }

    return va_8;
}

u16 con_32_to_16( u32 va_32 )
{
    u16 va_16 = 0;
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
    u8 va_8;
    u16 va_16;
    u32 va_32 = 0x33553355;

    va_16 = con_32_to_16( va_32 );

    printf( " va_16 = %x\n", va_16 );

    va_8 = con_16_to_8( va_16 );
    printf( " va_8 = %x\n", va_8 );

    return 0;
}
