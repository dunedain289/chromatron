/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// </license>
 */

#ifndef _HAL_DMA_H
#define _HAL_DMA_H

#define DMA_CH             CH3
#define DMA_CHTRNIF        DMA_CH3TRNIF_bm
#define DMA_CHERRIF        DMA_CH3ERRIF_bm


void dma_v_memcpy( void *dest, const void *src, uint16_t len );
uint16_t dma_u16_crc( uint8_t *ptr, uint16_t len );

#endif
