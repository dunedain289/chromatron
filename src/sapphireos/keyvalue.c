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

#include "system.h"
#include "config.h"
#include "types.h"
#include "fs.h"
#include "sockets.h"
#include "timers.h"
#include "hash.h"
#include "ffs_fw.h"
#include "crc.h"
#include "kvdb.h"

// #define NO_LOGGING
#include "logging.h"

#include "keyvalue.h"


static uint32_t kv_persist_writes;
static int32_t kv_test_key;

static int16_t cached_index = -1;
static catbus_hash_t32 cached_hash;

static const PROGMEM char kv_data_fname[] = "kv_data";

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_META_START
#else
    #define KV_SECTION_META_START       __attribute__ ((section (".kv_meta_start"), used))
#endif

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_META_END
#else
    #define KV_SECTION_META_END         __attribute__ ((section (".kv_meta_end"), used))
#endif

KV_SECTION_META_START kv_meta_t kv_start[] = {
    { SAPPHIRE_TYPE_NONE, 0, 0, 0, 0, "kvstart" }
};

KV_SECTION_META_END kv_meta_t kv_end[] = {
    { SAPPHIRE_TYPE_NONE, 0, 0, 0, 0, "kvend" }
};


static int8_t _kv_i8_dynamic_count_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){

        if( hash == __KV__kv_dynamic_count ){
            
            *(uint16_t *)data = kvdb_u16_count();
        }
        else if( hash == __KV__kv_dynamic_db_size ){
            
            *(uint16_t *)data = kvdb_u16_db_size();
        }

        return 0;
    }

    return -1;
}

KV_SECTION_META kv_meta_t kv_cfg[] = {
    { SAPPHIRE_TYPE_UINT32,  0, 0,                   &kv_persist_writes,  0,           "kv_persist_writes" },
    { SAPPHIRE_TYPE_INT32,   0, 0,                   &kv_test_key,        0,           "kv_test_key" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  0, _kv_i8_dynamic_count_handler,  "kv_dynamic_count" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  0, _kv_i8_dynamic_count_handler,  "kv_dynamic_db_size" },
};

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_DYNAMIC_START
#else
    #define KV_SECTION_DYNAMIC_START       __attribute__ ((section (".kv_dynamic_start"), used))
#endif

#if defined(__SIM__) || defined(BOOTLOADER)
    #define KV_SECTION_DYNAMIC_END
#else
    #define KV_SECTION_DYNAMIC_END         __attribute__ ((section (".kv_dynamic_end"), used))
#endif
// KV_SECTION_DYNAMIC_START kv_dynamic_t dynamic_start[] = {{0, 0, 0}};
// KV_SECTION_DYNAMIC_END kv_dynamic_t dynamic_end[] = {{0, 0, 0}};


#ifdef __SIM__
    #define SERVICE_SECTION_START
#else
    #define SERVICE_SECTION_START       __attribute__ ((section (".service_start"), used))
#endif

#ifdef __SIM__
    #define SERVICE_SECTION_END
#else
    #define SERVICE_SECTION_END         __attribute__ ((section (".service_end"), used))
#endif

SERVICE_SECTION_START kv_svc_name_t svc_start = {"sapphire.device"};
SERVICE_SECTION_END char svc_end[1] = "";

static bool persist_fail;
static bool run_persist;

typedef struct{
    catbus_hash_t32 hash;
    sapphire_type_t8 type;
    uint8_t array_len;
    uint8_t reserved[4];
} kv_persist_block_header_t;
#define KV_PERSIST_MAX_DATA_LEN     SAPPHIRE_TYPE_MAX_LEN
#define KV_PERSIST_BLOCK_LEN        ( sizeof(kv_persist_block_header_t) + KV_PERSIST_MAX_DATA_LEN )


PT_THREAD( persist_thread( pt_t *pt, void *state ) );

static int8_t _kv_i8_persist_set_internal(
    file_t f,
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len );

// static uint16_t kv_meta_vfile_handler(
//     vfile_op_t8 op,
//     uint32_t pos,
//     void *ptr,
//     uint16_t len )
// {

//     // the pos and len values are already bounds checked by the FS driver
//     switch( op ){

//         case FS_VFILE_OP_READ:
//             memcpy_P( ptr, (void *)kv_start + sizeof(kv_meta_t) + pos, len );
//             break;

//         case FS_VFILE_OP_SIZE:
//             len = ( (void *)kv_end - (void *)kv_start ) - sizeof(kv_meta_t);
//             break;

//         case FS_VFILE_OP_DELETE:
//             break;

//         default:
//             len = 0;
//             break;
//     }

//     return len;
// }

static int8_t _kv_i8_dynamic_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){

        return kvdb_i8_get( hash, data );
    }
    else if( op == KV_OP_SET ){

        return kvdb_i8_set( hash, *(int32_t *)data );   
    }

    return -1;
}

static uint16_t _kv_u16_fixed_count( void ){

    return ( kv_end - kv_start ) - 1;
}

uint16_t kv_u16_count( void ){

    uint16_t count = _kv_u16_fixed_count();
    
    count += kv_u8_get_dynamic_count();

    return count;
}

int16_t kv_i16_search_hash( catbus_hash_t32 hash ){

    // check if hash exists
    if( hash == 0 ){

        return -1;
    }

    // check cache
    if( cached_hash == hash ){

        return cached_index;
    }

    // get address of hash index
    uint32_t kv_index_start = ( ffs_fw_u32_read_internal_length() - sizeof(uint16_t) ) -
                              ( (uint32_t)_kv_u16_fixed_count() * sizeof(kv_hash_index_t) );

    int16_t first = 0;
    int16_t last = _kv_u16_fixed_count() - 1;
    int16_t middle = ( first + last ) / 2;

    // binary search through hash index
    while( first <= last ){

        kv_hash_index_t index_entry;
        memcpy_PF( &index_entry, kv_index_start + ( middle * sizeof(kv_hash_index_t) ), sizeof(index_entry) );

        if( index_entry.hash < hash ){

            first = middle + 1;
        }
        else if( index_entry.hash == hash ){

            // update cache
            cached_hash = hash;
            cached_index = index_entry.index;

            return index_entry.index;
        }
        else{

            last = middle - 1;
        }

        middle = ( first + last ) / 2;
    }

    // try lookup by hash    
    int16_t index = kvdb_i16_get_index_for_hash( hash );

    if( index >= 0 ){

        return _kv_u16_fixed_count() + index;
    }

    return KV_ERR_STATUS_NOT_FOUND;
}


uint32_t kv_u32_get_hash_from_index( uint16_t index ){

    uint32_t kv_index_start = ( ffs_fw_u32_read_internal_length() - sizeof(uint16_t) ) -
                              ( (uint32_t)_kv_u16_fixed_count() * sizeof(kv_hash_index_t) );

    kv_hash_index_t index_entry;

    for( uint16_t i = 0; i < _kv_u16_fixed_count(); i++ ){
    
        memcpy_PF( &index_entry, kv_index_start + ( (uint32_t)i * sizeof(kv_hash_index_t) ), sizeof(index_entry) );    

        if( index_entry.index == index ){

            return index_entry.hash;
        }
    }

    return 0;
}

int8_t kv_i8_lookup_index( uint16_t index, kv_meta_t *meta, uint8_t flags )
{

    if( index < _kv_u16_fixed_count() ){

        kv_meta_t *ptr = (kv_meta_t *)( kv_start + 1 ) + index;

        // load meta data
        memcpy_P( meta, ptr, sizeof(kv_meta_t) );
    }
    else if( index < kv_u16_count() ){

        // compute dynamic index
        index -= _kv_u16_fixed_count();

        memset( meta, 0, sizeof(kv_meta_t) );

        catbus_meta_t catbus_meta;

        catbus_hash_t32 hash = kvdb_h_get_hash_for_index( index );

        if( hash == 0 ){

            return KV_ERR_STATUS_NOT_FOUND;            
        }

        if( kvdb_i8_get_meta( hash, &catbus_meta ) < 0 ){

            return KV_ERR_STATUS_NOT_FOUND;             
        }

        if( flags & KV_META_FLAGS_GET_NAME ){

            kvdb_i8_lookup_name( hash, meta->name );
        }

        // attach handler
        meta->handler   = _kv_i8_dynamic_handler;
        meta->type      = catbus_meta.type;
        meta->flags     = catbus_meta.flags;
        meta->array_len = catbus_meta.count;
    }
    else{

        return KV_ERR_STATUS_NOT_FOUND;
    }

    return 0;
}

uint16_t kv_u16_get_size_meta( kv_meta_t *meta ){

    uint16_t type_len = type_u16_size( meta->type );

    if( type_len == CATBUS_TYPE_INVALID ){

        return CATBUS_TYPE_INVALID;
    }

    type_len *= ( (uint16_t)meta->array_len + 1 );

    return type_len;
}   

int8_t kv_i8_lookup_hash(
    catbus_hash_t32 hash,
    kv_meta_t *meta,
    uint8_t flags )
{
    int16_t index = kv_i16_search_hash( hash );

    if( index < 0 ){

        return KV_ERR_STATUS_NOT_FOUND;
    }

    return kv_i8_lookup_index( index, meta, flags );
}

int8_t kv_i8_get_name( catbus_hash_t32 hash, char name[KV_NAME_LEN] ){

    kv_meta_t meta;

    memset( name, 0, KV_NAME_LEN );

    int8_t status = kv_i8_lookup_hash( hash, &meta, KV_META_FLAGS_GET_NAME );

    if( status == 0 ){

        strlcpy( name, meta.name, KV_NAME_LEN );
    }
    else{

        // try dynamic DB
        status = kvdb_i8_lookup_name( hash, name );
    }

    return status;
}


static int8_t _kv_i8_init_persist( void ){

    bool file_retry = TRUE;

retry:;

    file_t f = fs_f_open_P( kv_data_fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return -1;
    }

    kv_persist_file_header_t header;

    // check if file was just created
    if( fs_i32_get_size( f ) == 0 ){

        header.magic = KV_PERSIST_MAGIC;
        header.version = KV_PERSIST_VERSION;

        fs_i16_write( f, &header, sizeof(header) );
        fs_v_seek( f, 0 );
    }


    memset( &header, 0, sizeof(header) );

    // read header
    fs_i16_read( f, &header, sizeof(header) );

    // check file header
    if( ( header.magic != KV_PERSIST_MAGIC ) ||
        ( header.version != KV_PERSIST_VERSION ) ){

        fs_v_delete( f );
        fs_f_close( f );

        if( file_retry ){

            file_retry = FALSE;
            goto retry;
        }
    }

    uint8_t buf[KV_PERSIST_BLOCK_LEN];
    kv_persist_block_header_t *hdr = (kv_persist_block_header_t *)buf;
    kv_meta_t meta;

    while( fs_i16_read( f, buf, sizeof(buf) ) == sizeof(buf) ){

        // look up meta data, verify type matches, and check if there is
        // a memory pointer
        int8_t status = kv_i8_lookup_hash( hdr->hash, &meta, 0 );

        if( ( status >= 0 ) &&
            ( meta.type == hdr->type ) &&
            ( meta.ptr != 0 ) ){

            uint16_t type_size = kv_u16_get_size_meta( &meta );

            if( type_size > KV_PERSIST_MAX_DATA_LEN ){

                type_size = KV_PERSIST_MAX_DATA_LEN;
            }

            memcpy( meta.ptr, buf + sizeof(kv_persist_block_header_t), type_size );
        }
    }

    fs_f_close( f );

    return 0;
}


void kv_v_init( void ){

    // fs_f_create_virtual( PSTR("kvmeta"), kv_meta_vfile_handler );

    // check if safe mode
    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        // initialize all persisted KV items
        int8_t status = _kv_i8_init_persist();

        if( status == 0 ){

            persist_fail = FALSE;

            thread_t_create( persist_thread,
                         PSTR("kv_persist"),
                         0,
                         0 );
        }
        else{

            persist_fail = TRUE;
        }
    }
}



static int8_t _kv_i8_persist_set_internal(
    file_t f,
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len )
{

    if( persist_fail ){

        return 0;
    }

    uint8_t buf[KV_PERSIST_MAX_DATA_LEN];

    kv_persist_block_header_t hdr;

    fs_v_seek( f, sizeof(kv_persist_file_header_t) );

    // seek to matching item or end of file
    while( fs_i16_read( f, &hdr, sizeof(hdr) ) == sizeof(hdr) ){

        // check for match
        if( hdr.hash == hash ){

            // remember current position
            int32_t pos = fs_i32_tell( f );

            // found our match
            // copy data into buffer
            fs_i16_read( f, buf, len );

            // compare file data against the data we've been given
            if( memcmp( buf, data, len ) == 0 ){

                // data has not changed, so there's no reason to write anything

                goto end;
            }

            // back up the file position to before header
            fs_v_seek( f, pos - sizeof(hdr) );

            break;
        }

        // advance position to next entry
        fs_v_seek( f, fs_i32_tell( f ) + KV_PERSIST_MAX_DATA_LEN );
    }

    // set up header
    hdr.hash        = hash;
    hdr.type        = meta->type;
    hdr.array_len   = meta->array_len;
    memset( hdr.reserved, 0, sizeof(hdr.reserved) );

    // write header
    fs_i16_write( f, &hdr, sizeof(hdr) );

    // bounds check data
    // not needed, we assert above
    // if( len > KV_PERSIST_BLOCK_DATA_LEN ){

    //     len = KV_PERSIST_BLOCK_DATA_LEN;
    // }

    // Copy data to 0 initialized buffer of the full data length.
    // Even if the value being persisted is smaller than this, we
    // write the full block so that the file always has an even
    // number of full blocks.  Otherwise, the (somewhat naive) scanning
    // algorithm will miss the last block.
    memset( buf + len, 0, sizeof(buf) - len );

    // copy data into buffer
    memcpy( buf, data, len );

    // write data
    // we write the entire buffer length so the entry size is always
    // consistent.
    fs_i16_write( f, buf, sizeof(buf) );

    kv_persist_writes++;

end:
    return 0;
}


static int8_t _kv_i8_persist_set(
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len )
{

    if( persist_fail ){

        return 0;
    }

    ASSERT( len <= KV_PERSIST_MAX_DATA_LEN );

    file_t f = fs_f_open_P( kv_data_fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return -1;
    }

    int8_t status = _kv_i8_persist_set_internal( f, meta, hash, data, len );

    fs_f_close( f );

    return status;
}

static int8_t _kv_i8_internal_set(
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len )
{

    // check flags for readonly
    if( meta->flags & KV_FLAGS_READ_ONLY ){

        return KV_ERR_STATUS_READONLY;
    }

    // set copy length
    uint16_t copy_len = kv_u16_get_size_meta( meta );

    if( copy_len > len ){

        copy_len = len;
    }

    // check if persist flag is set
    if( meta->flags & KV_FLAGS_PERSIST ){

        // check if we *don't* have a RAM pointer
        if( meta->ptr == 0 ){

            _kv_i8_persist_set( meta, hash, data, copy_len );
        }
        else{

            // signal thread to persist in background
            run_persist = TRUE;
        }
    }

    // check if parameter has a pointer
    if( meta->ptr != 0 ){

        ATOMIC;

        // set data
        memcpy( meta->ptr, data, copy_len );

        END_ATOMIC;
    }

    // check if parameter has a notifier
    if( meta->handler == 0 ){

        return KV_ERR_STATUS_OK;
    }

    ATOMIC;

    // call handler
    int8_t status = meta->handler( KV_OP_SET, hash, (void *)data, copy_len );

    END_ATOMIC;

    return status;
}

int8_t kv_i8_set_by_hash(
    catbus_hash_t32 hash,
    const void *data,
    uint16_t len )
{

    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta, 0 );

    if( status < 0 ){

        return status;
    }

    return _kv_i8_internal_set( &meta, hash, data, len );
}


static int8_t _kv_i8_persist_get(
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    file_t f = fs_f_open_P( kv_data_fname, FS_MODE_READ_ONLY );

    if( f < 0 ){

        return -1;
    }

    fs_v_seek( f, sizeof(kv_persist_file_header_t) );

    kv_persist_block_header_t hdr;
    memset( &hdr, 0, sizeof(hdr) ); // init to all 0s in case the file is empty

    // seek to matching item or end of file
    while( fs_i16_read( f, &hdr, sizeof(hdr) ) == sizeof(hdr) ){

        if( hdr.hash == hash ){

            break;
        }

        // advance position to next entry
        fs_v_seek( f, fs_i32_tell( f ) + KV_PERSIST_MAX_DATA_LEN );
    }

    uint16_t data_read = 0;

    // check if data was found
    if( hdr.hash == hash ){

        data_read = fs_i16_read( f, data, len );
    }

    fs_f_close( f );

    // check if correct amount of data was read.
    // data_read will be 0 if the item was not found in the file.
    if( data_read != len ){

        return -1;
    }

    return 0;
}

static int8_t _kv_i8_internal_get(
    kv_meta_t *meta,
    catbus_hash_t32 hash,
    void *data,
    uint16_t max_len )
{


    // set copy length
    uint16_t copy_len = kv_u16_get_size_meta( meta );

    if( copy_len > max_len ){

        copy_len = max_len;
    }

    // check if parameter has a pointer
    if( meta->ptr != 0 ){

        // atomic because interrupts may access RAM data
        ATOMIC;

        // get data
        memcpy( data, meta->ptr, copy_len );

        END_ATOMIC;
    }
    // didn't have a ram pointer:
    // check if persist flag is set, if it is,
    // we'll try to get data from the file.
    else if( meta->flags & KV_FLAGS_PERSIST ){

        // check data from file system
        if( _kv_i8_persist_get( meta, hash, data, max_len ) < 0 ){

            // did not return any data, set default
            memset( data, 0, max_len );
        }
    }

    // check if parameter has a notifier
    if( meta->handler == 0 ){

        return KV_ERR_STATUS_OK;
    }

    ATOMIC;

    // call handler
    int8_t status = meta->handler( KV_OP_GET, hash, data, copy_len );

    END_ATOMIC;

    return status;
}

int8_t kv_i8_get_by_hash(
    catbus_hash_t32 hash,
    void *data,
    uint16_t max_len )
{

    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta, 0 );

    if( status < 0 ){

        return status;
    }

    return _kv_i8_internal_get( &meta, hash, data, max_len );
}


int16_t kv_i16_len( catbus_hash_t32 hash )
{
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta, 0 );

    if( status < 0 ){

        return status;
    }

    return kv_u16_get_size_meta( &meta );
}

sapphire_type_t8 kv_i8_type( catbus_hash_t32 hash )
{

    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta, 0 );

    if( status < 0 ){

        return status;
    }

    return meta.type;
}

int8_t kv_i8_persist( catbus_hash_t32 hash )
{
    // look up parameter
    kv_meta_t meta;

    int8_t status = kv_i8_lookup_hash( hash, &meta, 0 );

    if( status < 0 ){

        // parameter not found
        // log_v_error_P( PSTR("KV param not found") );

        return status;
    }

    // check for persist flag
    if( ( meta.flags & KV_FLAGS_PERSIST ) == 0 ){

        // log_v_error_P( PSTR("Persist flag not set!") );

        return -1;
    }

    // get parameter data
    uint8_t data[KV_PERSIST_MAX_DATA_LEN];
    _kv_i8_internal_get( &meta, hash, data, sizeof(data) );

    // get parameter length
    uint16_t param_len = kv_u16_get_size_meta( &meta );

    return _kv_i8_persist_set( &meta, hash, data, param_len );
}



PT_THREAD( persist_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static kv_meta_t *ptr;
    static file_t f;

    while(1){

        THREAD_WAIT_WHILE( pt, run_persist == FALSE );

        run_persist = FALSE;

        f = fs_f_open_P( kv_data_fname, FS_MODE_WRITE_OVERWRITE );

        if( f < 0 ){

            goto end;
        }

        kv_meta_t meta;
        ptr = (kv_meta_t *)kv_start;

        // iterate through handlers
        while( ptr < kv_end ){

            // load meta data
            memcpy_P( &meta, ptr, sizeof(kv_meta_t) );

            // check flags - and that there is a RAM pointer
            if( ( ( meta.flags & KV_FLAGS_PERSIST ) != 0 ) &&
                  ( meta.ptr != 0 ) ){

                uint16_t param_len = kv_u16_get_size_meta( &meta );

                uint32_t hash = hash_u32_string( meta.name );
                _kv_i8_persist_set_internal( f, &meta, hash, meta.ptr, param_len );
            }   

            ptr++;

            TMR_WAIT( pt, 20 );
        }

        f = fs_f_close( f );

end:
        // prevent back to back updates from swamping the system
        TMR_WAIT( pt, 2000 );
    }

PT_END( pt );
}


int8_t kv_i8_publish( catbus_hash_t32 hash ){
    
    return catbus_i8_publish( hash );
}


uint8_t kv_u8_get_dynamic_count( void ){

    return kvdb_u16_count();
}
