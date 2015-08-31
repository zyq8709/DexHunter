/*
 * Copyright (C) 2007 The Android Open Source Project
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
 * Make sure private methods don't inherit.
 */
public class Main {
    public static void main(String args[]) {
        PrivatePackage inst1 = new PrivatePackage();
        PrivatePackage inst2 = new PrivatePackageSub();
        PrivatePackageSub inst3 = new PrivatePackageSub();

        System.out.println("PrivatePackage --> " + inst1.getStr());
        System.out.println("PrivatePackage --> " + inst2.getStr());
        System.out.println("PrivatePackage --> " + inst3.getStr());
        System.out.println("PrivatePackageSub --> " + inst3.getStrSub());

        inst1.stretchTest();
    }
}

class PrivatePackage {
    public String getStr() {
        return privGetStr();
    }

    private String privGetStr() {
        return "PrivatePackage!";
    }

    public void stretchTest() {
        PrivatePackage inst = new PrivatePackageSub();
        System.out.println("PrivatePackage --> " + inst.getStr());
        System.out.println("PrivatePackage --> " + inst.privGetStr());
    }
}

class PrivatePackageSub extends PrivatePackage {
    public String getStrSub() {
        return privGetStr();
    }

    private String privGetStr() {
        return "PrivatePackageSub!";
    }
}
