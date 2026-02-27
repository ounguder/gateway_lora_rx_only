/*!
 * @file      main_demo.c
 *
 * @brief     LoRa P2P RX-only receiver — DEMO BUILD (no SIM7000E modem).
 *
 * Stripped copy of main_p2p_receiver.c for demonstration purposes.
 * All modem code removed; only LoRa RX + payload decode + human-readable print.
 */

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "apps_common.h"
#include "apps_utilities.h"
#include "lr11xx_radio.h"
#include "lr11xx_system.h"
#include "lr11xx_system_types.h"
#include "lr11xx_regmem.h"
#include "main_p2p_receiver.h"
#include "payload_parser.h"
#include "smtc_hal_dbg_trace.h"
#include "uart_init.h"
#include "smtc_hal_mcu.h"
#include "smtc_hal_mcu_gpio.h"
#include "smtc_shield_lr11xx.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

#define IRQ_MASK                                                                           \
    ( LR11XX_SYSTEM_IRQ_TX_DONE | LR11XX_SYSTEM_IRQ_RX_DONE | LR11XX_SYSTEM_IRQ_TIMEOUT | \
      LR11XX_SYSTEM_IRQ_HEADER_ERROR | LR11XX_SYSTEM_IRQ_CRC_ERROR | LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define APP_PARTIAL_SLEEP true

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static lr11xx_hal_context_t* context;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

int main( void )
{
    smtc_hal_mcu_init( );
    uart_init( );

    HAL_DBG_TRACE_INFO( "===== LR11xx LoRa P2P Receiver [DEMO] =====\r\n" );
    HAL_DBG_TRACE_INFO( "  Modem: DISABLED (demo build)\r\n" );

    HAL_DBG_TRACE_INFO( "[DBG] shield_init...\r\n" );
    apps_common_shield_init( );
    HAL_DBG_TRACE_INFO( "[DBG] shield_init OK\r\n" );

    apps_common_print_sdk_driver_version( );

    HAL_DBG_TRACE_INFO( "[DBG] get_context...\r\n" );
    context = apps_common_lr11xx_get_context( );
    HAL_DBG_TRACE_INFO( "[DBG] get_context OK\r\n" );

    /* ---- LR1121 hardware diagnostic ---- */
    {
        smtc_hal_mcu_gpio_state_t busy_state;

        HAL_DBG_TRACE_INFO( "[DIAG] Reading BUSY pin (D3) before reset...\r\n" );
        smtc_hal_mcu_gpio_get_state( context->busy.inst, &busy_state );
        HAL_DBG_TRACE_INFO( "[DIAG] BUSY = %s\r\n",
                            ( busy_state == SMTC_HAL_MCU_GPIO_STATE_HIGH ) ? "HIGH" : "LOW" );

        HAL_DBG_TRACE_INFO( "[DIAG] Pulling RESET (A0) LOW...\r\n" );
        smtc_hal_mcu_gpio_set_state( context->reset.inst, SMTC_HAL_MCU_GPIO_STATE_LOW );
        smtc_hal_mcu_wait_ms( 5 );

        HAL_DBG_TRACE_INFO( "[DIAG] Releasing RESET (A0) HIGH...\r\n" );
        smtc_hal_mcu_gpio_set_state( context->reset.inst, SMTC_HAL_MCU_GPIO_STATE_HIGH );
        smtc_hal_mcu_wait_ms( 500 );

        smtc_hal_mcu_gpio_get_state( context->busy.inst, &busy_state );
        HAL_DBG_TRACE_INFO( "[DIAG] BUSY after reset = %s\r\n",
                            ( busy_state == SMTC_HAL_MCU_GPIO_STATE_HIGH ) ? "HIGH (STUCK!)" : "LOW (OK)" );

        if( busy_state == SMTC_HAL_MCU_GPIO_STATE_HIGH )
        {
            HAL_DBG_TRACE_ERROR( "[DIAG] LR1121 BUSY stuck HIGH - check shield connection!\r\n" );
            HAL_DBG_TRACE_ERROR( "[DIAG]   Verify: D3(BUSY) D7(NSS) D11(MOSI) D12(MISO) D13(SCK) A0(RST)\r\n" );
            HAL_DBG_TRACE_ERROR( "[DIAG]   Halting here. Fix hardware and reset.\r\n" );
            while( 1 ) { smtc_hal_mcu_wait_ms( 1000 ); }
        }
    }

    /* ---- Inline system_init with step-by-step debug ---- */
    HAL_DBG_TRACE_INFO( "[DBG] system_init (inline)...\r\n" );
    {
        smtc_hal_mcu_gpio_state_t bs;
        smtc_shield_lr11xx_t* sh = apps_common_lr11xx_get_shield( );

        /* Step 1: reset */
        HAL_DBG_TRACE_INFO( "[INIT] 1/9 lr11xx_system_reset...\r\n" );
        ASSERT_LR11XX_RC( lr11xx_system_reset( ( void* ) context ) );
        HAL_DBG_TRACE_INFO( "[INIT] 1/9 reset returned, waiting 500ms for boot...\r\n" );
        smtc_hal_mcu_wait_ms( 500 );
        smtc_hal_mcu_gpio_get_state( context->busy.inst, &bs );
        HAL_DBG_TRACE_INFO( "[INIT] BUSY = %s\r\n",
                            ( bs == SMTC_HAL_MCU_GPIO_STATE_HIGH ) ? "HIGH" : "LOW" );

        /* Step 2: regulator */
        HAL_DBG_TRACE_INFO( "[INIT] 2/9 set_reg_mode...\r\n" );
        const lr11xx_system_reg_mode_t regulator = smtc_shield_lr11xx_get_reg_mode( sh );
        ASSERT_LR11XX_RC( lr11xx_system_set_reg_mode( ( void* ) context, regulator ) );
        HAL_DBG_TRACE_INFO( "[INIT] 2/9 OK\r\n" );

        /* Step 3: RF switch */
        HAL_DBG_TRACE_INFO( "[INIT] 3/9 set_dio_as_rf_switch...\r\n" );
        const lr11xx_system_rfswitch_cfg_t* rf_sw = smtc_shield_lr11xx_get_rf_switch_cfg( sh );
        ASSERT_LR11XX_RC( lr11xx_system_set_dio_as_rf_switch( context, rf_sw ) );
        HAL_DBG_TRACE_INFO( "[INIT] 3/9 OK\r\n" );

        /* Step 4: TCXO */
        const smtc_shield_lr11xx_xosc_cfg_t* tcxo = smtc_shield_lr11xx_get_xosc_cfg( sh );
        if( tcxo->has_tcxo == true )
        {
            HAL_DBG_TRACE_INFO( "[INIT] 4/9 set_tcxo_mode...\r\n" );
            ASSERT_LR11XX_RC( lr11xx_system_set_tcxo_mode( context, tcxo->supply, tcxo->startup_time_in_tick ) );
            HAL_DBG_TRACE_INFO( "[INIT] 4/9 OK\r\n" );
        }
        else
        {
            HAL_DBG_TRACE_INFO( "[INIT] 4/9 no TCXO — skip\r\n" );
        }

        /* Step 5: LF clock */
        HAL_DBG_TRACE_INFO( "[INIT] 5/9 cfg_lfclk...\r\n" );
        const smtc_shield_lr11xx_lfclk_cfg_t* lf = smtc_shield_lr11xx_get_lfclk_cfg( sh );
        ASSERT_LR11XX_RC( lr11xx_system_cfg_lfclk( context, lf->lf_clk_cfg, lf->wait_32k_ready ) );
        HAL_DBG_TRACE_INFO( "[INIT] 5/9 OK\r\n" );

        /* Step 6: clear errors */
        HAL_DBG_TRACE_INFO( "[INIT] 6/9 clear_errors...\r\n" );
        ASSERT_LR11XX_RC( lr11xx_system_clear_errors( context ) );
        HAL_DBG_TRACE_INFO( "[INIT] 6/9 OK\r\n" );

        /* Step 7: calibrate */
        HAL_DBG_TRACE_INFO( "[INIT] 7/9 calibrate(0x3F)...\r\n" );
        ASSERT_LR11XX_RC( lr11xx_system_calibrate( context, 0x3F ) );
        HAL_DBG_TRACE_INFO( "[INIT] 7/9 cmd sent, polling BUSY (5s timeout)...\r\n" );
        {
            uint32_t poll_ms = 0;
            smtc_hal_mcu_gpio_state_t cal_bs;
            do {
                smtc_hal_mcu_wait_ms( 100 );
                poll_ms += 100;
                smtc_hal_mcu_gpio_get_state( context->busy.inst, &cal_bs );
                if( ( poll_ms % 1000 ) == 0 )
                {
                    HAL_DBG_TRACE_INFO( "[INIT] 7/9 BUSY=%s after %lu ms\r\n",
                                        ( cal_bs == SMTC_HAL_MCU_GPIO_STATE_HIGH ) ? "HIGH" : "LOW",
                                        ( unsigned long ) poll_ms );
                }
            } while( cal_bs == SMTC_HAL_MCU_GPIO_STATE_HIGH && poll_ms < 5000 );

            if( cal_bs == SMTC_HAL_MCU_GPIO_STATE_HIGH )
            {
                HAL_DBG_TRACE_ERROR( "[INIT] Calibration BUSY stuck HIGH after 5s!\r\n" );
                HAL_DBG_TRACE_ERROR( "[INIT] TCXO or PLL issue — trying calibrate(0x01) RC64k only...\r\n" );

                /* Reset and retry with minimal calibration */
                lr11xx_system_reset( ( void* ) context );
                smtc_hal_mcu_wait_ms( 500 );
                lr11xx_system_set_reg_mode( ( void* ) context, regulator );
                lr11xx_system_set_dio_as_rf_switch( context, rf_sw );
                /* Skip TCXO this time */
                lr11xx_system_cfg_lfclk( context, lf->lf_clk_cfg, lf->wait_32k_ready );
                lr11xx_system_clear_errors( context );

                HAL_DBG_TRACE_INFO( "[INIT] Retry calibrate(0x01) — RC64k only...\r\n" );
                lr11xx_system_calibrate( context, 0x01 );
                smtc_hal_mcu_wait_ms( 1000 );
                smtc_hal_mcu_gpio_get_state( context->busy.inst, &cal_bs );
                HAL_DBG_TRACE_INFO( "[INIT] BUSY after RC64k cal = %s\r\n",
                                    ( cal_bs == SMTC_HAL_MCU_GPIO_STATE_HIGH ) ? "HIGH" : "LOW" );
            }
            else
            {
                HAL_DBG_TRACE_INFO( "[INIT] 7/9 calibration done in %lu ms\r\n",
                                    ( unsigned long ) poll_ms );
            }
        }

        /* Step 8: get errors */
        HAL_DBG_TRACE_INFO( "[INIT] 8/9 get_errors...\r\n" );
        uint16_t errors;
        ASSERT_LR11XX_RC( lr11xx_system_get_errors( context, &errors ) );
        HAL_DBG_TRACE_INFO( "[INIT] 8/9 errors=0x%04X\r\n", errors );

        /* Step 9: final clear */
        HAL_DBG_TRACE_INFO( "[INIT] 9/9 clear_errors + clear_irq...\r\n" );
        ASSERT_LR11XX_RC( lr11xx_system_clear_errors( context ) );
        ASSERT_LR11XX_RC( lr11xx_system_clear_irq_status( context, LR11XX_SYSTEM_IRQ_ALL_MASK ) );
        HAL_DBG_TRACE_INFO( "[INIT] 9/9 OK\r\n" );
    }
    HAL_DBG_TRACE_INFO( "[DBG] system_init OK\r\n" );

    HAL_DBG_TRACE_INFO( "[DBG] fetch_version...\r\n" );
    apps_common_lr11xx_fetch_and_print_version( ( void* ) context );
    HAL_DBG_TRACE_INFO( "[DBG] fetch_version OK\r\n" );

    HAL_DBG_TRACE_INFO( "[DBG] radio_init...\r\n" );
    apps_common_lr11xx_radio_init( ( void* ) context );
    HAL_DBG_TRACE_INFO( "[DBG] radio_init OK\r\n" );

    HAL_DBG_TRACE_INFO( "[DBG] set_dio_irq...\r\n" );
    ASSERT_LR11XX_RC( lr11xx_system_set_dio_irq_params( context, IRQ_MASK, 0 ) );
    HAL_DBG_TRACE_INFO( "[DBG] clear_irq...\r\n" );
    ASSERT_LR11XX_RC( lr11xx_system_clear_irq_status( context, LR11XX_SYSTEM_IRQ_ALL_MASK ) );

    HAL_DBG_TRACE_INFO( "--- Listening (RX_CONTINUOUS) ---\r\n" );

    apps_common_lr11xx_handle_pre_rx( );
    ASSERT_LR11XX_RC( lr11xx_radio_set_rx( context, RX_CONTINUOUS ) );

    while( 1 )
    {
        apps_common_lr11xx_irq_process( context, IRQ_MASK );
    }
}

void on_rx_done( void )
{
    uint8_t buffer_rx[PAYLOAD_LENGTH];
    uint8_t size;

    apps_common_lr11xx_handle_post_rx( );

    apps_common_lr11xx_receive( context, buffer_rx, PAYLOAD_LENGTH, &size );

    payload_fields_t fields  = { 0 };
    bool             decoded = payload_decode( buffer_rx, size, &fields );
    bool             valid   = false;

    if( decoded )
    {
        valid = payload_validate( &fields );
    }

    payload_print_human( &fields, decoded, valid );

    apps_common_lr11xx_handle_pre_rx( );
    ASSERT_LR11XX_RC( lr11xx_radio_set_rx( context, RX_CONTINUOUS ) );
}

void on_rx_timeout( void )
{
    apps_common_lr11xx_handle_post_rx( );
    apps_common_lr11xx_handle_pre_rx( );
    ASSERT_LR11XX_RC( lr11xx_radio_set_rx( context, RX_CONTINUOUS ) );
}

void on_rx_crc_error( void )
{
    HAL_DBG_TRACE_WARNING( "CRC error on received packet\r\n" );
    apps_common_lr11xx_handle_post_rx( );
    apps_common_lr11xx_handle_pre_rx( );
    ASSERT_LR11XX_RC( lr11xx_radio_set_rx( context, RX_CONTINUOUS ) );
}

void on_fsk_len_error( void )
{
    apps_common_lr11xx_handle_post_rx( );
    apps_common_lr11xx_handle_pre_rx( );
    ASSERT_LR11XX_RC( lr11xx_radio_set_rx( context, RX_CONTINUOUS ) );
}
