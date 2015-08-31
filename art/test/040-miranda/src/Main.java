/*
 * Copyright (C) 2006 The Android Open Source Project
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

import java.lang.reflect.Method;

/**
 * Miranda testing.
 */
public class Main {
    public static void main(String[] args) {
        MirandaClass mir = new MirandaClass();
        System.out.println("MirandaClass:");
        System.out.println("  inInterface:  " + mir.inInterface());
        System.out.println("  inInterface2: " + mir.inInterface2());
        System.out.println("  inAbstract:   " + mir.inAbstract());

        /* try again through abstract class; results should be identical */
        MirandaAbstract mira = mir;
        System.out.println("MirandaAbstract / MirandaClass:");
        System.out.println("  inInterface:  " + mira.inInterface());
        System.out.println("  inInterface2: " + mira.inInterface2());
        System.out.println("  inAbstract:   " + mira.inAbstract());

        MirandaAbstract mira2 = new MirandaClass2();
        System.out.println("MirandaAbstract / MirandaClass2:");
        System.out.println("  inInterface:  " + mira2.inInterface());
        System.out.println("  inInterface2: " + mira2.inInterface2());
        System.out.println("  inAbstract:   " + mira2.inAbstract());

        System.out.println("Test getting miranda method via reflection:");
        try {
          Class mirandaClass = Class.forName("MirandaAbstract");
          Method mirandaMethod = mirandaClass.getDeclaredMethod("inInterface", (Class[]) null);
          System.out.println("  did not expect to find miranda method");
        } catch (NoSuchMethodException nsme) {
          System.out.println("  caught expected NoSuchMethodException");
        } catch (Exception e) {
          System.out.println("  caught unexpected exception " + e);
        }
    }
}
