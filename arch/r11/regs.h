#ifndef R11_REGS_H
#define R11_REGS_H
#include <stdint.h>
#include <stddef.h>
#define R11_CCU_BASE             0x01C20000U
#define R11_CCU_BUS_GATE0        (R11_CCU_BASE + 0x0060U)
#define R11_CCU_SPI0_CLK         (R11_CCU_BASE + 0x00A0U)
#define R11_CCU_BUS_RESET0       (R11_CCU_BASE + 0x02C0U)
#define R11_PIO_BASE             0x01C20800U
#define R11_PC_CFG0              (R11_PIO_BASE + 0x0048U)
#define R11_SPI0_BASE            0x01C68000U
#define R11_SPI0_GCR             (R11_SPI0_BASE + 0x0004U)
#define R11_SPI0_TCR             (R11_SPI0_BASE + 0x0008U)
#define R11_SPI0_FIFO_STA        (R11_SPI0_BASE + 0x001CU)
#define R11_SPI0_CCTL            (R11_SPI0_BASE + 0x0024U)
#define R11_SPI0_MBC             (R11_SPI0_BASE + 0x0030U)
#define R11_SPI0_MTC             (R11_SPI0_BASE + 0x0034U)
#define R11_SPI0_BCC             (R11_SPI0_BASE + 0x0038U)
#define R11_SPI0_TXD             (R11_SPI0_BASE + 0x0200U)
#define R11_SPI0_RXD             (R11_SPI0_BASE + 0x0300U)
#define R11_SPI0_GATE_BIT        (1U << 20)
#define R11_SPI0_RESET_BIT       (1U << 20)
#define R11_SPI_GCR_ENABLE       (1U << 0)
#define R11_SPI_GCR_MASTER       (1U << 1)
#define R11_SPI_GCR_SOFT_RESET   (1U << 31)
#define R11_SPI_TCR_XCH          (1U << 31)
static inline void mmio_write32(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }
static inline uint32_t mmio_read32(uintptr_t a) { return *(volatile uint32_t *)a; }
static inline void mmio_write8(uintptr_t a, uint8_t v) { *(volatile uint8_t *)a = v; }
static inline uint8_t mmio_read8(uintptr_t a) { return *(volatile uint8_t *)a; }
#endif
