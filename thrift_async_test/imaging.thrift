/**
 * Thrift files can namespace, package, or prefix their output in various
 * target languages.
 */
namespace cpp imaging
namespace java imaging
namespace as3 imaging

enum Transform {
  UPDOWN = 1,
  LEFTRIGHT = 2,
  TRANSPOSE = 3,
  ROTATE90CW = 4,
  ROTATE90CCW = 5,
  ROTATE180 = 6,
  XGRADIENT = 7
}

exception InvalidOperation {
  1: i32 what,
  2: string why
}

service Imaging {

  /**
   * A method definition looks like C code. It has a return type, arguments,
   * and optionally a list of exceptions that it may throw. Note that argument
   * lists and exception lists are specified using the exact same syntax as
   * field lists in struct or exception definitions.
   */

   binary mandelbrot(1:i32 w, 2:i32 h) throws (1:InvalidOperation ouch),
   binary transform(1:Transform t, 2:binary img) throws (1:InvalidOperation ouch),
}
