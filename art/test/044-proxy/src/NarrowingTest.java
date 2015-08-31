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
import java.util.Arrays;

class NarrowingTest {

   interface I1 {
       public Object foo();
   }

   interface I2 extends I1 {
       // Note that this method declaration narrows the return type.
       @Override
       public String foo();
   }

   public static void main(String[] args) {
       I2 proxy = (I2) Proxy.newProxyInstance(NarrowingTest.class.getClassLoader(),
                                              new Class<?>[] { I2.class },
               new InvocationHandler() {
                   int count = 0;
                   @Override
                   public Object invoke(Object proxy, Method method,
                                        Object[] args) throws Throwable {
                       System.out.println("Invocation of " + method);
                       if (count == 0) {
                           count++;
                           return "hello";
                       } else {
                           return Integer.valueOf(1);
                       }
                   }
               });

       Method[] methods = proxy.getClass().getDeclaredMethods();
       System.out.println("Proxy methods: " + Arrays.deepToString(methods));

       System.out.println("Invoking foo using I2 type: " + proxy.foo());

       I1 proxyAsParent = proxy;
       System.out.println("Invoking foo using I1 type: " + proxyAsParent.foo());

       try {
           proxy.foo();
           System.out.println("Didn't get expected exception");
       } catch (ClassCastException e) {
           // With an I2 invocation returning an integer is an exception.
           System.out.println("Got expected exception");
       }

       System.out.println("Proxy narrowed invocation return type passed");
   }
}
