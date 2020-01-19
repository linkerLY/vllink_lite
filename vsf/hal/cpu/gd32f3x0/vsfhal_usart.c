/***************************************************************************
 *   Copyright (C) 2018 - 2019 by Chen Le <talpachen@gmail.com>            *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "vsf.h"
#include "vsfhal_core.h"

#if VSFHAL_USART_EN

// usart + dma + HTFIE + FTFIE + 48byte buffer

#define DMA_BUFF_SIZE			48

static uint8_t tx_dma_buff[VSFHAL_USART_NUM][DMA_BUFF_SIZE];

static void (*vsfhal_usart_ontx[VSFHAL_USART_NUM])(void *);
static void (*vsfhal_usart_onrx[VSFHAL_USART_NUM])(void *, uint16_t rx);
static void *vsfhal_usart_callback_param[VSFHAL_USART_NUM];

#define CONFIG_USART_GPIO(port, pin_idx)		do {	\
GPIO_PUD(port) = (GPIO_PUD(port) & ~(GPIO_PUD_PUD0 << (pin_idx * 2))) | (0x1ul << (pin_idx * 2));	\
} while(0)
//#define CONFIG_USART_GPIO(port, pin_idx)

vsf_err_t vsfhal_usart_init(vsfhal_usart_t index)
{
	if (index >= VSFHAL_USART_NUM)
		return VSFERR_BUG;
	
	switch (index)
	{
#if VSFHAL_USART0_ENABLE
	case 0:
#if VSFHAL_USART0_TXD_PA9_EN
		RCU_AHBEN |= RCU_AHBEN_PAEN;
		IO_AF_SELECT(0, 9, 1);
		IO_CFG_AF(0, 9);
#endif
#if VSFHAL_USART0_RXD_PA10_EN
		RCU_AHBEN |= RCU_AHBEN_PAEN;
		CONFIG_USART_GPIO(GPIOA, 10);
		IO_AF_SELECT(0, 10, 1);
		IO_CFG_AF(0, 10);
#endif
#if VSFHAL_USART0_TXD_PB6_EN
		RCU_AHBEN |= RCU_AHBEN_PBEN;
		IO_AF_SELECT(1, 6, 0);
		IO_CFG_AF(1, 6);
#endif
#if VSFHAL_USART0_RXD_PB7_EN
		RCU_AHBEN |= RCU_AHBEN_PBEN;
		CONFIG_USART_GPIO(GPIOB, 7);
		IO_AF_SELECT(1, 7, 0);
		IO_CFG_AF(1, 7);
#endif
		RCU_AHBEN |= RCU_AHBEN_DMAEN;
		RCU_APB2EN |= RCU_APB2EN_USART0EN;
		break;
#endif
#if VSFHAL_USART1_ENABLE
	case 1:
#if VSFHAL_USART1_TXD_PA2_EN
		RCU_AHBEN |= RCU_AHBEN_PAEN;
		IO_AF_SELECT(0, 2, 1);
		IO_CFG_AF(0, 2);
#endif
#if VSFHAL_USART1_RXD_PA3_EN
		RCU_AHBEN |= RCU_AHBEN_PAEN;
		CONFIG_USART_GPIO(GPIOA, 3);
		IO_AF_SELECT(0, 3, 1);
		IO_CFG_AF(0, 3);
#endif
#if VSFHAL_USART1_TXD_PA8_EN
		RCU_AHBEN |= RCU_AHBEN_PAEN;
		IO_AF_SELECT(0, 8, 4);
		IO_CFG_AF(0, 8);
#endif
#if VSFHAL_USART1_RXD_PB0_EN
		RCU_AHBEN |= RCU_AHBEN_PBEN;
		CONFIG_USART_GPIO(GPIOB, 0);
		IO_AF_SELECT(1, 0, 4);
		IO_CFG_AF(1, 0);
#endif
#if VSFHAL_USART1_TXD_PA14_EN
		RCU_AHBEN |= RCU_AHBEN_PAEN;
		IO_AF_SELECT(0, 14, 1);
		IO_CFG_AF(0, 14);
#endif
#if VSFHAL_USART1_RXD_PA15_EN
		RCU_AHBEN |= RCU_AHBEN_PAEN;
		CONFIG_USART_GPIO(GPIOA, 15);
		IO_AF_SELECT(0, 15, 1);
		IO_CFG_AF(0, 15);
#endif
		RCU_AHBEN |= RCU_AHBEN_DMAEN;
		RCU_APB1EN |= RCU_APB1EN_USART1EN;
		break;
#endif
	}
	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_fini(vsfhal_usart_t index)
{
	if (index >= VSFHAL_USART_NUM)
		return VSFERR_BUG;

	switch (index)
	{
#if VSFHAL_USART0_ENABLE
	case 0:
		RCU_APB2EN &= ~RCU_APB2EN_USART0EN;
		break;
#endif
#if VSFHAL_USART1_ENABLE
	case 1:
		RCU_APB1EN &= ~RCU_APB1EN_USART1EN;
		break;
#endif
	}
	
	return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_config(vsfhal_usart_t index, uint32_t baudrate, uint32_t mode)
{
	uint32_t temp;
	uint32_t usartx;
	struct vsfhal_info_t *info;

	if (index >= VSFHAL_USART_NUM)
		return VSFERR_BUG;

	if (vsfhal_core_get_info(&info) || (NULL == info))
		return VSFERR_BUG;
	
	switch (index)
	{
	case 0:
		usartx = USART0;
		temp = info->apb2_freq_hz;
		break;
	case 1:
		usartx = USART1;
		temp = info->apb1_freq_hz;
		break;
	}
	
	if (!baudrate)
	{
		USART_CTL0(usartx) = 0;
		return VSFERR_NONE;
	}
	
	if (mode & (VSFHAL_USART_PARITY_ODD | VSFHAL_USART_PARITY_EVEN))
		mode |= VSFHAL_USART_BITLEN_9;

	if (index == 0)
	{
		DMA_CH1CTL = 0;
		USART_CTL0(usartx) = 0;
		USART_CTL0(usartx) = USART_CTL0_RTIE | (mode & 0x1600) | USART_CTL0_TEN | USART_CTL0_REN;
		USART_CTL1(usartx) = USART_CTL1_RTEN | ((mode >> 16) & 0x3000);
		USART_CTL2(usartx) = USART_CTL2_DENT;
		USART_RT(usartx) = 30;

		// dma tx
		DMA_CH1CTL = DMA_CHXCTL_MNAGA | DMA_CHXCTL_DIR | DMA_CHXCTL_FTFIE;
		DMA_CH1PADDR = (uint32_t)(usartx + 0x28U);
	}
	else
	{
		DMA_CH3CTL = 0;
		USART_CTL0(usartx) = 0;
		USART_CTL0(usartx) = (mode & 0x1600) | USART_CTL0_TEN | USART_CTL0_REN;
		USART_CTL1(usartx) = (mode >> 16) & 0x3000;
		USART_CTL2(usartx) = USART_CTL2_DENR | USART_CTL2_DENT;
		
		// dma tx
		DMA_CH3CTL = DMA_CHXCTL_MNAGA | DMA_CHXCTL_DIR | DMA_CHXCTL_FTFIE;
		DMA_CH3PADDR = (uint32_t)(usartx + 0x28U);
	}
	USART_BAUD(usartx) = temp / baudrate;
	USART_CMD(usartx) = 0x1f;
	USART_RFCS(usartx) |= USART_RFCS_RFEN | USART_RFCS_RFFIE;
	USART_CTL0(usartx) |= USART_CTL0_UEN;

	return VSFERR_NONE;
}

vsf_err_t vsfhal_usart_config_cb(vsfhal_usart_t index, int32_t int_priority, void *p,
		void (*ontx)(void *), void (*onrx)(void *, uint16_t))
{
	if (index >= VSFHAL_USART_NUM)
		return VSFERR_BUG;

	vsfhal_usart_ontx[index] = ontx;
	vsfhal_usart_onrx[index] = onrx;
	vsfhal_usart_callback_param[index] = p;
	
	if (int_priority >= 0)
	{
		switch (index)
		{
		#if VSFHAL_USART0_ENABLE
		case 0:
			NVIC_EnableIRQ(USART0_IRQn);
			NVIC_SetPriority(USART0_IRQn, int_priority);
			NVIC_EnableIRQ(DMA_Channel1_2_IRQn);
			NVIC_SetPriority(DMA_Channel1_2_IRQn, int_priority);
			break;
		#endif
		#if VSFHAL_USART1_ENABLE
		case 1:
			NVIC_EnableIRQ(USART1_IRQn);
			NVIC_SetPriority(USART1_IRQn, int_priority);
			NVIC_EnableIRQ(DMA_Channel3_4_IRQn);
			NVIC_SetPriority(DMA_Channel3_4_IRQn, int_priority);
			break;
		#endif
		}
	}
	return VSFERR_NONE;
}

uint16_t vsfhal_usart_tx_bytes(vsfhal_usart_t index, uint8_t *data, uint16_t size)
{
	uint8_t buf[DMA_BUFF_SIZE], last;

	if ((index >= VSFHAL_USART_NUM) || !size)
		return 0;

	switch (index)
	{
	case 0:
		DMA_CH1CTL &= ~DMA_CHXCTL_CHEN;
		last = DMA_CH1CNT;
		if (last)
			memcpy(buf, (void *)DMA_CH1MADDR, last);
		size = min(sizeof(buf) - last, size);
		if (size)
			memcpy(buf + last, data, size);
		memcpy(tx_dma_buff[index], buf, size + last);
		DMA_CH1MADDR = (uint32_t)tx_dma_buff[index];
		DMA_CH1CNT = size + last;
		DMA_CH1CTL |= DMA_CHXCTL_CHEN;
		return size;
	case 1:
		DMA_CH3CTL &= ~DMA_CHXCTL_CHEN;
		last = DMA_CH3CNT;
		if (last)
			memcpy(buf, (void *)DMA_CH3MADDR, last);
		size = min(sizeof(buf) - last, size);
		if (size)
			memcpy(buf + last, data, size);
		memcpy(tx_dma_buff[index], buf, size + last);
		DMA_CH3MADDR = (uint32_t)tx_dma_buff[index];
		DMA_CH3CNT = size + last;
		DMA_CH3CTL |= DMA_CHXCTL_CHEN;
		return size;
	default:
		return 0;
	}
}

uint16_t vsfhal_usart_tx_get_data_size(vsfhal_usart_t index)
{
	switch (index)
	{
	case 0:
		if (DMA_CH1CTL & DMA_CHXCTL_CHEN)
			return DMA_CH1CNT;
		else
			return 0;
	case 1:
		if (DMA_CH3CTL & DMA_CHXCTL_CHEN)
			return DMA_CH3CNT;
		else
			return 0;
	default:
		return 0;
	}
}

uint16_t vsfhal_usart_tx_get_free_size(vsfhal_usart_t index)
{
	return DMA_BUFF_SIZE -  vsfhal_usart_tx_get_data_size(index);
}

vsf_err_t vsfhal_usart_tx_int_config(vsfhal_usart_t index, bool enable)
{
	switch (index)
	{
	case 0:
		if (enable)
			DMA_CH1CTL |= DMA_CHXCTL_FTFIE;
		else
			DMA_CH1CTL &= ~DMA_CHXCTL_FTFIE;
		break;
	case 1:
		if (enable)
			DMA_CH3CTL |= DMA_CHXCTL_FTFIE;
		else
			DMA_CH3CTL &= ~DMA_CHXCTL_FTFIE;
		break;
	default:
		return VSFERR_BUG;
	}

	return VSFERR_NONE;
}

uint16_t vsfhal_usart_rx_bytes(vsfhal_usart_t index, uint8_t *data, uint16_t size)
{
	uint16_t i;
	uint32_t usartx;

	switch (index)
	{
	case 0:
		usartx = USART0;
		break;
	case 1:
		usartx = USART1;
		break;
	default:
		return 0;
	}
	
	for (i = 0; i < size; i++)
		data[i] = USART_RDATA(usartx);
	return size;
}

uint16_t vsfhal_usart_rx_get_data_size(vsfhal_usart_t index)
{
	uint16_t size;
	uint32_t usartx;

	switch (index)
	{
	case 0:
		usartx = USART0;
		break;
	case 1:
		usartx = USART1;
		break;
	default:
		return 0;
	}
	
	size = (USART_RFCS(usartx) >> 12) & 0x7;
	if (USART_STAT(usartx) & USART_STAT_RBNE)
		size += 1;
	
	return size;
}

uint16_t vsfhal_usart_rx_get_free_size(vsfhal_usart_t index)
{
	return DMA_BUFF_SIZE - vsfhal_usart_rx_get_data_size(index);
}

vsf_err_t vsfhal_usart_rx_int_config(vsfhal_usart_t index, bool enable)
{
	uint32_t usartx;

	switch (index)
	{
	case 0:
		usartx = USART0;
		break;
	case 1:
		usartx = USART1;
		break;
	default:
		return VSFERR_BUG;
	}

	if (enable)
	{
		USART_CTL0(usartx) &= ~USART_CTL0_RTIE;
		USART_RFCS(usartx) &= ~USART_RFCS_RFFIE;
	}
	else
	{
		USART_CTL0(usartx) |= USART_CTL0_RTIE;
		USART_RFCS(usartx) |= USART_RFCS_RFFIE;
	}
	return VSFERR_NONE;
}

#if VSFHAL_USART0_ENABLE
ROOT void USART0_IRQHandler(void)
{
	if (USART_STAT(USART0) & USART_STAT_RTF)
		USART_INTC(USART0) = USART_INTC_RTC;
	if (USART_STAT(USART0) & USART_STAT_RBNE)
	{
		if (USART_RFCS(USART0) & USART_RFCS_RFFINT)
			USART_RFCS(USART0) &= ~USART_RFCS_RFFINT;
		if (vsfhal_usart_onrx[0])
			vsfhal_usart_onrx[0](vsfhal_usart_callback_param[0], USART_RDATA(USART0));
	}
}

ROOT void DMA_Channel1_2_IRQHandler(void)
{
	DMA_INTC = (DMA_INTC_GIFC << (1 * 4)) | (DMA_INTC_GIFC << (2 * 4));
	DMA_CH1CTL &= ~DMA_CHXCTL_CHEN;
	if (vsfhal_usart_ontx[0])
		vsfhal_usart_ontx[0](vsfhal_usart_callback_param[0]);
}
#endif
#if VSFHAL_USART1_ENABLE
ROOT void USART1_IRQHandler(void)
{
	if (USART_STAT(USART1) & USART_STAT_RBNE)
	{
		if (USART_RFCS(USART1) & USART_RFCS_RFFINT)
			USART_RFCS(USART1) &= ~USART_RFCS_RFFINT;
		if (vsfhal_usart_onrx[1])
			vsfhal_usart_onrx[1](vsfhal_usart_callback_param[1], USART_RDATA(USART0));
	}
}
ROOT void DMA_Channel3_4_IRQHandler(void)
{
	DMA_INTC = (DMA_INTC_GIFC << (3 * 4)) | (DMA_INTC_GIFC << (4 * 4));
	DMA_CH3CTL &= ~DMA_CHXCTL_CHEN;
	if (vsfhal_usart_ontx[1])
		vsfhal_usart_ontx[1](vsfhal_usart_callback_param[1]);
}
void gd32f3x0_usart1_poll(void)
{
	istate_t gint = GET_GLOBAL_INTERRUPT_STATE();
	DISABLE_GLOBAL_INTERRUPT();
	if (USART_STAT(USART1) & USART_STAT_RBNE)
	{
		if (USART_RFCS(USART1) & USART_RFCS_RFFINT)
			USART_RFCS(USART1) &= ~USART_RFCS_RFFINT;
		if (vsfhal_usart_onrx[1])
			vsfhal_usart_onrx[1](vsfhal_usart_callback_param[1], USART_RDATA(USART0));
	}
	SET_GLOBAL_INTERRUPT_STATE(gint);
}
#endif

#endif
