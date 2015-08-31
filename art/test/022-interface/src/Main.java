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

/**
 * test calling through an interface
 */
public class Main {
    public static void main(String args[]) {
        int result = 0;
        Iface2Sub1 faceObj;
        ImplA faceObj2;

        faceObj = new ImplBSub();

        result = faceObj.iFunc2(5);
        System.out.print("ImplBSub intf: ");
        System.out.println(result);

        faceObj2 = new ImplA();
        result = faceObj2.iFunc2(5);
        System.out.print("ImplA: ");
        System.out.println(result);

        objectOverrideTests();
    }

  static void check(boolean z) {
    if (!z) {
      throw new AssertionError();
    }
  }

  static void objectOverrideTests() {
    ObjectOverridingInterface o =
        new ObjectOverridingInterface() {
          public boolean equals(Object o) {
            return true;
          }
          public int hashCode() {
            return 0xC001D00D;
          }
          public String toString() {
            return "Mallet's Mallet";
          }
          public int length() {
            return toString().length();
          }
          public char charAt(int i) {
            return toString().charAt(i);
          }
          public CharSequence subSequence(int s, int e) {
            return toString().subSequence(s, e);
          }
        };
    doObjectOverrideTests(o);
  }

  private static interface SubInterface extends Cloneable, SubObjectOverridingInterface {
  }

  private static class SubInterfaceImpl implements SubInterface {
    public int length() {
      return 0;
    }
    public char charAt(int i) {
      return '!';
    }
    public CharSequence subSequence(int s, int e) {
      return "";
    }
  }

  static String subObjectOverrideTests(SubInterface i) {
    return i.toString();
  }

  static void doObjectOverrideTests(ObjectOverridingInterface o) {
    check(o.equals(null));
    check(o.hashCode() == 0xC001D00D);
    check(o.toString().equals("Mallet's Mallet"));
    check(subObjectOverrideTests(new SubInterfaceImpl()) != null);
    System.out.println("objectOverrideTests: SUCCESS");
  }
}
