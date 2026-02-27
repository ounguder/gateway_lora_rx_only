/*!
 * @file      payload_parser.h
 *
 * @brief     Binary frame v1 decoder and validator for the IoT
 *            Soil & Ambient Monitoring system (FSD.md §6).
 *
 * Canonical payload format (FSD §6.1): 16-byte little-endian binary frame v1.
 * Frame layout (offsets):
 *   0x00 VER(1) 0x01 NODE(1) 0x02 SEQ(1) 0x03 TIME(4LE)
 *   0x07 SM_PCT(1) 0x08 ST_C(2LE) 0x0A AT_C(2LE) 0x0C AH_PCT(2LE) 0x0E VB_V(2LE)
 *
 * Field constraints are taken verbatim from specs/protocol/payload.schema.json.
 * Update both files together whenever the protocol contract changes (FSD §9).
 */

#ifndef PAYLOAD_PARSER_H
#define PAYLOAD_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdint.h>
#include <stdbool.h>

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*! Exact byte length of a valid binary frame v1 (FSD §6.1). */
#define FRAME_SIZE         16u

/*! Protocol version byte value for this frame format. */
#define FRAME_VER_CURRENT  1u

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*!
 * @brief Decoded fields from one binary frame v1.
 *
 * Zero-initialise before calling payload_decode():  payload_fields_t f = {0}.
 * All fields are valid only when payload_decode() returns true.
 */
typedef struct
{
    uint32_t node;   /*!< NODE: integer, deployment range 1..3, schema allows 1..255 */
    uint32_t time;   /*!< TIME: UTC epoch seconds, must be >= 1 700 000 000          */
    uint32_t seq;    /*!< SEQ:  burst sequence index 0..2                            */
    int32_t  sm_pct; /*!< SM_PCT: soil moisture 0..100 %                            */
    float    st_c;   /*!< ST_C:  soil temperature -55..125 degC                     */
    float    at_c;   /*!< AT_C:  ambient temperature -40..85 degC                   */
    float    ah_pct; /*!< AH_PCT: ambient humidity 0..100 %RH                       */
    float    vb_v;   /*!< VB_V:  battery voltage 0..20 V                            */
} payload_fields_t;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

/*!
 * @brief Decode a binary frame v1 received over LoRa.
 *
 * @param[in]  buf    Raw bytes from the radio.
 * @param[in]  len    Number of valid bytes in buf.
 * @param[out] fields Destination struct; caller must zero-initialise before call.
 *
 * @return true  if len == FRAME_SIZE and buf[0] == FRAME_VER_CURRENT (all fields decoded).
 *         false if buf or fields is NULL, len != FRAME_SIZE, or version mismatch.
 *
 * @note Multi-byte fields are read with memcpy to avoid UB from unaligned pointer
 *       casts on STM32 Cortex-M4. Safe to call from an IRQ context; no heap allocation.
 */
bool payload_decode( const uint8_t* buf, uint8_t len, payload_fields_t* fields );

/*!
 * @brief Validate decoded fields against the canonical schema ranges (FSD §6.2).
 *
 * @param[in] fields Pointer to a fully-populated payload_fields_t.
 *
 * @return true if every field is within its schema-defined range, false otherwise.
 *         Out-of-range fields are individually reported via HAL_DBG_TRACE_WARNING.
 *
 * @note Call this only when payload_decode() returned true.
 */
bool payload_validate( const payload_fields_t* fields );

/*!
 * @brief Print decoded fields and overall decode/validation status to the UART log.
 *
 * @param[in] fields   Pointer to the decoded payload struct.
 * @param[in] decoded  Return value of payload_decode() — true means all fields present.
 * @param[in] valid    Return value of payload_validate() (ignored when decoded is false).
 */
void payload_print( const payload_fields_t* fields, bool decoded, bool valid );

/*!
 * @brief Print decoded fields with full human-readable parameter names and units.
 *
 * Produces clearly labelled multi-line output such as:
 *   Node ID:            1
 *   Soil Temperature:   18.75 C
 *
 * @param[in] fields   Pointer to the decoded payload struct.
 * @param[in] decoded  Return value of payload_decode().
 * @param[in] valid    Return value of payload_validate() (ignored when decoded is false).
 */
void payload_print_human( const payload_fields_t* fields, bool decoded, bool valid );

#ifdef __cplusplus
}
#endif

#endif /* PAYLOAD_PARSER_H */

/* --- EOF ------------------------------------------------------------------ */
