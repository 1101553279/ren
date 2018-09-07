#include <stdio.h>

int one_cal( int n )
{
    int i = 0;

    while(n > 0)
    {
        if( 0x1 & n )
            i++;
        n >>= 1;
    }

    return i;
}

int two_cal( int n )
{
    int i = 0;
    
    while(n > 0)
    {
        if(n % 2 == 1)
            i++;
        n /= 2;
    } 

    return i;
}

int three_cal( int n )
{
    int i = 0;
    unsigned int and = 1;

    while( and > 0 )
    {
        if(n & and)
            i++;
        and <<= 1;
    }

    return i;
}

int four_cal( int n )
{
    int i = 0;

    while(n > 0)
    {
        i++;
        n = (n-1) & n;
    }

    return i;
}

int main( void )
{
    int ret = 0;
    int n = 0x33;
    
    ret = one_cal( n ); 
    printf( "one_cal( %#x ) = %d\n", n, ret );

    ret = two_cal( n );
    printf( "two_cal( %#x  ) = %d\n", n, ret );
    
    ret = three_cal( n );
    printf( "three_cal( %#x  ) = %d\n", n, ret );

    ret = four_cal( n );
    printf( "four_cal( %#x  ) = %d\n", n, ret );

    return 0;
}
