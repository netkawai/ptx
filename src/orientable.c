#include "orientable.h"

PtxOrientation ptx_orientable_get_orientation(const PtxOrientable *self) {
    return self->orientation;
}

void ptx_orientable_set_orientation(PtxOrientable *self,
                                    PtxOrientation orientation) {
    self->orientation = orientation;
}
