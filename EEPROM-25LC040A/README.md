Код компилируется без Arduino # Тестовое задание: драйвер для EEPROM 25LC040A по SPI (bit-banging)

Реализован класс `Eeprom25LC040A` с методами: `readByte` / `writeByte` `readArray` / `writeArray` `readBit` / `writeBit`

Для NOR-памяти (например W25Q128) изменится: 
- Адрес 24-битный (3 байта) 
- Обязательное стирание сектора перед записью 
- Page Program до 256 байт
