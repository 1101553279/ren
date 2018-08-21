#include <stdio.h>


#define BLK_SIZE  30
#define BLK_NUM   4


/*   __________ __________ ......................... ___________ ________________
 *  |__________|__________|.........................|___________|________________
 */


unsigned char mem_base[ BLK_NUM ][ BLK_SIZE ];
void *link;

int main( void )
{
    unsigned char i = 0;
    void *ptr;

    ptr = ( void * )mem_base;
    link = 0;

    for( i = 0; i < BLK_NUM; i++ )
    {
        *(void **)ptr = link;
        link = ptr;
        ptr = (void*)( (unsigned char *)ptr + BLK_SIZE );
    }

    i = 0;
    ptr = link;
    while( 0 != ptr )
    {
        i++;
        printf( " i = %d, %#x\r\n", i, ptr );
        ptr = *(void **)ptr;
    }
    


    return 0;
}
