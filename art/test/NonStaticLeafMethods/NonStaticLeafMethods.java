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

class NonStaticLeafMethods {
    NonStaticLeafMethods() {
    }
    void nop() {
    }
    byte identity(byte x) {
        return x;
    }
    int identity(int x) {
        return x;
    }
    int sum(int a, int b) {
        return a + b;
    }
    int sum(int a, int b, int c) {
        return a + b + c;
    }
    int sum(int a, int b, int c, int d) {
        return a + b + c + d;
    }
    int sum(int a, int b, int c, int d, int e) {
        return a + b + c + d + e;
    }
    double identity(double x) {
        return x;
    }
    double sum(double a, double b) {
        return a + b;
    }
    double sum(double a, double b, double c) {
        return a + b + c;
    }
    double sum(double a, double b, double c, double d) {
        return a + b + c + d;
    }
    double sum(double a, double b, double c, double d, double e) {
        return a + b + c + d + e;
    }
}
