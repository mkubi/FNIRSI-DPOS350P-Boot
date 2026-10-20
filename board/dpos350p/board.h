#ifndef BOARD_H
#define BOARD_H

/* SPI NOR: Winbond W25Q16JV, 2 MiB, SPI0 */
#define SPI_NOR_SIZE        (2U * 1024U * 1024U)

/* SD-NAND: CSNP1GCR01-B0W, 1 GiB, SDC2 */
/* SDC2 base: 0x01C11000 */

/* GT911 touch controller */
#define GT911_SDA_PORT      GPIO_PORT_G
#define GT911_SDA_PIN       6U
#define GT911_SCL_PORT      GPIO_PORT_G
#define GT911_SCL_PIN       7U
#define GT911_INT_PORT      GPIO_PORT_E
#define GT911_INT_PIN       22U
#define GT911_RST_PORT      GPIO_PORT_E
#define GT911_RST_PIN       24U

/* FEL recovery: hold SPI0 CS (PC2) low during reset */
#define FEL_CS_PORT         GPIO_PORT_C
#define FEL_CS_PIN          2U

#endif /* BOARD_H */
