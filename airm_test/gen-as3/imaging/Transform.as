/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 */
package imaging 
{
import org.apache.thrift.Set;
import flash.utils.Dictionary;
  public class Transform   {
    public static const UPDOWN:int = 1;
    public static const LEFTRIGHT:int = 2;
    public static const TRANSPOSE:int = 3;
    public static const ROTATE90CW:int = 4;
    public static const ROTATE90CCW:int = 5;
    public static const ROTATE180:int = 6;
    public static const XGRADIENT:int = 7;

    public static const VALID_VALUES:Set = new Set(UPDOWN, LEFTRIGHT, TRANSPOSE, ROTATE90CW, ROTATE90CCW, ROTATE180, XGRADIENT);
    public static const VALUES_TO_NAMES:Dictionary = new Dictionary();
    {
      VALUES_TO_NAMES[UPDOWN] = "UPDOWN";
      VALUES_TO_NAMES[LEFTRIGHT] = "LEFTRIGHT";
      VALUES_TO_NAMES[TRANSPOSE] = "TRANSPOSE";
      VALUES_TO_NAMES[ROTATE90CW] = "ROTATE90CW";
      VALUES_TO_NAMES[ROTATE90CCW] = "ROTATE90CCW";
      VALUES_TO_NAMES[ROTATE180] = "ROTATE180";
      VALUES_TO_NAMES[XGRADIENT] = "XGRADIENT";

    }
  }
}
