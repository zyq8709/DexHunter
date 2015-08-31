/*
 * Copyright (C) 2011 The Android Open Source Project
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
import java.lang.reflect.*;

public class ReturnsAndArgPassing {

  public static final String testName = "ReturnsAndArgPassing";

  static void check(boolean x) {
    if (!x) {
      throw new AssertionError(testName + " Check failed");
    }
  }

  interface MyInterface {
    void voidFoo();
    void voidBar();
    boolean booleanFoo();
    boolean booleanBar();
    byte byteFoo();
    byte byteBar();
    char charFoo();
    char charBar();
    short shortFoo();
    short shortBar();
    int intFoo();
    int intBar();
    long longFoo();
    long longBar();
    float floatFoo();
    float floatBar();
    double doubleFoo();
    double doubleBar();
    Object selectArg(int select, int a, long b, float c, double d, Object x);
  }

  static int fooInvocations = 0;
  static int barInvocations = 0;

  static class MyInvocationHandler implements InvocationHandler {
    boolean causeNpeOnReturn = false;
    Class<?> returnType = null;
    public Object invoke(Object proxy, Method method, Object[] args) {
      check(proxy instanceof Proxy);
      check(method.getDeclaringClass() == MyInterface.class);
      String name = method.getName();
      if (name.endsWith("Foo")) {
        check(args == null);
        fooInvocations++;
      } else if (name.endsWith("Bar")) {
        check(args == null);
        barInvocations++;
      }
      if (causeNpeOnReturn) {
        return null;
      } else if (name.equals("voidFoo") || name.equals("voidBar")) {
        return null;
      } else if (name.equals("booleanFoo")) {
        return true;
      } else if (name.equals("booleanBar")) {
        return false;
      } else if (name.equals("selectArg")) {
        check(args.length == 6);
        int select = (Integer)args[0];
        return args[select];
      } else {
        try {
          if (name.endsWith("Foo")) {
            return returnType.getField("MAX_VALUE").get(null);
          } else {
            check(name.endsWith("Bar"));
            return returnType.getField("MIN_VALUE").get(null);
          }
        } catch (Exception e) {
          throw new Error("return type = " + returnType, e);
        }
      }
    }
  }

  static void testProxyReturns() {
    System.out.println(testName + ".testProxyReturns RUNNING");
    MyInvocationHandler myHandler = new MyInvocationHandler();
    MyInterface proxyMyInterface =
        (MyInterface)Proxy.newProxyInstance(ReturnsAndArgPassing.class.getClassLoader(),
                                            new Class[] { MyInterface.class },
                                            myHandler);
    check(fooInvocations == 0);
    proxyMyInterface.voidFoo();
    check(fooInvocations == 1);

    check(barInvocations == 0);
    proxyMyInterface.voidBar();
    check(barInvocations == 1);

    check(fooInvocations == 1);
    myHandler.returnType = Boolean.class;
    check(proxyMyInterface.booleanFoo() == true);
    check(fooInvocations == 2);

    check(barInvocations == 1);
    check(proxyMyInterface.booleanBar() == false);
    check(barInvocations == 2);

    check(fooInvocations == 2);
    myHandler.returnType = Byte.class;
    check(proxyMyInterface.byteFoo() == Byte.MAX_VALUE);
    check(fooInvocations == 3);

    check(barInvocations == 2);
    check(proxyMyInterface.byteBar() == Byte.MIN_VALUE);
    check(barInvocations == 3);

    check(fooInvocations == 3);
    myHandler.returnType = Character.class;
    check(proxyMyInterface.charFoo() == Character.MAX_VALUE);
    check(fooInvocations == 4);

    check(barInvocations == 3);
    check(proxyMyInterface.charBar() == Character.MIN_VALUE);
    check(barInvocations == 4);

    check(fooInvocations == 4);
    myHandler.returnType = Short.class;
    check(proxyMyInterface.shortFoo() == Short.MAX_VALUE);
    check(fooInvocations == 5);

    check(barInvocations == 4);
    check(proxyMyInterface.shortBar() == Short.MIN_VALUE);
    check(barInvocations == 5);

    check(fooInvocations == 5);
    myHandler.returnType = Integer.class;
    check(proxyMyInterface.intFoo() == Integer.MAX_VALUE);
    check(fooInvocations == 6);

    check(barInvocations == 5);
    check(proxyMyInterface.intBar() == Integer.MIN_VALUE);
    check(barInvocations == 6);

    check(fooInvocations == 6);
    myHandler.returnType = Long.class;
    check(proxyMyInterface.longFoo() == Long.MAX_VALUE);
    check(fooInvocations == 7);

    check(barInvocations == 6);
    check(proxyMyInterface.longBar() == Long.MIN_VALUE);
    check(barInvocations == 7);

    check(fooInvocations == 7);
    myHandler.returnType = Float.class;
    check(proxyMyInterface.floatFoo() == Float.MAX_VALUE);
    check(fooInvocations == 8);

    check(barInvocations == 7);
    check(proxyMyInterface.floatBar() == Float.MIN_VALUE);
    check(barInvocations == 8);

    check(fooInvocations == 8);
    myHandler.returnType = Double.class;
    check(proxyMyInterface.doubleFoo() == Double.MAX_VALUE);
    check(fooInvocations == 9);

    check(barInvocations == 8);
    check(proxyMyInterface.doubleBar() == Double.MIN_VALUE);
    check(barInvocations == 9);

    // Toggle flag to get return values to cause NPEs
    myHandler.causeNpeOnReturn = true;

    check(fooInvocations == 9);
    try {
        proxyMyInterface.booleanFoo();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(fooInvocations == 10);

    check(barInvocations == 9);
    try {
        proxyMyInterface.booleanBar();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(barInvocations == 10);

    check(fooInvocations == 10);
    try {
        proxyMyInterface.byteFoo();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(fooInvocations == 11);

    check(barInvocations == 10);
    try {
        proxyMyInterface.byteBar();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(barInvocations == 11);

    check(fooInvocations == 11);
    try {
        proxyMyInterface.charFoo();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(fooInvocations == 12);

    check(barInvocations == 11);
    try {
        proxyMyInterface.charBar();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(barInvocations == 12);

    check(fooInvocations == 12);
    try {
        proxyMyInterface.shortFoo();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(fooInvocations == 13);

    check(barInvocations == 12);
    try {
        proxyMyInterface.shortBar();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(barInvocations == 13);

    check(fooInvocations == 13);
    try {
        proxyMyInterface.intFoo();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(fooInvocations == 14);

    check(barInvocations == 13);
    try {
        proxyMyInterface.intBar();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(barInvocations == 14);

    check(fooInvocations == 14);
    try {
        proxyMyInterface.longFoo();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(fooInvocations == 15);

    check(barInvocations == 14);
    try {
        proxyMyInterface.longBar();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(barInvocations == 15);

    check(fooInvocations == 15);
    try {
        proxyMyInterface.floatFoo();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(fooInvocations == 16);

    check(barInvocations == 15);
    try {
        proxyMyInterface.floatBar();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(barInvocations == 16);

    check(fooInvocations == 16);
    try {
        proxyMyInterface.doubleFoo();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(fooInvocations == 17);

    check(barInvocations == 16);
    try {
        proxyMyInterface.doubleBar();
        throw new AssertionError("Expected NPE");
    } catch (NullPointerException e) {
    }
    check(barInvocations == 17);

    // Toggle flag to stop NPEs
    myHandler.causeNpeOnReturn = false;

    check(fooInvocations == 17);
    myHandler.returnType = Double.class;  // Double -> byte == fail
    try {
        proxyMyInterface.byteFoo();
        throw new AssertionError("Expected ClassCastException");
    } catch (ClassCastException e) {
    }
    check(fooInvocations == 18);

    check(barInvocations == 17);
    try {
        proxyMyInterface.byteBar();
        throw new AssertionError("Expected NPE");
    } catch (ClassCastException e) {
    }
    check(barInvocations == 18);

    check(fooInvocations == 18);
    myHandler.returnType = Float.class;  // Float -> byte == fail
    try {
        proxyMyInterface.byteFoo();
        throw new AssertionError("Expected ClassCastException");
    } catch (ClassCastException e) {
    }
    check(fooInvocations == 19);

    check(barInvocations == 18);
    try {
        proxyMyInterface.byteBar();
        throw new AssertionError("Expected NPE");
    } catch (ClassCastException e) {
    }
    check(barInvocations == 19);

    check(fooInvocations == 19);
    myHandler.returnType = Long.class;  // Long -> byte == fail
    try {
        proxyMyInterface.byteFoo();
        throw new AssertionError("Expected ClassCastException");
    } catch (ClassCastException e) {
    }
    check(fooInvocations == 20);

    check(barInvocations == 19);
    try {
        proxyMyInterface.byteBar();
        throw new AssertionError("Expected NPE");
    } catch (ClassCastException e) {
    }
    check(barInvocations == 20);

    check(fooInvocations == 20);
    myHandler.returnType = Integer.class;  // Int -> byte == fail
    try {
        proxyMyInterface.byteFoo();
        throw new AssertionError("Expected ClassCastException");
    } catch (ClassCastException e) {
    }
    check(fooInvocations == 21);

    check(barInvocations == 20);
    try {
        proxyMyInterface.byteBar();
        throw new AssertionError("Expected NPE");
    } catch (ClassCastException e) {
    }
    check(barInvocations == 21);

    check(fooInvocations == 21);
    myHandler.returnType = Short.class;  // Short -> byte == fail
    try {
        proxyMyInterface.byteFoo();
        throw new AssertionError("Expected ClassCastException");
    } catch (ClassCastException e) {
    }
    check(fooInvocations == 22);

    check(barInvocations == 21);
    try {
        proxyMyInterface.byteBar();
        throw new AssertionError("Expected NPE");
    } catch (ClassCastException e) {
    }
    check(barInvocations == 22);

    check(fooInvocations == 22);
    myHandler.returnType = Character.class;  // Char -> byte == fail
    try {
        proxyMyInterface.byteFoo();
        throw new AssertionError("Expected ClassCastException");
    } catch (ClassCastException e) {
    }
    check(fooInvocations == 23);

    check(barInvocations == 22);
    try {
        proxyMyInterface.byteBar();
        throw new AssertionError("Expected NPE");
    } catch (ClassCastException e) {
    }
    check(barInvocations == 23);

    check(fooInvocations == 23);
    myHandler.returnType = Character.class;  // Char -> short == fail
    try {
        proxyMyInterface.shortFoo();
        throw new AssertionError("Expected ClassCastException");
    } catch (ClassCastException e) {
    }
    check(fooInvocations == 24);

    check(barInvocations == 23);
    try {
        proxyMyInterface.shortBar();
        throw new AssertionError("Expected NPE");
    } catch (ClassCastException e) {
    }
    check(barInvocations == 24);

    System.out.println(testName + ".testProxyReturns PASSED");
  }

  static void testProxyArgPassing() {
    System.out.println(testName + ".testProxyArgPassing RUNNING");
    MyInvocationHandler myHandler = new MyInvocationHandler();
    MyInterface proxyMyInterface =
        (MyInterface)Proxy.newProxyInstance(ReturnsAndArgPassing.class.getClassLoader(),
                                            new Class[] { MyInterface.class },
                                            myHandler);

    check((Integer)proxyMyInterface.selectArg(0, Integer.MAX_VALUE, Long.MAX_VALUE,
        Float.MAX_VALUE, Double.MAX_VALUE, Object.class) == 0);
    check((Integer)proxyMyInterface.selectArg(1, Integer.MAX_VALUE, Long.MAX_VALUE,
        Float.MAX_VALUE, Double.MAX_VALUE, Object.class) == Integer.MAX_VALUE);
    check((Long)proxyMyInterface.selectArg(2, Integer.MAX_VALUE, Long.MAX_VALUE,
        Float.MAX_VALUE, Double.MAX_VALUE, Object.class) == Long.MAX_VALUE);
    check((Float)proxyMyInterface.selectArg(3, Integer.MAX_VALUE, Long.MAX_VALUE,
        Float.MAX_VALUE, Double.MAX_VALUE, Object.class) == Float.MAX_VALUE);
    check((Double)proxyMyInterface.selectArg(4, Integer.MAX_VALUE, Long.MAX_VALUE,
        Float.MAX_VALUE, Double.MAX_VALUE, Object.class) == Double.MAX_VALUE);
    check(proxyMyInterface.selectArg(5, Integer.MAX_VALUE, Long.MAX_VALUE,
        Float.MAX_VALUE, Double.MAX_VALUE, Object.class) == Object.class);

    System.out.println(testName + ".testProxyArgPassing PASSED");
  }

  public static void main(String args[]) {
    testProxyReturns();
    testProxyArgPassing();
  }
}
