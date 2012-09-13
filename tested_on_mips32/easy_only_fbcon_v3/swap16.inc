/*
   #include <stdio.h>

   #include "types.inc"

   uint16_t swap16(uint16_t data);
   int main()
   {
    uint16_t value1, value2;

    value1 = 0x1234;
    value2 = swap16(value1);

    printf("0x%x -> 0x%x\n", value1, value2);

    return 0;
   }

 */


uint16_t swap16(uint16_t data)
{
    uint16_t ans;
    uint16_t value1, value2;

    value1 = (data & 0xff00) >> 8;
    value2 = data & 0x00ff;

    ans = (value2 << 8) | value1;
    return ans;
}

