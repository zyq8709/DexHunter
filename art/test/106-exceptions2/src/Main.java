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

class Main {

    public int ifoo;

    /* Test requires visual inspection of object code to verify */
    int noThrow(Main nonNullA,
                Main nonNullB,
                Main nonNullC) {

        // "this" check should be eliminated on both IGET/IPUT
        ifoo++;

       // "this" check should be eliminated on both IGET/IPUT
       if (ifoo != 321) {
           // Check not eliminated
           nonNullA.ifoo = 12;
           // Check not eliminated
           nonNullB.ifoo = 21;
       } else {
           // Check not eliminated
           nonNullA.ifoo = 12;
       }

       // Check eliminated
       nonNullA.ifoo = 13;

       // Check not eliminated
       nonNullB.ifoo = 21;

       nonNullC = nonNullB;

       // Check eliminated
       nonNullC.ifoo = 32;

      // All null checks eliminated
      return ifoo + nonNullA.ifoo + nonNullB.ifoo + nonNullC.ifoo;
    }

    /* Test to ensure we don't remove necessary null checks */
    int checkThrow(Main nonNullA,
                   Main nonNullB,
                   Main nonNullC,
                   Main nullA,
                   Main nullB,
                   Main nullC) {

        // "this" check should be eliminated on both IGET/IPUT
        ifoo++;

       try {
           nullA.ifoo = 12;
           // Should not be reached
           return -1;
       } catch (NullPointerException npe) {
           ifoo++;
       }
       try {
           nullB.ifoo = 13;
           // Should not be reached
           return -2;
       } catch (NullPointerException npe) {
           ifoo++;
       }
       try {
           nullC.ifoo = 14;
           // Should not be reached
           return -3;
       } catch (NullPointerException npe) {
           ifoo++;
       }

       // "this" check should be eliminated
       if (ifoo != 321) {
           // Check not eliminated
           nonNullA.ifoo = 12;
           // Check not eliminated
           nonNullB.ifoo = 21;
           // Should throw here
           try {
               nullA.ifoo = 11;
               return -4;
           } catch (NullPointerException npe) {
           }
       } else {
           // Check not eliminated
           nonNullA.ifoo = 12;
           // Should throw here
           try {
               nullA.ifoo = 11;
               return -5;
           } catch (NullPointerException npe) {
           }
       }

       // Check not eliminated
       nonNullA.ifoo = 13;

       // Check not eliminated
       nonNullB.ifoo = 21;

       nonNullC = nonNullB;

       // Check eliminated
       nonNullC.ifoo = 32;

       // Should throw here
       try {
           nullA.ifoo = 13;
           return -6;
       } catch (NullPointerException npe) {
       }

      return ifoo + nonNullA.ifoo + nonNullB.ifoo + nonNullC.ifoo;
    }


    static int nullCheckTestNoThrow(int x) {
        Main base = new Main();
        Main a = new Main();
        Main b = new Main();
        Main c = new Main();
        base.ifoo = x;
        return base.noThrow(a,b,c);
    }

    static int nullCheckTestThrow(int x) {
        Main base = new Main();
        Main a = new Main();
        Main b = new Main();
        Main c = new Main();
        Main d = null;
        Main e = null;
        Main f = null;
        base.ifoo = x;
        return base.checkThrow(a,b,c,d,e,f);
    }


    static void throwImplicitAIOBE(int[] array, int index) {
      array[index] = 0;
    }

    static int checkAIOBE() {
      int[] array = new int[10];
      int res;
      try {
        throwImplicitAIOBE(array, 11);
        res = 123;
      } catch (NullPointerException npe) {
        res = 768;
      } catch (ArrayIndexOutOfBoundsException e) {
        res = 456;
      }
      try {
        throwImplicitAIOBE(array, -1);
        res += 123;
      } catch (NullPointerException npe) {
        res += 768;
      } catch (ArrayIndexOutOfBoundsException e) {
        res += 456;
      }
      return res;
    }

    static int throwImplicitDivZero(int x, int y) {
      return x / y;
    }

    static int checkDivZero() {
      try {
        throwImplicitDivZero(100, 0);
        return 123;
      } catch (NullPointerException npe) {
        return 768;
      } catch (ArrayIndexOutOfBoundsException e) {
        return 987;
      } catch (ArithmeticException e) {
        return 456;
      }
    }

    public static void main(String[] args) {
        boolean failure = false;
        int res;

        res = nullCheckTestNoThrow(1976);
        if (res == 2054) {
            System.out.println("nullCheckTestNoThrow PASSED");
        } else {
            System.out.println("nullCheckTestNoThrow FAILED: " + res);
            failure = true;
        }

        res = nullCheckTestThrow(1976);
        if (res == 2057) {
            System.out.println("nullCheckTestThrow PASSED");
        } else {
            System.out.println("nullCheckTestThrow FAILED: " + res);
            failure = true;
        }

        res = checkAIOBE();
        if (res == 912) {
          System.out.println("checkAIOBE PASSED");
        } else {
          System.out.println("checkAIOBE FAILED: " + res);
          failure = true;
        }

        res = checkDivZero();
        if (res == 456) {
          System.out.println("checkDivZero PASSED");
        } else {
          System.out.println("checkDivZero FAILED: " + res);
          failure = true;
        }
        System.exit(failure ? 1 : 0);
    }
}
