/*
 * Copyright (C) 2012 The Android Open Source Project
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

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class Main {
  public static void main(String[] args) throws Exception {
    arrayAccess();
    arrayStore();
    classCast();
    classNotFound();
    negativeArraySize();
    nullPointers();
    reflection();
    stringIndex();
  }

  private static void assertEquals(String expected, String actual) {
    if (expected == null && actual == null) {
      return;
    }
    if (expected != null && expected.equals(actual)) {
      return;
    }
    throw new AssertionError("not equal\n" +
                                 "expected: " + expected + "\n" +
                                 "actual:   " + actual);
  }

  private static void fail() {
    throw new AssertionError();
  }

  private static void arrayAccess() throws Exception {
    byte[] bs = new byte[1];
    double[] ds = new double[1];
    Object[] os = new Object[1];

    // aput
    try {
      bs[2] = 0;
      fail();
    } catch (ArrayIndexOutOfBoundsException ex) {
      assertEquals("length=1; index=2", ex.getMessage());
    }

    // aget
    try {
      byte b = bs[2];
      fail();
    } catch (ArrayIndexOutOfBoundsException ex) {
      assertEquals("length=1; index=2", ex.getMessage());
    }

    // aput-wide
    try {
      ds[2] = 0.0;
      fail();
    } catch (ArrayIndexOutOfBoundsException ex) {
      assertEquals("length=1; index=2", ex.getMessage());
    }

    // aget-wide
    try {
      double d = ds[2];
      fail();
    } catch (ArrayIndexOutOfBoundsException ex) {
      assertEquals("length=1; index=2", ex.getMessage());
    }

    // aput-object
    try {
      os[2] = null;
      fail();
    } catch (ArrayIndexOutOfBoundsException ex) {
      assertEquals("length=1; index=2", ex.getMessage());
    }

    // aget-object
    try {
      Object o = os[2];
      fail();
    } catch (ArrayIndexOutOfBoundsException ex) {
      assertEquals("length=1; index=2", ex.getMessage());
    }
  }

  private static void arrayStore() throws Exception {
    try {
      Object[] array = new String[10];
      Object o = new Exception();
      array[0] = o;
      fail();
    } catch (ArrayStoreException ex) {
      assertEquals("java.lang.Exception cannot be stored in an array of type java.lang.String[]",
                   ex.getMessage());
    }

    try {
      Object[] array = new C[10][];
      Object o = new Integer(5);
      array[0] = o;
      fail();
    } catch (ArrayStoreException ex) {
      assertEquals("java.lang.Integer cannot be stored in an array of type Main$C[][]",
                   ex.getMessage());
    }

    try {
      Object[] array = new Float[10][];
      Object o = new C[4];
      array[0] = o;
      fail();
    } catch (ArrayStoreException ex) {
      assertEquals("Main$C[] cannot be stored in an array of type java.lang.Float[][]",
                   ex.getMessage());
    }

    try {
      String[] src = new String[] { null, null, null, null, "hello", "goodbye" };
      Integer[] dst = new Integer[10];
      System.arraycopy(src, 1, dst, 0, 5);
    } catch (ArrayStoreException ex) {
      assertEquals("source[4] of type java.lang.String cannot be stored in destination array of type java.lang.Integer[]",
                   ex.getMessage());
    }

    try {
      String[] src = new String[1];
      int[] dst = new int[1];
      System.arraycopy(src, 0, dst, 0, 1);
    } catch (ArrayStoreException ex) {
      assertEquals("Incompatible types: src=java.lang.String[], dst=int[]", ex.getMessage());
    }

    try {
      float[] src = new float[1];
      Runnable[] dst = new Runnable[1];
      System.arraycopy(src, 0, dst, 0, 1);
    } catch (ArrayStoreException ex) {
      assertEquals("Incompatible types: src=float[], dst=java.lang.Runnable[]", ex.getMessage());
    }

    try {
      boolean[] src = new boolean[1];
      double[][] dst = new double[1][];
      System.arraycopy(src, 0, dst, 0, 1);
    } catch (ArrayStoreException ex) {
      assertEquals("Incompatible types: src=boolean[], dst=double[][]", ex.getMessage());
    }

    try {
      String src = "hello";
      Object[] dst = new Object[1];
      System.arraycopy(src, 0, dst, 0, 1);
    } catch (ArrayStoreException ex) {
      assertEquals("source of type java.lang.String is not an array", ex.getMessage());
    }

    try {
      Object[] src = new Object[1];
      Integer dst = new Integer(5);
      System.arraycopy(src, 0, dst, 0, 1);
    } catch (ArrayStoreException ex) {
      assertEquals("destination of type java.lang.Integer is not an array", ex.getMessage());
    }

    // This test demonstrates that the exception message complains
    // about the source in cases where neither source nor
    // destination is an array.
    try {
      System.arraycopy(new C(), 0, "hello", 0, 1);
    } catch (ArrayStoreException ex) {
      assertEquals("source of type Main$C is not an array", ex.getMessage());
    }
  }

  private static void classCast() throws Exception {
    // Reference types.
    try {
      Object o = new Exception();
      String s = (String) o;
      fail();
    } catch (ClassCastException ex) {
      assertEquals("java.lang.Exception cannot be cast to java.lang.String", ex.getMessage());
    }

    // Arrays of reference types.
    try {
      Object o = (C) makeArray(String.class);
      fail();
    } catch (ClassCastException ex) {
      assertEquals("java.lang.String[] cannot be cast to Main$C", ex.getMessage());
    }

    // Arrays of primitives.
    try {
      Object o = (C) makeArray(float.class);
      fail();
    } catch (ClassCastException ex) {
      assertEquals("float[] cannot be cast to Main$C", ex.getMessage());
    }

    // Multi-dimensional arrays of primitives.
    try {
      Object o = (C) makeArray(char[].class);
      fail();
    } catch (ClassCastException ex) {
      assertEquals("char[][] cannot be cast to Main$C", ex.getMessage());
    }

    // Multi-dimensional arrays of references.
    try {
      Object o = (Object[][][]) makeInteger();
      fail();
    } catch (ClassCastException ex) {
      assertEquals("java.lang.Integer cannot be cast to java.lang.Object[][][]", ex.getMessage());
    }
  }

  static class C { }

  /**
   * Helper for testCastOperator and testCastOperatorWithArrays. It's important that the
   * return type is Object, since otherwise the compiler will just reject the code.
   */
  private static Object makeInteger() {
    return new Integer(5);
  }

  /**
   * Helper for testCastOperatorWithArrays. It's important that
   * the return type is Object.
   */
  private static Object makeArray(Class c) {
    return Array.newInstance(c, 1);
  }

  private static void classNotFound() throws Exception {
    try {
      // There is no such thing as an array of void.
      Class.forName("[V");
      fail();
    } catch (ClassNotFoundException ex) {
      assertEquals("Invalid name: [V", ex.getMessage());
    }

    try {
      // This class name is valid, but doesn't exist.
      Class.forName("package.Class");
      fail();
    } catch (ClassNotFoundException ex) {
      assertEquals("package.Class", ex.getMessage());
    }

    try {
      // This array class name is valid, but the type doesn't exist.
      Class.forName("[[Lpackage.Class;");
      fail();
    } catch (ClassNotFoundException ex) {
      assertEquals("[[Lpackage.Class;", ex.getMessage());
    }
  }

  private static void negativeArraySize() throws Exception {
    try {
      int[] is = new int[-123];
      fail();
    } catch (NegativeArraySizeException ex) {
      assertEquals("-123", ex.getMessage());
    }
  }

  // Defeat the fact that null's are untyped for precise detail message creation with quickening.
  private static Object returnNullObject() {
    return null;
  }

  private static A returnNullA() {
    return null;
  }

  private static void nullPointers() throws Exception {
    // Invoke method.
    try {
      Object o = returnNullObject();
      o.hashCode();
      fail();
    } catch (NullPointerException ex) {
      assertEquals("Attempt to invoke virtual method 'int java.lang.Object.hashCode()' on a null object reference", ex.getMessage());
    }

    // Read field.
    try {
      A a = returnNullA();
      int i = a.i;
      fail();
    } catch (NullPointerException ex) {
      assertEquals("Attempt to read from field 'int A.i' on a null object reference", ex.getMessage());
    }

    // Write field.
    try {
      A a = returnNullA();
      a.i = 1;
      fail();
    } catch (NullPointerException ex) {
      assertEquals("Attempt to write to field 'int A.i' on a null object reference", ex.getMessage());
    }

    // Read array.
    try {
      int[] is = null;
      int i = is[0];
      fail();
    } catch (NullPointerException ex) {
      assertEquals("Attempt to read from null array", ex.getMessage());
    }

    // Write array.
    try {
      int[] is = null;
      is[0] = 1;
      fail();
    } catch (NullPointerException ex) {
      assertEquals("Attempt to write to null array", ex.getMessage());
    }

    // Array length.
    try {
      int[] is = null;
      int i = is.length;
      fail();
    } catch (NullPointerException ex) {
      assertEquals("Attempt to get length of null array", ex.getMessage());
    }
  }

  private static void reflection() throws Exception {
    // Can't assign Integer to a String field.
    try {
      Field field = A.class.getField("b");
      field.set(new A(), 5);
      fail();
    } catch (IllegalArgumentException expected) {
      assertEquals("field A.b has type java.lang.String, got java.lang.Integer", expected.getMessage());
    }

    // Can't unbox null to a primitive.
    try {
      Field field = A.class.getField("i");
      field.set(new A(), null);
      fail();
    } catch (IllegalArgumentException expected) {
      assertEquals("field A.i has type int, got null", expected.getMessage());
    }

    // Can't unbox String to a primitive.
    try {
      Field field = A.class.getField("i");
      field.set(new A(), "hello, world!");
      fail();
    } catch (IllegalArgumentException expected) {
      assertEquals("field A.i has type int, got java.lang.String", expected.getMessage());
    }

    // Can't pass an Integer as a String.
    try {
      Method m = A.class.getMethod("m", int.class, String.class);
      m.invoke(new A(), 2, 2);
      fail();
    } catch (IllegalArgumentException expected) {
      assertEquals("method A.m argument 2 has type java.lang.String, got java.lang.Integer", expected.getMessage());
    }

    // Can't pass null as an int.
    try {
      Method m = A.class.getMethod("m", int.class, String.class);
      m.invoke(new A(), null, "");
      fail();
    } catch (IllegalArgumentException expected) {
      assertEquals("method A.m argument 1 has type int, got null", expected.getMessage());
    }

    try {
      Method m = String.class.getMethod("charAt", int.class);
      m.invoke("hello"); // Wrong number of arguments.
      fail();
    } catch (IllegalArgumentException iae) {
      assertEquals("Wrong number of arguments; expected 1, got 0", iae.getMessage());
    }
    try {
      Method m = String.class.getMethod("charAt", int.class);
      m.invoke("hello", "world"); // Wrong type.
      fail();
    } catch (IllegalArgumentException iae) {
      assertEquals("method java.lang.String.charAt argument 1 has type int, got java.lang.String", iae.getMessage());
    }
    try {
      Method m = String.class.getMethod("charAt", int.class);
      m.invoke("hello", (Object) null); // Null for a primitive argument.
      fail();
    } catch (IllegalArgumentException iae) {
      assertEquals("method java.lang.String.charAt argument 1 has type int, got null", iae.getMessage());
    }
    try {
      Method m = String.class.getMethod("charAt", int.class);
      m.invoke(new Integer(5)); // Wrong type for 'this'.
      fail();
    } catch (IllegalArgumentException iae) {
      assertEquals("Expected receiver of type java.lang.String, but got java.lang.Integer", iae.getMessage());
    }
    try {
      Method m = String.class.getMethod("charAt", int.class);
      m.invoke(null); // Null for 'this'.
      fail();
    } catch (NullPointerException npe) {
      assertEquals("null receiver", npe.getMessage());
    }
  }

  private static void stringIndex() throws Exception {
    // charAt too small.
    try {
      "hello".charAt(-1);
      fail();
    } catch (StringIndexOutOfBoundsException ex) {
      assertEquals("length=5; index=-1", ex.getMessage());
    }

    // charAt too big.
    try {
      "hello".charAt(7);
      fail();
    } catch (StringIndexOutOfBoundsException ex) {
      assertEquals("length=5; index=7", ex.getMessage());
    }

    // substring too big.
    try {
      "hello there".substring(9,14);
      fail();
    } catch (StringIndexOutOfBoundsException ex) {
      assertEquals("length=11; regionStart=9; regionLength=5", ex.getMessage());
    }
  }
}

class A {
  public String b;
  public int i;
  public void m(int i, String s) {}
}
