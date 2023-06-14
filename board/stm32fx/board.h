// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "boards/board_declarations.h"
#include "boards/unused_funcs.h"

// ///// Board definition and detection ///// //
#include "boards/board_v1.h"

void detect_board_type(void) {
  hw_type = HW_TYPE_V1;
  current_board = &board_v1;
}
