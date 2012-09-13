/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define uint32_t    unsigned long
#define char_t      char

typedef union
{
    uint32_t word;
    char_t   byte[4];
} uint32_u_t;

static void endian_fix_16bit(uint32_t source[], uint32_t target[], uint32_t size)
{
    uint32_t   i;
    uint32_u_t value1, value2;

    for (i = 0; i < size / 4; i++)
    {
        /*
         * target[i] = source[i]
         */
        value1.word    = source[i];
        value2.byte[0] = value1.byte[1];
        value2.byte[1] = value1.byte[0];
        value2.byte[2] = value1.byte[3];
        value2.byte[3] = value1.byte[2];

        //value3.byte[0] = 0;
        //value2.byte[1] = 0;
        //value2.byte[2] = 0;
        //value2.byte[3] = 0;

        target[i] = value2.word;
    }
}

