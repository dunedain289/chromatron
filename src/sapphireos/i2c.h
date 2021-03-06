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


#ifndef __I2C_H__
#define __I2C_H__


typedef uint8_t i2c_baud_t8;
#define I2C_BAUD_100K       0
#define I2C_BAUD_400K       1


void i2c_v_init( i2c_baud_t8 baud );
void i2c_v_set_pins( uint8_t clock, uint8_t data );
uint8_t i2c_u8_status( void );
void i2c_v_start( void );
void i2c_v_stop( void );
void i2c_v_write( uint8_t address, const uint8_t *src, uint8_t len );
void i2c_v_read( uint8_t address, uint8_t *dst, uint8_t len );

void i2c_v_write_reg8( uint8_t address, uint8_t reg_addr, uint8_t data );
uint8_t i2c_u8_read_reg8( uint8_t address, uint8_t reg_addr );

#endif
