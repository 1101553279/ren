#include <stdio.h>


int pbuf_header( short inc )
{
    if( inc > 0 )
        return inc;
    else if( inc < 0 )
        return -inc;

    return 0;
}

int main( void )
{
    unsigned short newlen = 30;
    unsigned short len = 40;
    int grow;
    unsigned short ugrow;

    grow = newlen - len;
    ugrow = (unsigned short )grow;


    printf( "newlen = %d, len = %d, grow = %d, ugrow = %u\r\n", newlen, len, grow, ugrow );

    newlen += grow;
    printf( "newlen = %d\r\n", newlen );
    
    printf( "header = %d\r\n", pbuf_header( -3 ) );
    printf( "header = %d\r\n", pbuf_header( 3 ) );

    return 0;
}
