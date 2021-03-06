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

#include "hal_io.h"

#include "hal_i2c.h"

#include "i2c.h"

/*

This is a bit bang driver, so it can work on any set of pins.

*/

static uint8_t status;

static io_port_t *scl_port = &IO_PIN0_PORT;
static uint8_t scl_pin = ( 1 << IO_PIN0_PIN );
static io_port_t *sda_port = &IO_PIN1_PORT;
static uint8_t sda_pin = ( 1 << IO_PIN1_PIN );


#define SDA_HIGH() ( sda_port->OUTSET = sda_pin )
#define SDA_LOW() ( sda_port->OUTCLR = sda_pin )

#define SCL_HIGH() ( scl_port->OUTSET = scl_pin )
#define SCL_LOW() ( scl_port->OUTCLR = scl_pin )


#define I2C_DELAY() for( uint8_t __i = 0; __i < 8; __i++ ){ asm volatile("nop"); }


static void send_bit( uint8_t b ){

    if( b ){

        SDA_HIGH();
    }
    else{

        SDA_LOW();
    }

    I2C_DELAY();

    SCL_HIGH();

    I2C_DELAY();
    I2C_DELAY();
    I2C_DELAY();   

    SCL_LOW();

    I2C_DELAY();
}


static bool read_bit( void ){

    I2C_DELAY();

    // release bus
    SDA_HIGH();
    SCL_HIGH();

    I2C_DELAY();
    I2C_DELAY();
    I2C_DELAY();
    
    // read bit
    uint8_t b = sda_port->IN & sda_pin;

    SCL_LOW();

    I2C_DELAY();

    return b != 0;
}

static uint8_t send_byte( uint8_t b ){
    
    send_bit( b & 0x80 );
    send_bit( b & 0x40 );
    send_bit( b & 0x20 );
    send_bit( b & 0x10 );
    send_bit( b & 0x08 );
    send_bit( b & 0x04 );
    send_bit( b & 0x02 );
    send_bit( b & 0x01 );

    // ack
    return read_bit();
}


static uint8_t read_byte( bool ack ){

    uint8_t b = 0;

    // for( uint8_t i = 0; i < 8; i++ ){

    //     b <<= 1;

    //     if( read_bit() ){

    //         b |= 1;
    //     }
    // }

    if( read_bit() ){

        b |= 0x80;
    }
    if( read_bit() ){

        b |= 0x40;
    }
    if( read_bit() ){

        b |= 0x20;
    }
    if( read_bit() ){

        b |= 0x10;
    }
    if( read_bit() ){

        b |= 0x08;
    }
    if( read_bit() ){

        b |= 0x04;
    }
    if( read_bit() ){

        b |= 0x02;
    }
    if( read_bit() ){

        b |= 0x01;
    }

    if( ack ){

        send_bit( 0 );
    }
    else{

        send_bit( 1 );
    }

    return b;
}

void i2c_v_init( i2c_baud_t8 baud ){

    i2c_v_set_pins( IO_PIN_1_XCK, IO_PIN_0_GPIO );
}

void i2c_v_set_pins( uint8_t clock, uint8_t data ){

    // set old pins to input
    io_v_set_mode( clock, IO_MODE_INPUT );
    io_v_set_mode( data, IO_MODE_INPUT );

    scl_port = io_p_get_port( clock );
    scl_pin = ( 1 << io_u8_get_pin( clock ) );
    sda_port = io_p_get_port( data );
    sda_pin = ( 1 << io_u8_get_pin( data ) );
    
    io_v_set_mode( clock, IO_MODE_OUTPUT_OPEN_DRAIN );
    io_v_set_mode( data, IO_MODE_OUTPUT_OPEN_DRAIN );

    SDA_HIGH();
    SCL_HIGH();

    I2C_DELAY();
}

uint8_t i2c_u8_status( void ){

    return status;
}

void i2c_v_start( void ){

    SDA_HIGH();
    SCL_HIGH();
    I2C_DELAY();

    SDA_LOW();
    I2C_DELAY();

    SCL_LOW();
    I2C_DELAY();
}

void i2c_v_stop( void ){

    I2C_DELAY();

    SDA_LOW();
    I2C_DELAY();

    SCL_HIGH();
    I2C_DELAY();

    SDA_HIGH();
    I2C_DELAY();
}

void i2c_v_write( uint8_t address, const uint8_t *src, uint8_t len ){

    address <<= 1;
    address &= ~0x01; // set write

    i2c_v_start();

    // send address/read bit
    send_byte( address );

    while( len > 0 ){

        send_byte( *src );

        src++;
        len--;
    }

    i2c_v_stop();
}

void i2c_v_read( uint8_t address, uint8_t *dst, uint8_t len ){

    address <<= 1;
    address |= 0x01; // set read bit

    i2c_v_start();

    // send address/write bit
    send_byte( address );

    while( len > 0 ){

        if( len > 1 ){

            *dst = read_byte( TRUE );
        }
        else{

            *dst = read_byte( FALSE );
        }

        dst++;
        len--;
    }

    i2c_v_stop();
}
