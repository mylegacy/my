uint16_t swap16(uint16_t data)
{
    uint16_t ans;
    uint16_t value1, value2;

    value1 = (data & 0xff00) >> 8;
    value2 = data & 0x00ff;

    ans = (value2 << 8) | value1;
    return ans;
}

