#include <xtensa/coreasm.h>
#include <xtensa/corebits.h>
#include <xtensa/config/system.h>
#include "freertos/xtensa_context.h"
#include "esp_debug_helpers.h"
#include "esp_private/panic_reason.h"
#include "sdkconfig.h"
#include "soc/soc.h"
#include "soc/dport_reg.h"

#define L5_INTR_STACK_SIZE  12
#define L5_INTR_A2_OFFSET   0
#define L5_INTR_A3_OFFSET   4
#define L5_INTR_A4_OFFSET   8

.data
_l5_intr_stack:
 .space      L5_INTR_STACK_SIZE

.section .iram1,"ax"
 .global     xt_highint5
 .type       xt_highint5,@function
 .align      4
 .literal .GPIO_STATUS1_W1TC_REG, 0x3FF44058
 .literal .GPIO_STATUS1_REG, 0x3FF44050
 .literal .GPIO_OUT_W1TS_REG, 0x3FF44008
 .literal .GPIO_OUT_W1TC_REG, 0x3FF4400C
 .literal .GPIO__NUM_33, (1<<1)
 .literal .GPIO__NUM_2,  (1<<2)
 .literal .SPI_CMD_REG, 0x3FF64000
 .literal .SPI_USR, (1<<18)
xt_highint5:
    /* save contents of registers A2-A4 */
    movi    a0, _l5_intr_stack
    s32i    a2, a0, L5_INTR_A2_OFFSET
    s32i    a3, a0, L5_INTR_A3_OFFSET
    s32i    a4, a0, L5_INTR_A4_OFFSET

    /* clearing the interrupt status of GPIO_NUM_33 */
    l32r a2, .GPIO_STATUS1_W1TC_REG
    l32r a3, .GPIO__NUM_33
    s32i a3, a2, 0

    /* setting GPIO2 to high */
    l32r a2, .GPIO_OUT_W1TS_REG
    l32r a3, .GPIO__NUM_2
    s32i a3, a2, 0

    /* busy wait */
    movi a4, 50
1:
    addi a4, a4, -1
    bnez a4, 1b

    /* setting GPIO2 to low */
    /* l32r a2, .GPIO_OUT_W1TC_REG */
    /* l32r a3, .GPIO__NUM_2*/
    s32i a3, a2, 4 /* GPIO_OUT_W1TC_REG = GPIO_OUT_W1TS_REG + 4 */

    /* SPI2.cmd.usr = 1; */
    /* The SPI peripheral takes a few hundred nanoseconds 
       to start, no need for extra delay */
    l32r a2, .SPI_CMD_REG
    l32r a3, .SPI_USR
    s32i a3, a2, 0

    /* This hack doesn't seem needed. Add if double interrupting occuring.
    l32r a2, .GPIO_STATUS1_REG
    l32i a2, a2, 0
    memw*/

    /* restore contents of registers A2-A4 */
    movi    a0, _l5_intr_stack
    l32i    a2, a0, L5_INTR_A2_OFFSET
    l32i    a3, a0, L5_INTR_A3_OFFSET
    l32i    a4, a0, L5_INTR_A4_OFFSET
    rsync                                   /* ensure register restored */

    /* hand back from interrupt */
    rsr     a0, EXCSAVE_5
    rfi     5

    .global ld_include_highint_hdl
ld_include_highint_hdl: