#include "Types.h"

void CRC_BuildTable();

u32 CRC_Calculate( u32 crc, const void *buffer, u32 count );
u32 CRC_CalculatePalette( u32 crc, const void *buffer, u32 count );
u32 Adler32(u32 crc, const void *buffer, u32 count);
