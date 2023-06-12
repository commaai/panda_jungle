// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "boards/board_declarations.h"
#include "boards/unused_funcs.h"

// ///// Board definition and detection ///// //
#include "stm32h7/lladc.h"
#include "stm32h7/lldac.h"

void detect_board_type(void) {
  hw_type = HW_TYPE_V2;
  current_board = &board_v2;
}
