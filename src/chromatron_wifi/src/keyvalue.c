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

/*

This is just an API wrapper for vm_core and catbus_packer, among others.

*/

#include "keyvalue.h"
#include "kvdb.h"

void kv_v_reset_cache( void ){

}

int8_t kv_i8_array_set(
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    const void *data,
    uint16_t len )
{
    return kvdb_i8_array_set( hash, CATBUS_TYPE_NONE, index, data, len );
}

int8_t kv_i8_array_get(
    catbus_hash_t32 hash,
    uint16_t index,
    uint16_t count,
    void *data,
    uint16_t max_len )
{

    return kvdb_i8_array_get( hash, CATBUS_TYPE_NONE, index, data, max_len );
}

int8_t kv_i8_get_meta( catbus_hash_t32 hash, catbus_meta_t *meta )
{

    return kvdb_i8_get_meta( hash, meta );
}

