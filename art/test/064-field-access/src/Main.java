/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import other.PublicClass;
import java.lang.reflect.Field;

/*
 * Test field access through reflection.
 */
public class Main {
  public static void main(String[] args) {
    SubClass.main(null);

    try {
      GetNonexistent.main(null);
      System.err.println("Not expected to succeed");
    } catch (VerifyError fe) {
      // dalvik
      System.out.println("Got expected failure");
    } catch (NoSuchFieldError nsfe) {
      // reference
      System.out.println("Got expected failure");
    }
  }

  /*
   * Get the field specified by "field" from "obj".
   *
   * "type" determines which "get" call is made, e.g. 'B' turns into
   * field.getByte().
   *
   * The "expectedException" must match the class of the exception thrown,
   * or be null if no exception was expected.
   *
   * On success, the boxed value retrieved is returned.
   */
  public Object getValue(Field field, Object obj, char type,
      Class expectedException) {
    Object result = null;
    try {
      switch (type) {
        case 'Z':
          result = field.getBoolean(obj);
          break;
        case 'B':
          result = field.getByte(obj);
          break;
        case 'S':
          result = field.getShort(obj);
          break;
        case 'C':
          result = field.getChar(obj);
          break;
        case 'I':
          result = field.getInt(obj);
          break;
        case 'J':
          result = field.getLong(obj);
          break;
        case 'F':
          result = field.getFloat(obj);
          break;
        case 'D':
          result = field.getDouble(obj);
          break;
        case 'L':
          result = field.get(obj);
          break;
        default:
          throw new RuntimeException("bad type '" + type + "'");
      }

      /* success; expected? */
      if (expectedException != null) {
        System.err.println("ERROR: call succeeded for field " + field +
            " with a read of type '" + type +
            "', was expecting " + expectedException);
        Thread.dumpStack();
      }
    } catch (Exception ex) {
      if (expectedException == null) {
        System.err.println("ERROR: call failed unexpectedly: "
            + ex.getClass());
        ex.printStackTrace();
      } else {
        if (!expectedException.equals(ex.getClass())) {
          System.err.println("ERROR: incorrect exception: wanted "
              + expectedException.getName() + ", got "
              + ex.getClass());
          ex.printStackTrace();
        }
      }
    }

    return result;
  }
}

/*
 * Local class with some fields.
 */
class SamePackage {
  public boolean samePackagePublicBooleanInstanceField = true;
  public byte samePackagePublicByteInstanceField = 2;
  public char samePackagePublicCharInstanceField = 3;
  public short samePackagePublicShortInstanceField = 4;
  public int samePackagePublicIntInstanceField = 5;
  public long samePackagePublicLongInstanceField = 6;
  public float samePackagePublicFloatInstanceField = 7.0f;
  public double samePackagePublicDoubleInstanceField = 8.0;
  public Object samePackagePublicObjectInstanceField = "9";

  protected boolean samePackageProtectedBooleanInstanceField = true;
  protected byte samePackageProtectedByteInstanceField = 10;
  protected char samePackageProtectedCharInstanceField = 11;
  protected short samePackageProtectedShortInstanceField = 12;
  protected int samePackageProtectedIntInstanceField = 13;
  protected long samePackageProtectedLongInstanceField = 14;
  protected float samePackageProtectedFloatInstanceField = 15.0f;
  protected double samePackageProtectedDoubleInstanceField = 16.0;
  protected Object samePackageProtectedObjectInstanceField = "17";

  private boolean samePackagePrivateBooleanInstanceField = true;
  private byte samePackagePrivateByteInstanceField = 18;
  private char samePackagePrivateCharInstanceField = 19;
  private short samePackagePrivateShortInstanceField = 20;
  private int samePackagePrivateIntInstanceField = 21;
  private long samePackagePrivateLongInstanceField = 22;
  private float samePackagePrivateFloatInstanceField = 23.0f;
  private double samePackagePrivateDoubleInstanceField = 24.0;
  private Object samePackagePrivateObjectInstanceField = "25";

  /* package */ boolean samePackagePackageBooleanInstanceField = true;
  /* package */ byte samePackagePackageByteInstanceField = 26;
  /* package */ char samePackagePackageCharInstanceField = 27;
  /* package */ short samePackagePackageShortInstanceField = 28;
  /* package */ int samePackagePackageIntInstanceField = 29;
  /* package */ long samePackagePackageLongInstanceField = 30;
  /* package */ float samePackagePackageFloatInstanceField = 31.0f;
  /* package */ double samePackagePackageDoubleInstanceField = 32.0;
  /* package */ Object samePackagePackageObjectInstanceField = "33";

  public static boolean samePackagePublicBooleanStaticField = true;
  public static byte samePackagePublicByteStaticField = 34;
  public static char samePackagePublicCharStaticField = 35;
  public static short samePackagePublicShortStaticField = 36;
  public static int samePackagePublicIntStaticField = 37;
  public static long samePackagePublicLongStaticField = 38;
  public static float samePackagePublicFloatStaticField = 39.0f;
  public static double samePackagePublicDoubleStaticField = 40.0;
  public static Object samePackagePublicObjectStaticField = "41";

  protected static boolean samePackageProtectedBooleanStaticField = true;
  protected static byte samePackageProtectedByteStaticField = 42;
  protected static char samePackageProtectedCharStaticField = 43;
  protected static short samePackageProtectedShortStaticField = 44;
  protected static int samePackageProtectedIntStaticField = 45;
  protected static long samePackageProtectedLongStaticField = 46;
  protected static float samePackageProtectedFloatStaticField = 47.0f;
  protected static double samePackageProtectedDoubleStaticField = 48.0;
  protected static Object samePackageProtectedObjectStaticField = "49";

  private static boolean samePackagePrivateBooleanStaticField = true;
  private static byte samePackagePrivateByteStaticField = 50;
  private static char samePackagePrivateCharStaticField = 51;
  private static short samePackagePrivateShortStaticField = 52;
  private static int samePackagePrivateIntStaticField = 53;
  private static long samePackagePrivateLongStaticField = 54;
  private static float samePackagePrivateFloatStaticField = 55.0f;
  private static double samePackagePrivateDoubleStaticField = 56.0;
  private static Object samePackagePrivateObjectStaticField = "57";

  /* package */ static boolean samePackagePackageBooleanStaticField = true;
  /* package */ static byte samePackagePackageByteStaticField = 58;
  /* package */ static char samePackagePackageCharStaticField = 59;
  /* package */ static short samePackagePackageShortStaticField = 60;
  /* package */ static int samePackagePackageIntStaticField = 61;
  /* package */ static long samePackagePackageLongStaticField = 62;
  /* package */ static float samePackagePackageFloatStaticField = 63.0f;
  /* package */ static double samePackagePackageDoubleStaticField = 64.0;
  /* package */ static Object samePackagePackageObjectStaticField = "65";
}

/*
 * This is a sub-class of other.PublicClass, which should be allowed to access
 * the various protected fields declared by other.PublicClass and its parent
 * other.ProtectedClass.
 */
class SubClass extends PublicClass {
  /*
   * Perform the various tests.
   *
   * localInst.getValue() is performed using an instance of Main as the
   * source of the reflection call.  otherInst.getValue() uses a subclass
   * of OtherPackage as the source.
   */
  public static void main(String[] args) {
    SubClass subOther = new SubClass();
    subOther.doDirectTests();
    subOther.doReflectionTests();
  }

  private static void check(boolean b) {
    if (!b) {
      throw new Error("Test failed");
    }
  }

  public void doDirectTests() {
    check(otherProtectedClassPublicBooleanInstanceField == true);
    check(otherProtectedClassPublicByteInstanceField == 2);
    check(otherProtectedClassPublicCharInstanceField == 3);
    check(otherProtectedClassPublicShortInstanceField == 4);
    check(otherProtectedClassPublicIntInstanceField == 5);
    check(otherProtectedClassPublicLongInstanceField == 6);
    check(otherProtectedClassPublicFloatInstanceField == 7.0f);
    check(otherProtectedClassPublicDoubleInstanceField == 8.0);
    check(otherProtectedClassPublicObjectInstanceField == "9");

    check(otherProtectedClassProtectedBooleanInstanceField == true);
    check(otherProtectedClassProtectedByteInstanceField == 10);
    check(otherProtectedClassProtectedCharInstanceField == 11);
    check(otherProtectedClassProtectedShortInstanceField == 12);
    check(otherProtectedClassProtectedIntInstanceField == 13);
    check(otherProtectedClassProtectedLongInstanceField == 14);
    check(otherProtectedClassProtectedFloatInstanceField == 15.0f);
    check(otherProtectedClassProtectedDoubleInstanceField == 16.0);
    check(otherProtectedClassProtectedObjectInstanceField == "17");

    // check(otherProtectedClassPrivateBooleanInstanceField == true);
    // check(otherProtectedClassPrivateByteInstanceField == 18);
    // check(otherProtectedClassPrivateCharInstanceField == 19);
    // check(otherProtectedClassPrivateShortInstanceField == 20);
    // check(otherProtectedClassPrivateIntInstanceField == 21);
    // check(otherProtectedClassPrivateLongInstanceField == 22);
    // check(otherProtectedClassPrivateFloatInstanceField == 23.0f);
    // check(otherProtectedClassPrivateDoubleInstanceField == 24.0);
    // check(otherProtectedClassPrivateObjectInstanceField == "25");

    // check(otherProtectedClassPackageBooleanInstanceField == true);
    // check(otherProtectedClassPackageByteInstanceField == 26);
    // check(otherProtectedClassPackageCharInstanceField == 27);
    // check(otherProtectedClassPackageShortInstanceField == 28);
    // check(otherProtectedClassPackageIntInstanceField == 29);
    // check(otherProtectedClassPackageLongInstanceField == 30);
    // check(otherProtectedClassPackageFloatInstanceField == 31.0f);
    // check(otherProtectedClassPackageDoubleInstanceField == 32.0);
    // check(otherProtectedClassPackageObjectInstanceField == "33");

    check(otherProtectedClassPublicBooleanStaticField == true);
    check(otherProtectedClassPublicByteStaticField == 34);
    check(otherProtectedClassPublicCharStaticField == 35);
    check(otherProtectedClassPublicShortStaticField == 36);
    check(otherProtectedClassPublicIntStaticField == 37);
    check(otherProtectedClassPublicLongStaticField == 38);
    check(otherProtectedClassPublicFloatStaticField == 39.0f);
    check(otherProtectedClassPublicDoubleStaticField == 40.0);
    check(otherProtectedClassPublicObjectStaticField == "41");

    check(otherProtectedClassProtectedBooleanStaticField == true);
    check(otherProtectedClassProtectedByteStaticField == 42);
    check(otherProtectedClassProtectedCharStaticField == 43);
    check(otherProtectedClassProtectedShortStaticField == 44);
    check(otherProtectedClassProtectedIntStaticField == 45);
    check(otherProtectedClassProtectedLongStaticField == 46);
    check(otherProtectedClassProtectedFloatStaticField == 47.0f);
    check(otherProtectedClassProtectedDoubleStaticField == 48.0);
    check(otherProtectedClassProtectedObjectStaticField == "49");

    // check(otherProtectedClassPrivateBooleanStaticField == true);
    // check(otherProtectedClassPrivateByteStaticField == 50);
    // check(otherProtectedClassPrivateCharStaticField == 51);
    // check(otherProtectedClassPrivateShortStaticField == 52);
    // check(otherProtectedClassPrivateIntStaticField == 53);
    // check(otherProtectedClassPrivateLongStaticField == 54);
    // check(otherProtectedClassPrivateFloatStaticField == 55.0f);
    // check(otherProtectedClassPrivateDoubleStaticField == 56.0);
    // check(otherProtectedClassPrivateObjectStaticField == "57");

    // check(otherProtectedClassPackageBooleanStaticField == true);
    // check(otherProtectedClassPackageByteStaticField == 58);
    // check(otherProtectedClassPackageCharStaticField == 59);
    // check(otherProtectedClassPackageShortStaticField == 60);
    // check(otherProtectedClassPackageIntStaticField == 61);
    // check(otherProtectedClassPackageLongStaticField == 62);
    // check(otherProtectedClassPackageFloatStaticField == 63.0f);
    // check(otherProtectedClassPackageDoubleStaticField == 64.0);
    // check(otherProtectedClassPackageObjectStaticField == "65");

    check(otherPublicClassPublicBooleanInstanceField == true);
    check(otherPublicClassPublicByteInstanceField == -2);
    check(otherPublicClassPublicCharInstanceField == (char)-3);
    check(otherPublicClassPublicShortInstanceField == -4);
    check(otherPublicClassPublicIntInstanceField == -5);
    check(otherPublicClassPublicLongInstanceField == -6);
    check(otherPublicClassPublicFloatInstanceField == -7.0f);
    check(otherPublicClassPublicDoubleInstanceField == -8.0);
    check(otherPublicClassPublicObjectInstanceField == "-9");

    check(otherPublicClassProtectedBooleanInstanceField == true);
    check(otherPublicClassProtectedByteInstanceField == -10);
    check(otherPublicClassProtectedCharInstanceField == (char)-11);
    check(otherPublicClassProtectedShortInstanceField == -12);
    check(otherPublicClassProtectedIntInstanceField == -13);
    check(otherPublicClassProtectedLongInstanceField == -14);
    check(otherPublicClassProtectedFloatInstanceField == -15.0f);
    check(otherPublicClassProtectedDoubleInstanceField == -16.0);
    check(otherPublicClassProtectedObjectInstanceField == "-17");

    // check(otherPublicClassPrivateBooleanInstanceField == true);
    // check(otherPublicClassPrivateByteInstanceField == -18);
    // check(otherPublicClassPrivateCharInstanceField == (char)-19);
    // check(otherPublicClassPrivateShortInstanceField == -20);
    // check(otherPublicClassPrivateIntInstanceField == -21);
    // check(otherPublicClassPrivateLongInstanceField == -22);
    // check(otherPublicClassPrivateFloatInstanceField == -23.0f);
    // check(otherPublicClassPrivateDoubleInstanceField == -24.0);
    // check(otherPublicClassPrivateObjectInstanceField == "-25");

    // check(otherPublicClassPackageBooleanInstanceField == true);
    // check(otherPublicClassPackageByteInstanceField == -26);
    // check(otherPublicClassPackageCharInstanceField == (char)-27);
    // check(otherPublicClassPackageShortInstanceField == -28);
    // check(otherPublicClassPackageIntInstanceField == -29);
    // check(otherPublicClassPackageLongInstanceField == -30);
    // check(otherPublicClassPackageFloatInstanceField == -31.0f);
    // check(otherPublicClassPackageDoubleInstanceField == -32.0);
    // check(otherPublicClassPackageObjectInstanceField == "-33");

    check(otherPublicClassPublicBooleanStaticField == true);
    check(otherPublicClassPublicByteStaticField == -34);
    check(otherPublicClassPublicCharStaticField == (char)-35);
    check(otherPublicClassPublicShortStaticField == -36);
    check(otherPublicClassPublicIntStaticField == -37);
    check(otherPublicClassPublicLongStaticField == -38);
    check(otherPublicClassPublicFloatStaticField == -39.0f);
    check(otherPublicClassPublicDoubleStaticField == -40.0);
    check(otherPublicClassPublicObjectStaticField == "-41");

    check(otherPublicClassProtectedBooleanStaticField == true);
    check(otherPublicClassProtectedByteStaticField == -42);
    check(otherPublicClassProtectedCharStaticField == (char)-43);
    check(otherPublicClassProtectedShortStaticField == -44);
    check(otherPublicClassProtectedIntStaticField == -45);
    check(otherPublicClassProtectedLongStaticField == -46);
    check(otherPublicClassProtectedFloatStaticField == -47.0f);
    check(otherPublicClassProtectedDoubleStaticField == -48.0);
    check(otherPublicClassProtectedObjectStaticField == "-49");

    // check(otherPublicClassPrivateBooleanStaticField == true);
    // check(otherPublicClassPrivateByteStaticField == -50);
    // check(otherPublicClassPrivateCharStaticField == (char)-51);
    // check(otherPublicClassPrivateShortStaticField == -52);
    // check(otherPublicClassPrivateIntStaticField == -53);
    // check(otherPublicClassPrivateLongStaticField == -54);
    // check(otherPublicClassPrivateFloatStaticField == -55.0f);
    // check(otherPublicClassPrivateDoubleStaticField == -56.0);
    // check(otherPublicClassPrivateObjectStaticField == "-57");

    // check(otherPublicClassPackageBooleanStaticField == true);
    // check(otherPublicClassPackageByteStaticField == -58);
    // check(otherPublicClassPackageCharStaticField == (char)-59);
    // check(otherPublicClassPackageShortStaticField == -60);
    // check(otherPublicClassPackageIntStaticField == -61);
    // check(otherPublicClassPackageLongStaticField == -62);
    // check(otherPublicClassPackageFloatStaticField == -63.0f);
    // check(otherPublicClassPackageDoubleStaticField == -64.0);
    // check(otherPublicClassPackageObjectStaticField == "-65");

    SamePackage s = new SamePackage();
    check(s.samePackagePublicBooleanInstanceField == true);
    check(s.samePackagePublicByteInstanceField == 2);
    check(s.samePackagePublicCharInstanceField == 3);
    check(s.samePackagePublicShortInstanceField == 4);
    check(s.samePackagePublicIntInstanceField == 5);
    check(s.samePackagePublicLongInstanceField == 6);
    check(s.samePackagePublicFloatInstanceField == 7.0f);
    check(s.samePackagePublicDoubleInstanceField == 8.0);
    check(s.samePackagePublicObjectInstanceField == "9");

    check(s.samePackageProtectedBooleanInstanceField == true);
    check(s.samePackageProtectedByteInstanceField == 10);
    check(s.samePackageProtectedCharInstanceField == 11);
    check(s.samePackageProtectedShortInstanceField == 12);
    check(s.samePackageProtectedIntInstanceField == 13);
    check(s.samePackageProtectedLongInstanceField == 14);
    check(s.samePackageProtectedFloatInstanceField == 15.0f);
    check(s.samePackageProtectedDoubleInstanceField == 16.0);
    check(s.samePackageProtectedObjectInstanceField == "17");

    // check(s.samePackagePrivateBooleanInstanceField == true);
    // check(s.samePackagePrivateByteInstanceField == 18);
    // check(s.samePackagePrivateCharInstanceField == 19);
    // check(s.samePackagePrivateShortInstanceField == 20);
    // check(s.samePackagePrivateIntInstanceField == 21);
    // check(s.samePackagePrivateLongInstanceField == 22);
    // check(s.samePackagePrivateFloatInstanceField == 23.0f);
    // check(s.samePackagePrivateDoubleInstanceField == 24.0);
    // check(s.samePackagePrivateObjectInstanceField == "25");

    check(s.samePackagePackageBooleanInstanceField == true);
    check(s.samePackagePackageByteInstanceField == 26);
    check(s.samePackagePackageCharInstanceField == 27);
    check(s.samePackagePackageShortInstanceField == 28);
    check(s.samePackagePackageIntInstanceField == 29);
    check(s.samePackagePackageLongInstanceField == 30);
    check(s.samePackagePackageFloatInstanceField == 31.0f);
    check(s.samePackagePackageDoubleInstanceField == 32.0);
    check(s.samePackagePackageObjectInstanceField == "33");

    check(SamePackage.samePackagePublicBooleanStaticField == true);
    check(SamePackage.samePackagePublicByteStaticField == 34);
    check(SamePackage.samePackagePublicCharStaticField == 35);
    check(SamePackage.samePackagePublicShortStaticField == 36);
    check(SamePackage.samePackagePublicIntStaticField == 37);
    check(SamePackage.samePackagePublicLongStaticField == 38);
    check(SamePackage.samePackagePublicFloatStaticField == 39.0f);
    check(SamePackage.samePackagePublicDoubleStaticField == 40.0);
    check(SamePackage.samePackagePublicObjectStaticField == "41");

    check(SamePackage.samePackageProtectedBooleanStaticField == true);
    check(SamePackage.samePackageProtectedByteStaticField == 42);
    check(SamePackage.samePackageProtectedCharStaticField == 43);
    check(SamePackage.samePackageProtectedShortStaticField == 44);
    check(SamePackage.samePackageProtectedIntStaticField == 45);
    check(SamePackage.samePackageProtectedLongStaticField == 46);
    check(SamePackage.samePackageProtectedFloatStaticField == 47.0f);
    check(SamePackage.samePackageProtectedDoubleStaticField == 48.0);
    check(SamePackage.samePackageProtectedObjectStaticField == "49");

    // check(SamePackage.samePackagePrivateBooleanStaticField == true);
    // check(SamePackage.samePackagePrivateByteStaticField == 50);
    // check(SamePackage.samePackagePrivateCharStaticField == 51);
    // check(SamePackage.samePackagePrivateShortStaticField == 52);
    // check(SamePackage.samePackagePrivateIntStaticField == 53);
    // check(SamePackage.samePackagePrivateLongStaticField == 54);
    // check(SamePackage.samePackagePrivateFloatStaticField == 55.0f);
    // check(SamePackage.samePackagePrivateDoubleStaticField == 56.0);
    // check(SamePackage.samePackagePrivateObjectStaticField == "57");

    check(SamePackage.samePackagePackageBooleanStaticField == true);
    check(SamePackage.samePackagePackageByteStaticField == 58);
    check(SamePackage.samePackagePackageCharStaticField == 59);
    check(SamePackage.samePackagePackageShortStaticField == 60);
    check(SamePackage.samePackagePackageIntStaticField == 61);
    check(SamePackage.samePackagePackageLongStaticField == 62);
    check(SamePackage.samePackagePackageFloatStaticField == 63.0f);
    check(SamePackage.samePackagePackageDoubleStaticField == 64.0);
    check(SamePackage.samePackagePackageObjectStaticField == "65");
  }

  private static boolean compatibleTypes(char srcType, char dstType) {
    switch (dstType) {
      case 'Z':
      case 'C':
      case 'B':
        return srcType == dstType;
      case 'S':
        return srcType == 'B' || srcType == 'S';
      case 'I':
        return srcType == 'B' || srcType == 'C' || srcType == 'S' || srcType == 'I';
      case 'J':
        return srcType == 'B' || srcType == 'C' || srcType == 'S' || srcType == 'I' ||
        srcType == 'J';
      case 'F':
        return srcType == 'B' || srcType == 'C' || srcType == 'S' || srcType == 'I' ||
        srcType == 'J' || srcType == 'F';
      case 'D':
        return srcType == 'B' || srcType == 'C' || srcType == 'S' || srcType == 'I' ||
        srcType == 'J' || srcType == 'F' || srcType == 'D';
      case 'L':
        return true;
      default:
        throw new Error("Unexpected type char " + dstType);
    }
  }

  public void doReflectionTests() {
    String typeChars = "ZBCSIJFDL";
    String fieldNameForTypeChar[] = {
        "Boolean",
        "Byte",
        "Char",
        "Short",
        "Int",
        "Long",
        "Float",
        "Double",
        "Object"
    };

    Main localInst = new Main();
    SamePackage samePkgInst = new SamePackage();
    PublicClass otherPkgInst = new PublicClass();
    Object plainObj = new Object();

    for (int round = 0; round < 3; round++) {
      Object validInst;
      Field[] fields;
      boolean same_package = false;
      switch (round) {
        case 0:
          validInst = new SamePackage();
          fields = SamePackage.class.getDeclaredFields();
          check(fields.length == 72);
          same_package = true;
          break;
        case 1:
          validInst = new PublicClass();
          fields = PublicClass.class.getDeclaredFields();
          check(fields.length == 72);
          break;
        default:
          validInst = new PublicClass();
          fields = PublicClass.class.getSuperclass().getDeclaredFields();
          check(fields.length == 72);
          break;
      }
      for (Field f : fields) {
        char typeChar = '?';
        for (int i = 0; i < fieldNameForTypeChar.length; i++) {
          if (f.getName().contains(fieldNameForTypeChar[i])) {
            typeChar = typeChars.charAt(i);
            break;
          }
        }
        // Check access or lack of to field.
        Class<?> subClassAccessExceptionClass = null;
        if (f.getName().contains("Private") ||
            (!same_package && f.getName().contains("Package"))) {
          // ART deliberately doesn't throw IllegalAccessException.
          // subClassAccessExceptionClass = IllegalAccessException.class;
        }
        Class<?> mainClassAccessExceptionClass = null;
        if (f.getName().contains("Private") ||
            (!same_package && f.getName().contains("Package")) ||
            (!same_package && f.getName().contains("Protected"))) {
          // ART deliberately doesn't throw IllegalAccessException.
          // mainClassAccessExceptionClass = IllegalAccessException.class;
        }

        this.getValue(f, validInst, typeChar, subClassAccessExceptionClass);
        localInst.getValue(f, validInst, typeChar, mainClassAccessExceptionClass);

        // Check things that can get beyond the IllegalAccessException.
        if (subClassAccessExceptionClass == null) {
          // Check NPE.
          Class<?> npeClass = null;
          if (!f.getName().contains("Static")) {
            npeClass = NullPointerException.class;
          }

          this.getValue(f, null, typeChar, npeClass);
          if (mainClassAccessExceptionClass == null) {
            localInst.getValue(f, null, typeChar, npeClass);
          }

          // Check access of wrong field type for valid instance.
          for (int i = 0; i < typeChars.length(); i++) {
            char otherChar = typeChars.charAt(i);
            Class<?> illArgClass = compatibleTypes(typeChar, otherChar) ?
                null : IllegalArgumentException.class;
            this.getValue(f, validInst, otherChar, illArgClass);
            if (mainClassAccessExceptionClass == null) {
              localInst.getValue(f, validInst, otherChar, illArgClass);
            }
          }

          if (!f.getName().contains("Static")) {
            // Wrong object.
            this.getValue(f, plainObj, typeChar, IllegalArgumentException.class);
            if (mainClassAccessExceptionClass == null) {
              localInst.getValue(f, plainObj, typeChar, IllegalArgumentException.class);
            }
          }
        }
      }
    }
    System.out.println("good");
  }

  /*
   * [this is a clone of Main.getValue() -- the class issuing the
   * reflection call is significant]
   */
  public Object getValue(Field field, Object obj, char type,
      Class expectedException) {

    Object result = null;
    try {
      switch (type) {
        case 'Z':
          result = field.getBoolean(obj);
          break;
        case 'B':
          result = field.getByte(obj);
          break;
        case 'S':
          result = field.getShort(obj);
          break;
        case 'C':
          result = field.getChar(obj);
          break;
        case 'I':
          result = field.getInt(obj);
          break;
        case 'J':
          result = field.getLong(obj);
          break;
        case 'F':
          result = field.getFloat(obj);
          break;
        case 'D':
          result = field.getDouble(obj);
          break;
        case 'L':
          result = field.get(obj);
          break;
        default:
          throw new RuntimeException("bad type '" + type + "'");
      }

      /* success; expected? */
      if (expectedException != null) {
        System.err.println("ERROR: call succeeded for field " + field +
            " with a read of type '" + type +
            "', was expecting " + expectedException);
        Thread.dumpStack();
      }
    } catch (Exception ex) {
      if (expectedException == null) {
        System.err.println("ERROR: call failed unexpectedly: "
            + ex.getClass());
        ex.printStackTrace();
      } else {
        if (!expectedException.equals(ex.getClass())) {
          System.err.println("ERROR: incorrect exception: wanted "
              + expectedException.getName() + ", got "
              + ex.getClass());
          ex.printStackTrace();
        }
      }
    }

    return result;
  }
}
