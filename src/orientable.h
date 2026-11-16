#ifndef PTX_ORIENTABLE_H
#define PTX_ORIENTABLE_H

/*
 * PtxOrientable:
 *
 * An interface for widgets that can be oriented horizontally or vertically.
 *
 * Port of GtkOrientable (GTK 4):
 * https://docs.gtk.org/gtk4/iface.Orientable.html
 *
 * In GTK this is an interface implemented by widgets such as
 * [class@PtxScrollbar] and GtkScale. In C, ptx models it as a struct that
 * orientable widgets embed as their first member.
 */

/**
 * PtxOrientation:
 * @PTX_ORIENTATION_HORIZONTAL: the horizontal orientation
 * @PTX_ORIENTATION_VERTICAL: the vertical orientation
 *
 * Represents the orientation of widgets and other objects.
 *
 * Since: 0.2
 *
 * Equivalent of: GtkOrientation
 */
typedef enum {
    PTX_ORIENTATION_HORIZONTAL,
    PTX_ORIENTATION_VERTICAL
} PtxOrientation;

/**
 * PtxPositionType:
 * @PTX_POS_LEFT: the left side
 * @PTX_POS_TOP: the top side
 * @PTX_POS_RIGHT: the right side
 * @PTX_POS_BOTTOM: the bottom side
 *
 * Describes which edge of a widget a certain feature is positioned at.
 *
 * Since: 0.2
 *
 * Equivalent of: GtkPositionType
 */
typedef enum {
    PTX_POS_LEFT,
    PTX_POS_TOP,
    PTX_POS_RIGHT,
    PTX_POS_BOTTOM
} PtxPositionType;

/**
 * PtxOrientable:
 * @orientation: the orientation of the widget
 *
 * The interface for orientable widgets.
 *
 * Since: 0.2
 *
 * Equivalent of: GtkOrientable
 */
typedef struct PtxOrientable PtxOrientable;

struct PtxOrientable {
    PtxOrientation orientation;
};

/**
 * ptx_orientable_get_orientation:
 * @self: a `PtxOrientable`
 *
 * Retrieves the orientation of the orientable.
 *
 * Returns: the orientation of the orientable
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_orientable_get_orientation()
 */
PtxOrientation ptx_orientable_get_orientation(const PtxOrientable *self);

/**
 * ptx_orientable_set_orientation:
 * @self: a `PtxOrientable`
 * @orientation: the orientable's new orientation
 *
 * Sets the orientation of the orientable.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_orientable_set_orientation()
 */
void ptx_orientable_set_orientation(PtxOrientable *self,
                                    PtxOrientation orientation);

#endif /* PTX_ORIENTABLE_H */
