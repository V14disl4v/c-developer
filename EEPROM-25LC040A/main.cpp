#include <cstdint> 
#include <cstddef>  

void digitalWrite(int pin, int value) { (void)pin; (void)value; }
int digitalRead(int pin) { (void)pin; return 0; }
void delay(int ms) { (void)ms; }

/// @name константы пинов
/// @{
const int CS_PIN = 10;  ///< номер пина
const int SCK_PIN = 11; ///< пин для sck (Serial Clock)
const int MOSI_PIN = 12; ///< от микроконтроллер
const int MISO_PIN = 13; ///< в микроконтроллер
/// @}

/// @name Основные команды микросхемы 25LC040A (Microchip)
/// @{
constexpr uint8_t CMD_READ = 0x03;   ///< Read data from memory
constexpr uint8_t CMD_WRITE = 0x02;   ///< Write data to memory
constexpr uint8_t CMD_WREN = 0x06;   ///< Write Enable Latch
constexpr uint8_t CMD_RDSR = 0x05;   ///< Read Status Register
/// @}

/**
 * @brief простой SPI, который дергает пины руками (bit-banging)
 */
class SpiDriver
{
public:
    SpiDriver() = default;
    
    /// @brief включение пина
    void csLow();  

    /// @brief выключение пина
    void csHigh();  

    /**
     * @brief отправляет байт и получает ответ
     * @param data что отправить
     * @return что пришло обратно
     */
    uint8_t transfer(uint8_t data);
};

void SpiDriver::csLow() { digitalWrite(CS_PIN, 0); }
void SpiDriver::csHigh() { digitalWrite(CS_PIN, 1); }

uint8_t SpiDriver::transfer(uint8_t data)
{
    uint8_t received = 0;
    for (int i = 7; i >= 0; i--)
    {
        digitalWrite(MOSI_PIN, (data & (1 << i)) ? 1 : 0);
        digitalWrite(SCK_PIN, 1);
        if (digitalRead(MISO_PIN)) received |= (1 << i);
        digitalWrite(SCK_PIN, 0);
    }
    return received;
}

/**
 * @brief Класс для работы с EEPROM 25LC040A
 *
 * Можно читать/писать байты, массивы и биты.
 * Адрес от 0 до 511 (9 бит).
 */
class Eeprom25LC040A
{
public:
    /**
     * @brief конструктор
     * @param driver указатель на уже созданный объект SpiDriver
     */
    explicit Eeprom25LC040A(SpiDriver* driver) : spi(driver) {};

    uint8_t readByte(uint16_t address);    
    void writeByte(uint16_t address, uint8_t value); 

    void readArray(uint16_t address, uint8_t* buffer, size_t length);   // читает массив
    void writeArray(uint16_t address, const uint8_t* data, size_t length); // пишет массив

    bool readBit(uint16_t address, uint8_t bitPos);     // читает бит
    void writeBit(uint16_t address, uint8_t bitPos, bool value); // пишет бит

private:
    SpiDriver* spi;

    /// @brief выполняет команду WREN
    void enableWrite(); 
    /**
     * @brief проверяет, выполняется ли сейчас запись
     * @return true - чип занят, false - готов
     */
    bool isBusy();    
};


/**
 * @brief чтение одного байта
 * @param address адрес 0…511
 * @return прочитанное значение
 */
uint8_t Eeprom25LC040A::readByte(uint16_t address)
{
    uint8_t cmd = CMD_READ | (((address >> 8) & 1) << 3);  // A8 в бите 3

    spi->csLow();
    spi->transfer(cmd);
    spi->transfer(address & 0xFF);  // младшие 8 бит
    uint8_t val = spi->transfer(0x00);
    spi->csHigh();

    return val;
}

/**
 * @brief апись одного байта
 * @param address адрес
 * @param value значение
 * @note Ждёт завершения записи (WIP=0)
 */
void Eeprom25LC040A::writeByte(uint16_t address, uint8_t value)
{
    enableWrite();

    uint8_t cmd = CMD_WRITE | (((address >> 8) & 1) << 3);

    spi->csLow();
    spi->transfer(cmd);
    spi->transfer(address & 0xFF);
    spi->transfer(value);
    spi->csHigh();

    while (isBusy()) {
        delay(1);  // ждём, пока запишется
    }
}

/**
 * @brief чтение массива байтов (побайтово)
 * @param address  начальный адрес
 * @param buffer буфер приёма
 * @param length количество байт
 * @warning без проверки границ адреса
 */
void Eeprom25LC040A::readArray(uint16_t address, uint8_t* buffer, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        buffer[i] = readByte(address + i);
    }
}

/**
 * @brief записывает блок байт
 * @param address  начальный адрес в памяти
 * @param data     указатель на массив данных для записи
 * @param length   сколько байт записать
 *
 * @warning нет проверки переполнения адреса
 */
void Eeprom25LC040A::writeArray(uint16_t address, const uint8_t* data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        writeByte(address + i, data[i]);
    }
}

/**
 * @brief Читает значение одного бита
 * @param address адрес байта
 * @param bitPos  позиция бита 
 * @return true если бит установлен, иначе false
 */
bool Eeprom25LC040A::readBit(uint16_t address, uint8_t bitPos)
{
    if (bitPos > 7) return false;
    uint8_t b = readByte(address);
    return (b & (1 << bitPos)) != 0;
}

/**
 * @brief изменяет значение одного бита (read → modify → write)
 * @param value   новое значение бита (true = 1, false = 0)
 *
 * @note один read + один write примерно 10 мс на операцию
 */
void Eeprom25LC040A::writeBit(uint16_t address, uint8_t bitPos, bool value)
{
    if (bitPos > 7) return;
    uint8_t b = readByte(address);
    if (value) {
        b |= (1 << bitPos);
    }
    else {
        b &= ~(1 << bitPos);
    }
    writeByte(address, b);
}

/**
 * @brief выдаёт команду Write Enable 
 *
 * @note обязательна перед каждой операцией записи
 * @note после успешной записи latch сбрасывается автоматически
 */
void Eeprom25LC040A::enableWrite()
{
    spi->csLow();
    spi->transfer(CMD_WREN);
    spi->csHigh();
}

/**
 * @brief проверяет бит WIP в регистре статуса
 * @return true - чип занят записью, false - готов к новым командам
 */
bool Eeprom25LC040A::isBusy()
{
    spi->csLow();
    spi->transfer(CMD_RDSR);
    uint8_t status = spi->transfer(0x00);
    spi->csHigh();
    return (status & 1) != 0;  /// бит WIP
}

/// использования класса
int main()
{
    SpiDriver drv;
    Eeprom25LC040A eep(&drv);

    eep.writeByte(0, 0x42);
    uint8_t x = eep.readByte(0);

    return 0;
}
