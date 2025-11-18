#include "SFLib.h"
#include <SPI.h>

/* Norflash spi init */
void SFLib::begin(int cs_pin)
{
    _cs_pin = cs_pin;
    // gpio init
    if((_hold_pin == -1)&&(NORFLASH_HOLD_PIN > 0)) _hold_pin = NORFLASH_HOLD_PIN;
    if(_hold_pin > 0){
        pinMode(_hold_pin, OUTPUT);
        digitalWrite(_hold_pin, HIGH);
    }
    if((_wp_pin == -1)&& (NORFLASH_WP_PIN > 0)) _wp_pin = NORFLASH_WP_PIN;
    if(_wp_pin > 0){
        pinMode(_wp_pin, OUTPUT);
        digitalWrite(_wp_pin, HIGH);
    }

    if((_cs_pin == -1) && (NORFLASH_CS_PIN > 0)) _cs_pin = NORFLASH_CS_PIN;
    if(_cs_pin > 0){
        pinMode(_cs_pin, OUTPUT);
        digitalWrite(_cs_pin, HIGH);
    }
#ifdef HARDWARE_SPI
    #if (defined STM32F4xx) || (defined STM32F1xx)
    SPI.setMISO(NORFLASH_MISO_PIN);
	SPI.setMOSI(NORFLASH_MOSI_PIN);
	SPI.setSCLK(NORFLASH_CLK_PIN);
    #endif
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV64);
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE3));
#else
    pinMode(NORFLASH_CLK_PIN, OUTPUT);
    pinMode(NORFLASH_MOSI_PIN, OUTPUT);
    pinMode(NORFLASH_MISO_PIN, INPUT);
    digitalWrite(NORFLASH_CLK_PIN, LOW);
    delay(1);
#endif
    // check write enable status
    write_enable();
    uint8_t data = read_status();
#ifdef NORFLASH_DEBUG_ENABLE
    Serial.printf("norflash write enable status:");
    Serial.println(data, BIN);
#endif
    // read device id
    uint16_t device_id = read_id();
#ifdef NORFLASH_DEBUG_ENABLE
    Serial.printf("norflash device id: 0x%04X", device_id);
#endif
}
void SFLib::begin()
{
    begin(-1);
}

/* Norflash write one byte */
void SFLib::write_byte(uint8_t data)
{
#ifdef HARDWARE_SPI
    // SPI.transfer(data);
    SPI.transfer(data);
#elif (defined SOFTWARE_SPI)
    for(uint8_t i = 0; i < 8; i++)
    {
        uint8_t status;
        status = data & (0x80 >> i);
        digitalWrite(NORFLASH_MOSI_PIN, status);
        digitalWrite(NORFLASH_CLK_PIN, LOW);
        digitalWrite(NORFLASH_CLK_PIN, HIGH);
    }
#endif
}

/* Norflash read one byte */
uint8_t SFLib::read_byte(uint8_t tx_data)
{
#ifdef HARDWARE_SPI
    uint8_t data = 0;
    data = SPI.transfer(tx_data);
    return data;
#elif (defined SOFTWARE_SPI)
    uint8_t i = 0, data = 0;
    for(i = 0; i < 8; i++)
    {
        digitalWrite(NORFLASH_CLK_PIN, HIGH);
        digitalWrite(NORFLASH_CLK_PIN, LOW);
        if(digitalRead(NORFLASH_MISO_PIN))
        {
            data |= 0x80 >> i;
        }
    }
    return data;
#endif
}
/* Read arbitrary length data */
void SFLib::read_data(uint32_t addr24, uint8_t *pbuf, uint32_t len)
{
    check_busy("read_data");
    digitalWrite(_cs_pin, LOW);
    write_byte(READ_DATA_CMD);
    write_byte((uint8_t)(addr24 >> 16));
    write_byte((uint8_t)(addr24 >> 8));
    write_byte((uint8_t)addr24);
    for(uint32_t i = 0; i < len; i++)
    {
        *pbuf = read_byte(0xFF);
        pbuf ++;
    }
    digitalWrite(_cs_pin, HIGH);
}
/* Write less than oneblock(256 byte) data */
void SFLib::write_one_block_data(uint32_t addr, uint8_t * pbuf, uint16_t len)
{
    uint16_t i;
    check_busy("write_one_block_data");
    write_enable();
    digitalWrite(_cs_pin, LOW);
    write_byte(PAGE_PROGRAM_CMD);
    write_byte((uint8_t)(addr >> 16));
    write_byte((uint8_t)(addr >> 8));
    write_byte((uint8_t)addr);
    for(i = 0; i < len; i++)
    {
        write_byte(*pbuf++);
    }
    digitalWrite(_cs_pin, HIGH);
    check_busy("write_one_block_data");
}

/* Write less than one sector(4096 byte) length data  */
void SFLib::write_one_sector_data(uint32_t addr, uint8_t * pbuf, uint16_t len)
{
    uint16_t free_space, head, page, remain;
    free_space = ONE_PAGE_SIZE - addr % ONE_PAGE_SIZE;
    if(len <= free_space)
    {
        head = len;
        page = 0;
        remain = 0;
    }
    if(len > free_space)
    {
        head = free_space;
        page = (len - free_space) / ONE_PAGE_SIZE;
        remain = (len - free_space) % ONE_PAGE_SIZE;
    }

    if(head != 0)
    {
    #ifdef NORFLASH_DEBUG_ENABLE
        Serial.print("head:");
        Serial.println(head);
    #endif
        write_one_block_data(addr, pbuf, head);
        pbuf = pbuf + head;
        addr = addr + head;
    }
    if(page != 0)
    {
    #ifdef NORFLASH_DEBUG_ENABLE
        Serial.print("page:");
        Serial.println(page);
    #endif
        for(uint16_t i = 0; i < page; i++) 
        {
            write_one_block_data(addr, pbuf, ONE_PAGE_SIZE);
            pbuf = pbuf + ONE_PAGE_SIZE;
            addr = addr + ONE_PAGE_SIZE;
        }
    }
    if(remain != 0)
    {
    #ifdef NORFLASH_DEBUG_ENABLE
        Serial.print("remain:");
        Serial.println(remain);
    #endif
        write_one_block_data(addr, pbuf, remain);
    }
}

/* Write arbitrary length data */
void SFLib::write_arbitrary_data(uint32_t addr, uint8_t* pbuf, uint32_t len)
{
	uint32_t secpos;
	uint16_t secoff;
	uint16_t secremain;
 	uint16_t i;
    uint8_t *write_buf = pbuf;
    uint8_t save_buffer[4096];  // save sector original data and add new data

 	secpos = addr / 4096;       // sector number
	secoff = addr % 4096;       // sector offset
	secremain = 4096 - secoff;  // sector remaining space

 	if(len <= secremain)
  {
      secremain = len;        // sector remaining space less than 4096
  }
	while(1)
	{
		read_data(secpos * 4096, save_buffer, 4096); // read sector data
		for(i = 0; i < secremain; i++)
		{// check data, if all data is 0xFF no need erase sector
			if(save_buffer[secoff + i] != 0XFF)
      {// need erase sector
          break;
      }
		}
		if(i < secremain)
		{// erase sector and write data
			sector_erase(secpos);
			for(i = 0; i < secremain; i++)
			{
				save_buffer[i + secoff] = write_buf[i];	 // add new data
			}
			write_one_sector_data(secpos * 4096, save_buffer, 4096); // write sector
		}
    else
    {// no need erase sector
        write_one_sector_data(addr, write_buf, secremain);
    }
		if(len == secremain)
    {// write done
        break;
    }
		else
		{// continue write
			secpos ++;  // sector number + 1
			secoff = 0; // sector offset = 0

      write_buf += secremain;  // write buff offset
      addr += secremain;       // addr offset
      len -= secremain;		 // remaining data len
			if(len > 4096)
      {// remaining data more than one sector(4096 byte)
          secremain = 4096;
      }
			else
      {// remaining data less than one sector(4096 byte)
          secremain = len;
      }
		}
	}
}

/* Erase sector */
void SFLib::sector_erase(uint32_t addr24)
{
    addr24 *= 4096;
    check_busy("sector_erase");
    write_enable();

    digitalWrite(_cs_pin, LOW);
    write_byte(SECTOR_ERASE_CMD);
    write_byte((uint8_t)(addr24 >> 16));
    write_byte((uint8_t)(addr24 >> 8));
    write_byte((uint8_t)addr24);

    digitalWrite(_cs_pin, HIGH);
    check_busy("sector_erase");
}
/* Erase all */
void SFLib::bulk_erase()
{
    check_busy("bulk_erase");
    write_enable();

    digitalWrite(_cs_pin, LOW);
    write_byte(CHIP_ERASE_CMD);
    digitalWrite(_cs_pin, HIGH);
    check_busy("bulk_erase");
}

/* Read norflash id
//0XEF13,表示芯片型号为W25Q80
//0XEF14,表示芯片型号为W25Q16
//0XEF15,表示芯片型号为W25Q32
//0XEF16,表示芯片型号为W25Q64
//0XEF17,表示芯片型号为W25Q128
*/
uint16_t SFLib::read_id()
{
    uint16_t data = 0;
    uint16_t device_id = 0;

    digitalWrite(_cs_pin, LOW);
    write_byte(ManufactDeviceID_CMD);
    write_byte(0x00);
    write_byte(0x00);
    write_byte(0x00);
    data = read_byte(0);
    device_id |= (data << 8);  //high byte
    data = read_byte(0);
    device_id |= data;  // low byte
    digitalWrite(_cs_pin, HIGH);

    return device_id;
}

uint16_t SFLib::read_chipid()
{
    uint32_t Temp = 0, Temp0 = 0, Temp1 = 0, Temp2 = 0;
    digitalWrite(_cs_pin, LOW);
    write_byte(READ_JEDEC_ID_CMD);
    Temp0 = read_byte(0x00);
    Temp1 = read_byte(0x00);
    Temp2 = read_byte(0x00);
    digitalWrite(_cs_pin, HIGH);
    Temp = (Temp0 << 16) | (Temp1 << 8) | Temp2;
    return Temp;
}

/* Norflash write enable */
void SFLib::write_enable()
{
    digitalWrite(_cs_pin, LOW);
    write_byte(WRITE_ENABLE_CMD);
    digitalWrite(_cs_pin, HIGH);
}

/* Norflash write disable */
void SFLib::write_disable()
{
    digitalWrite(_cs_pin, LOW);
    write_byte(WRITE_DISABLE_CMD);
    digitalWrite(_cs_pin, HIGH);
}

/* Read norflash status */
uint8_t SFLib::read_status()
{
    uint8_t status = 0;
    digitalWrite(_cs_pin, LOW);
    write_byte(READ_STATU_REGISTER_1);
    status = read_byte(0);
    digitalWrite(_cs_pin, HIGH);
    return status;
}

/* Write norflash status */
void SFLib::write_status(uint8_t status)
{
    digitalWrite(_cs_pin, LOW);
    write_byte(WRITE_STATUS);
    write_byte(status);
    digitalWrite(_cs_pin, HIGH);
}

/* check norflash busy status */
void SFLib::check_busy(String str)
{
    while(read_status() & 0x01)
    {
    #ifdef NORFLASH_DEBUG_ENABLE
        Serial.printf("status = 0, flash is busy of %s\n", str);
    #endif
    }
}
/* wakeup norflash*/
void SFLib::wakeup(void)
{
    digitalWrite(_cs_pin, LOW);
    write_byte(RELEASE_POWERDOWN_CMD);
    digitalWrite(_cs_pin, HIGH);
}
