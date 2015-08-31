import otherpackage.OtherPackageClass;

import java.io.Serializable;
import java.lang.reflect.AccessibleObject;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Type;
import java.lang.reflect.TypeVariable;

public class ClassAttrs {
    ClassAttrs() {
        /* local, not anonymous, not member */
        class ConsInnerNamed {
            public void showMe() {
                printClassAttrs(this.getClass());
            }
        }

        ConsInnerNamed cinner = new ConsInnerNamed();
        cinner.showMe();
    }

    public class PublicInnerClass {
    }

    protected class ProtectedInnerClass {
    }

    private class PrivateInnerClass {
    }

    class PackagePrivateInnerClass {
    }

    public interface PublicInnerInterface {
    }

    protected interface ProtectedInnerInterface {
    }

    private interface PrivateInnerInterface {
    }

    interface PackagePrivateInnerInterface {
    }

    private static void showModifiers(Class<?> c) {
        System.out.println(Modifier.toString(c.getModifiers()) + " " + c.getName());
    }

    // https://code.google.com/p/android/issues/detail?id=56267
    private static void test56267() {
        // Primitive classes.
        showModifiers(int.class);
        showModifiers(int[].class);

        // Regular classes.
        showModifiers(Object.class);
        showModifiers(Object[].class);

        // Inner classes.
        showModifiers(PublicInnerClass.class);
        showModifiers(PublicInnerClass[].class);
        showModifiers(ProtectedInnerClass.class);
        showModifiers(ProtectedInnerClass[].class);
        showModifiers(PrivateInnerClass.class);
        showModifiers(PrivateInnerClass[].class);
        showModifiers(PackagePrivateInnerClass.class);
        showModifiers(PackagePrivateInnerClass[].class);

        // Regular interfaces.
        showModifiers(Serializable.class);
        showModifiers(Serializable[].class);

        // Inner interfaces.
        showModifiers(PublicInnerInterface.class);
        showModifiers(PublicInnerInterface[].class);
        showModifiers(ProtectedInnerInterface.class);
        showModifiers(ProtectedInnerInterface[].class);
        showModifiers(PrivateInnerInterface.class);
        showModifiers(PrivateInnerInterface[].class);
        showModifiers(PackagePrivateInnerInterface.class);
        showModifiers(PackagePrivateInnerInterface[].class);
    }

    public static void main() {
        test56267();

        printClassAttrs(ClassAttrs.class);
        printClassAttrs(OtherClass.class);
        printClassAttrs(OtherPackageClass.class);

        /* local, not anonymous, not member */
        class InnerNamed {
            public void showMe() {
                printClassAttrs(this.getClass());
            }
        }
        InnerNamed inner = new InnerNamed();
        inner.showMe();

        ClassAttrs attrs = new ClassAttrs();

        /* anonymous, not local, not member */
        printClassAttrs((new OtherClass() { int i = 5; }).getClass());

        /* member, not anonymous, not local */
        printClassAttrs(MemberClass.class);

        /* fancy */
        printClassAttrs(FancyClass.class);

        try {
            Constructor cons;
            cons = MemberClass.class.getConstructor(
                    new Class[] { MemberClass.class });
            System.out.println("constructor signature: "
                    + getSignatureAttribute(cons));

            Method meth;
            meth = MemberClass.class.getMethod("foo", (Class[]) null);
            System.out.println("method signature: "
                    + getSignatureAttribute(meth));

            Field field;
            field = MemberClass.class.getField("mWha");
            System.out.println("field signature: "
                    + getSignatureAttribute(field));
        } catch (NoSuchMethodException nsme) {
            System.err.println("FAILED: " + nsme);
        } catch (NoSuchFieldException nsfe) {
            System.err.println("FAILED: " + nsfe);
        } catch (RuntimeException re) {
            System.err.println("FAILED: " + re);
            re.printStackTrace();
        }

        test_isAssignableFrom();
        test_isInstance();
    }

    private static void test_isAssignableFrom() {
        // Can always assign to things of the same type.
        assertTrue(String.class.isAssignableFrom(String.class));

        // Can assign any reference to java.lang.Object.
        assertTrue(Object.class.isAssignableFrom(Object.class));
        assertTrue(Object.class.isAssignableFrom(Class.class));
        assertTrue(Object.class.isAssignableFrom(String.class));
        assertFalse(Object.class.isAssignableFrom(int.class));
        assertFalse(Object.class.isAssignableFrom(long.class));

        // Interfaces.
        assertTrue(CharSequence.class.isAssignableFrom(String.class));
        assertFalse(CharSequence.class.isAssignableFrom(Object.class));

        // Superclasses.
        assertTrue(AccessibleObject.class.isAssignableFrom(Method.class));
        assertFalse(Method.class.isAssignableFrom(AccessibleObject.class));

        // Arrays.
        assertTrue(int[].class.isAssignableFrom(int[].class));
        assertFalse(int[].class.isAssignableFrom(char[].class));
        assertFalse(char[].class.isAssignableFrom(int[].class));
        assertTrue(Object.class.isAssignableFrom(int[].class));
        assertFalse(int[].class.isAssignableFrom(Object.class));

        try {
            assertFalse(Object.class.isAssignableFrom(null));
            fail();
        } catch (NullPointerException expected) {
        }
    }

    private static void test_isInstance() {
        // Can always assign to things of the same type.
        assertTrue(String.class.isInstance("hello"));

        // Can assign any reference to java.lang.Object.
        assertTrue(Object.class.isInstance(new Object()));
        assertTrue(Object.class.isInstance(Class.class));
        assertTrue(Object.class.isInstance("hello"));

        // Interfaces.
        assertTrue(CharSequence.class.isInstance("hello"));
        assertFalse(CharSequence.class.isInstance(new Object()));

        // Superclasses.
        assertTrue(AccessibleObject.class.isInstance(Method.class.getDeclaredMethods()[0]));
        assertFalse(Method.class.isInstance(Method.class.getDeclaredFields()[0]));

        // Arrays.
        assertTrue(int[].class.isInstance(new int[0]));
        assertFalse(int[].class.isInstance(new char[0]));
        assertFalse(char[].class.isInstance(new int[0]));
        assertTrue(Object.class.isInstance(new int[0]));
        assertFalse(int[].class.isInstance(new Object()));

        assertFalse(Object.class.isInstance(null));
    }

    private static void assertTrue(boolean b) {
        if (!b) throw new RuntimeException();
    }

    private static void assertFalse(boolean b) {
        if (b) throw new RuntimeException();
    }

    private static void fail() {
        throw new RuntimeException();
    }

    /* to call the (out-of-scope) <code>getSignatureAttribute</code> methods */
    public static String getSignatureAttribute(Object obj) {
        Method method;
        try {
            Class c = Class.forName("libcore.reflect.AnnotationAccess");
            method = c.getDeclaredMethod("getSignature", java.lang.reflect.AnnotatedElement.class);
            method.setAccessible(true);
        } catch (Exception ex) {
            ex.printStackTrace();
            return "<unknown>";
        }

        try {
            return (String) method.invoke(null, obj);
        } catch (IllegalAccessException ex) {
            throw new RuntimeException(ex);
        } catch (InvocationTargetException ex) {
            throw new RuntimeException(ex);
        }
    }

    /* for reflection testing */
    static class MemberClass<XYZ> {
        public MemberClass<XYZ> mWha;

        public MemberClass(MemberClass<XYZ> memb) {
            mWha = memb;
        }

        public Class<XYZ> foo() throws NoSuchMethodException {
            return null;
        }
    }

    /* for reflection testing (getClasses vs getDeclaredClasses) */
    static public class PublicMemberClass {
        float mBlah;
    }

    /*
     * Dump a variety of class attributes.
     */
    public static void printClassAttrs(Class clazz) {
        Class clazz2;

        System.out.println("***** " + clazz + ":");

        System.out.println("  name: "
            + clazz.getName());
        System.out.println("  canonical: "
            + clazz.getCanonicalName());
        System.out.println("  simple: "
            + clazz.getSimpleName());
        System.out.println("  genericSignature: "
            + getSignatureAttribute(clazz));

        System.out.println("  super: "
            + clazz.getSuperclass());
        System.out.println("  genericSuperclass: "
            + clazz.getGenericSuperclass());
        System.out.println("  declaring: "
            + clazz.getDeclaringClass());
        System.out.println("  enclosing: "
            + clazz.getEnclosingClass());
        System.out.println("  enclosingCon: "
            + clazz.getEnclosingConstructor());
        System.out.println("  enclosingMeth: "
            + clazz.getEnclosingMethod());
        System.out.println("  modifiers: "
            + clazz.getModifiers());
        System.out.println("  package: "
            + clazz.getPackage());

        System.out.println("  declaredClasses: "
            + stringifyTypeArray(clazz.getDeclaredClasses()));
        System.out.println("  member classes: "
            + stringifyTypeArray(clazz.getClasses()));

        System.out.println("  isAnnotation: "
            + clazz.isAnnotation());
        System.out.println("  isAnonymous: "
            + clazz.isAnonymousClass());
        System.out.println("  isArray: "
            + clazz.isArray());
        System.out.println("  isEnum: "
            + clazz.isEnum());
        System.out.println("  isInterface: "
            + clazz.isInterface());
        System.out.println("  isLocalClass: "
            + clazz.isLocalClass());
        System.out.println("  isMemberClass: "
            + clazz.isMemberClass());
        System.out.println("  isPrimitive: "
            + clazz.isPrimitive());
        System.out.println("  isSynthetic: "
            + clazz.isSynthetic());

        System.out.println("  genericInterfaces: "
            + stringifyTypeArray(clazz.getGenericInterfaces()));

        TypeVariable<Class<?>>[] typeParameters = clazz.getTypeParameters();
        System.out.println("  typeParameters: "
            + stringifyTypeArray(typeParameters));
    }

    /*
     * Convert an array of Type into a string.  Start with an array count.
     */
    private static String stringifyTypeArray(Type[] types) {
        StringBuilder stb = new StringBuilder();
        boolean first = true;

        stb.append("[" + types.length + "]");

        for (Type t: types) {
            if (first) {
                stb.append(" ");
                first = false;
            } else {
                stb.append(", ");
            }
            stb.append(t.toString());
        }

        return stb.toString();
    }
}
