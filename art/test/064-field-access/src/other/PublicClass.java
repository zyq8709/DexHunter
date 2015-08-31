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

package other;

// Class that makes the ProtectedClass sub-classable by classes outside of package other.
public class PublicClass extends ProtectedClass {
    public boolean otherPublicClassPublicBooleanInstanceField = true;
    public byte otherPublicClassPublicByteInstanceField = -2;
    public char otherPublicClassPublicCharInstanceField = (char)-3;
    public short otherPublicClassPublicShortInstanceField = -4;
    public int otherPublicClassPublicIntInstanceField = -5;
    public long otherPublicClassPublicLongInstanceField = -6;
    public float otherPublicClassPublicFloatInstanceField = -7.0f;
    public double otherPublicClassPublicDoubleInstanceField = -8.0;
    public Object otherPublicClassPublicObjectInstanceField = "-9";

    protected boolean otherPublicClassProtectedBooleanInstanceField = true;
    protected byte otherPublicClassProtectedByteInstanceField = -10;
    protected char otherPublicClassProtectedCharInstanceField = (char)-11;
    protected short otherPublicClassProtectedShortInstanceField = -12;
    protected int otherPublicClassProtectedIntInstanceField = -13;
    protected long otherPublicClassProtectedLongInstanceField = -14;
    protected float otherPublicClassProtectedFloatInstanceField = -15.0f;
    protected double otherPublicClassProtectedDoubleInstanceField = -16.0;
    protected Object otherPublicClassProtectedObjectInstanceField = "-17";

    private boolean otherPublicClassPrivateBooleanInstanceField = true;
    private byte otherPublicClassPrivateByteInstanceField = -18;
    private char otherPublicClassPrivateCharInstanceField = (char)-19;
    private short otherPublicClassPrivateShortInstanceField = -20;
    private int otherPublicClassPrivateIntInstanceField = -21;
    private long otherPublicClassPrivateLongInstanceField = -22;
    private float otherPublicClassPrivateFloatInstanceField = -23.0f;
    private double otherPublicClassPrivateDoubleInstanceField = -24.0;
    private Object otherPublicClassPrivateObjectInstanceField = "-25";

 /* package */ boolean otherPublicClassPackageBooleanInstanceField = true;
 /* package */ byte otherPublicClassPackageByteInstanceField = -26;
 /* package */ char otherPublicClassPackageCharInstanceField = (char)-27;
 /* package */ short otherPublicClassPackageShortInstanceField = -28;
 /* package */ int otherPublicClassPackageIntInstanceField = -29;
 /* package */ long otherPublicClassPackageLongInstanceField = -30;
 /* package */ float otherPublicClassPackageFloatInstanceField = -31.0f;
 /* package */ double otherPublicClassPackageDoubleInstanceField = -32.0;
 /* package */ Object otherPublicClassPackageObjectInstanceField = "-33";

    public static boolean otherPublicClassPublicBooleanStaticField = true;
    public static byte otherPublicClassPublicByteStaticField = -34;
    public static char otherPublicClassPublicCharStaticField = (char)-35;
    public static short otherPublicClassPublicShortStaticField = -36;
    public static int otherPublicClassPublicIntStaticField = -37;
    public static long otherPublicClassPublicLongStaticField = -38;
    public static float otherPublicClassPublicFloatStaticField = -39.0f;
    public static double otherPublicClassPublicDoubleStaticField = -40.0;
    public static Object otherPublicClassPublicObjectStaticField = "-41";

    protected static boolean otherPublicClassProtectedBooleanStaticField = true;
    protected static byte otherPublicClassProtectedByteStaticField = -42;
    protected static char otherPublicClassProtectedCharStaticField = (char)-43;
    protected static short otherPublicClassProtectedShortStaticField = -44;
    protected static int otherPublicClassProtectedIntStaticField = -45;
    protected static long otherPublicClassProtectedLongStaticField = -46;
    protected static float otherPublicClassProtectedFloatStaticField = -47.0f;
    protected static double otherPublicClassProtectedDoubleStaticField = -48.0;
    protected static Object otherPublicClassProtectedObjectStaticField = "-49";

    private static boolean otherPublicClassPrivateBooleanStaticField = true;
    private static byte otherPublicClassPrivateByteStaticField = -50;
    private static char otherPublicClassPrivateCharStaticField = (char)-51;
    private static short otherPublicClassPrivateShortStaticField = -52;
    private static int otherPublicClassPrivateIntStaticField = -53;
    private static long otherPublicClassPrivateLongStaticField = -54;
    private static float otherPublicClassPrivateFloatStaticField = -55.0f;
    private static double otherPublicClassPrivateDoubleStaticField = -56.0;
    private static Object otherPublicClassPrivateObjectStaticField = "-57";

 /* package */ static boolean otherPublicClassPackageBooleanStaticField = true;
 /* package */ static byte otherPublicClassPackageByteStaticField = -58;
 /* package */ static char otherPublicClassPackageCharStaticField = (char)-59;
 /* package */ static short otherPublicClassPackageShortStaticField = -60;
 /* package */ static int otherPublicClassPackageIntStaticField = -61;
 /* package */ static long otherPublicClassPackageLongStaticField = -62;
 /* package */ static float otherPublicClassPackageFloatStaticField = -63.0f;
 /* package */ static double otherPublicClassPackageDoubleStaticField = -64.0;
 /* package */ static Object otherPublicClassPackageObjectStaticField = "-65";
}
