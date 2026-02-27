/*!
 * @file      payload_parser.c
 *
 * @brief     Binary frame v1 decoder and validator implementation.
 *            See payload_parser.h for the public API contract.
 *
 * Validation limits mirror specs/protocol/payload.schema.json exactly.
 * When the schema changes, update both files and bump test vectors.
 */

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include "payload_parser.h"
#include "smtc_hal_dbg_trace.h"

#include <string.h>

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS — must match specs/protocol/payload.schema.json -------
 */

#define SCHEMA_NODE_MIN    1u
#define SCHEMA_NODE_MAX    255u
#define SCHEMA_TIME_MIN    1700000000ul
#define SCHEMA_SEQ_MIN     0u
#define SCHEMA_SEQ_MAX     2u
#define SCHEMA_SM_PCT_MIN  0
#define SCHEMA_SM_PCT_MAX  100
#define SCHEMA_ST_C_MIN    ( -55.0f )
#define SCHEMA_ST_C_MAX    ( 125.0f )
#define SCHEMA_AT_C_MIN    ( -40.0f )
#define SCHEMA_AT_C_MAX    ( 85.0f )
#define SCHEMA_AH_PCT_MIN  ( 0.0f )
#define SCHEMA_AH_PCT_MAX  ( 100.0f )
#define SCHEMA_VB_V_MIN    ( 0.0f )
#define SCHEMA_VB_V_MAX    ( 20.0f )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE HELPERS ---------------------------------------------------------
 */

/*!
 * @brief Print a float field as "label = ±INT.FRAC[missing]" without %f.
 *
 * Avoids dependency on -u _printf_float linker flag; safe on targets where
 * the default Newlib printf does not support floating-point conversion.
 */
static void print_float_field( const char* label, float val, bool missing )
{
    int   sign     = ( val < 0.0f ) ? -1 : 1;
    float abs_val  = ( sign < 0 ) ? -val : val;
    int   int_part = ( int ) abs_val;
    int   frac     = ( int ) ( ( abs_val - ( float ) int_part ) * 100.0f + 0.5f );

    if( frac >= 100 )
    {
        int_part++;
        frac = 0;
    }

    HAL_DBG_TRACE_INFO( "  %-8s = %s%d.%02d%s\r\n",
                        label,
                        ( sign < 0 ) ? "-" : "",
                        int_part,
                        frac,
                        missing ? " [MISSING]" : "" );
}

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

bool payload_decode( const uint8_t* buf, uint8_t len, payload_fields_t* fields )
{
    if( buf == NULL || fields == NULL ) return false;
    if( len != FRAME_SIZE )            return false;
    if( buf[0] != FRAME_VER_CURRENT )  return false;

    fields->node   = ( uint32_t ) buf[1];
    fields->seq    = ( uint32_t ) buf[2];

    uint32_t t;
    memcpy( &t, &buf[3], 4 );
    fields->time = t;

    fields->sm_pct = ( int32_t ) buf[7];

    int16_t st_raw;
    memcpy( &st_raw, &buf[8], 2 );
    fields->st_c = st_raw / 10.0f;

    int16_t at_raw;
    memcpy( &at_raw, &buf[10], 2 );
    fields->at_c = at_raw / 10.0f;

    uint16_t ah_raw;
    memcpy( &ah_raw, &buf[12], 2 );
    fields->ah_pct = ah_raw / 10.0f;

    uint16_t vb_raw;
    memcpy( &vb_raw, &buf[14], 2 );
    fields->vb_v = vb_raw / 100.0f;

    return true;
}

bool payload_validate( const payload_fields_t* fields )
{
    bool ok = true;

    /* NODE: integer 1..255 */
    if( fields->node < SCHEMA_NODE_MIN || fields->node > SCHEMA_NODE_MAX )
    {
        HAL_DBG_TRACE_WARNING( "  INVALID NODE=%lu  expected [%u..%u]\r\n",
                               ( unsigned long ) fields->node,
                               SCHEMA_NODE_MIN, SCHEMA_NODE_MAX );
        ok = false;
    }

    /* TIME: UTC epoch >= 1 700 000 000 */
    if( fields->time < SCHEMA_TIME_MIN )
    {
        HAL_DBG_TRACE_WARNING( "  INVALID TIME=%lu  expected >= %lu\r\n",
                               ( unsigned long ) fields->time,
                               ( unsigned long ) SCHEMA_TIME_MIN );
        ok = false;
    }

    /* SEQ: 0..2 */
    if( fields->seq > SCHEMA_SEQ_MAX )
    {
        HAL_DBG_TRACE_WARNING( "  INVALID SEQ=%lu  expected [%u..%u]\r\n",
                               ( unsigned long ) fields->seq,
                               SCHEMA_SEQ_MIN, SCHEMA_SEQ_MAX );
        ok = false;
    }

    /* SM_PCT: integer 0..100 */
    if( fields->sm_pct < SCHEMA_SM_PCT_MIN || fields->sm_pct > SCHEMA_SM_PCT_MAX )
    {
        HAL_DBG_TRACE_WARNING( "  INVALID SM_PCT=%ld  expected [%d..%d]\r\n",
                               ( long ) fields->sm_pct,
                               SCHEMA_SM_PCT_MIN, SCHEMA_SM_PCT_MAX );
        ok = false;
    }

    /* ST_C: -55..125 */
    if( fields->st_c < SCHEMA_ST_C_MIN || fields->st_c > SCHEMA_ST_C_MAX )
    {
        HAL_DBG_TRACE_WARNING( "  INVALID ST_C out of range [-55..125]\r\n" );
        ok = false;
    }

    /* AT_C: -40..85 */
    if( fields->at_c < SCHEMA_AT_C_MIN || fields->at_c > SCHEMA_AT_C_MAX )
    {
        HAL_DBG_TRACE_WARNING( "  INVALID AT_C out of range [-40..85]\r\n" );
        ok = false;
    }

    /* AH_PCT: 0..100 */
    if( fields->ah_pct < SCHEMA_AH_PCT_MIN || fields->ah_pct > SCHEMA_AH_PCT_MAX )
    {
        HAL_DBG_TRACE_WARNING( "  INVALID AH_PCT out of range [0..100]\r\n" );
        ok = false;
    }

    /* VB_V: 0..20 */
    if( fields->vb_v < SCHEMA_VB_V_MIN || fields->vb_v > SCHEMA_VB_V_MAX )
    {
        HAL_DBG_TRACE_WARNING( "  INVALID VB_V out of range [0..20]\r\n" );
        ok = false;
    }

    return ok;
}

void payload_print( const payload_fields_t* fields, bool decoded, bool valid )
{
    HAL_DBG_TRACE_INFO( "--- Decoded Payload Fields ---\r\n" );

    HAL_DBG_TRACE_INFO( "  %-8s = %lu%s\r\n",
                        "NODE", ( unsigned long ) fields->node,
                        decoded ? "" : " [MISSING]" );

    HAL_DBG_TRACE_INFO( "  %-8s = %lu%s\r\n",
                        "TIME", ( unsigned long ) fields->time,
                        decoded ? "" : " [MISSING]" );

    HAL_DBG_TRACE_INFO( "  %-8s = %lu%s\r\n",
                        "SEQ", ( unsigned long ) fields->seq,
                        decoded ? "" : " [MISSING]" );

    HAL_DBG_TRACE_INFO( "  %-8s = %ld%s\r\n",
                        "SM_PCT", ( long ) fields->sm_pct,
                        decoded ? "" : " [MISSING]" );

    print_float_field( "ST_C",   fields->st_c,   !decoded );
    print_float_field( "AT_C",   fields->at_c,   !decoded );
    print_float_field( "AH_PCT", fields->ah_pct, !decoded );
    print_float_field( "VB_V",   fields->vb_v,   !decoded );

    if( !decoded )
    {
        HAL_DBG_TRACE_WARNING( "  DECODE FAIL  (wrong length or version mismatch)\r\n" );
    }
    else if( valid )
    {
        HAL_DBG_TRACE_INFO( "  VALIDATION  PASS\r\n" );
    }
    else
    {
        HAL_DBG_TRACE_WARNING( "  VALIDATION  FAIL (field(s) out of schema range)\r\n" );
    }

    HAL_DBG_TRACE_INFO( "----------------------------\r\n" );
}

/*!
 * @brief Print a labelled float value with unit, without %f.
 */
static void print_human_float( const char* label, float val, const char* unit )
{
    int   sign     = ( val < 0.0f ) ? -1 : 1;
    float abs_val  = ( sign < 0 ) ? -val : val;
    int   int_part = ( int ) abs_val;
    int   frac     = ( int ) ( ( abs_val - ( float ) int_part ) * 100.0f + 0.5f );

    if( frac >= 100 )
    {
        int_part++;
        frac = 0;
    }

    HAL_DBG_TRACE_INFO( "  %-20s %s%d.%02d %s\r\n",
                        label,
                        ( sign < 0 ) ? "-" : "",
                        int_part,
                        frac,
                        unit );
}

void payload_print_human( const payload_fields_t* fields, bool decoded, bool valid )
{
    HAL_DBG_TRACE_INFO( "\r\n========== RECEIVED FRAME ==========\r\n" );

    if( !decoded )
    {
        HAL_DBG_TRACE_WARNING( "  DECODE FAILED (wrong length or version)\r\n" );
        HAL_DBG_TRACE_INFO( "====================================\r\n\r\n" );
        return;
    }

    HAL_DBG_TRACE_INFO( "  %-20s %lu\r\n",
                        "Node ID:",           ( unsigned long ) fields->node );
    HAL_DBG_TRACE_INFO( "  %-20s %lu\r\n",
                        "Sequence:",          ( unsigned long ) fields->seq );
    HAL_DBG_TRACE_INFO( "  %-20s %lu\r\n",
                        "Timestamp (UTC):",   ( unsigned long ) fields->time );
    HAL_DBG_TRACE_INFO( "  %-20s %ld%%\r\n",
                        "Soil Moisture:",     ( long ) fields->sm_pct );

    print_human_float( "Soil Temperature:",  fields->st_c,   "C" );
    print_human_float( "Air Temperature:",   fields->at_c,   "C" );
    print_human_float( "Air Humidity:",      fields->ah_pct, "%RH" );
    print_human_float( "Battery Voltage:",   fields->vb_v,   "V" );

    if( valid )
    {
        HAL_DBG_TRACE_INFO( "  %-20s VALID\r\n", "Status:" );
    }
    else
    {
        HAL_DBG_TRACE_WARNING( "  %-20s INVALID (out of range)\r\n", "Status:" );
    }

    HAL_DBG_TRACE_INFO( "====================================\r\n\r\n" );
}

/* --- EOF ------------------------------------------------------------------ */
