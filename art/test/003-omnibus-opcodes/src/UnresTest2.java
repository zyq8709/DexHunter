/*
 * Test failure to resolve classes.
 */
class UnresTest2 {
    /*
     * Try check-cast and instance-of.
     */
    static boolean checkCasts(Object obj) {
        boolean foo = false;

        try {
            UnresClass un = (UnresClass) obj;
            Main.assertTrue(false);
        } catch (NoClassDefFoundError ncdfe) {
            // good
        }
        try {
            foo = obj instanceof UnresClass;
            Main.assertTrue(false);
        } catch (NoClassDefFoundError ncdfe) {
            // good
        }

        return foo;
    }

    public static void run() {
        System.out.println("UnresTest2...");
        UnresClass un;
        UnresStuff stuff = new UnresStuff();

        try {
            un = new UnresClass();
            Main.assertTrue(false);
        } catch (NoClassDefFoundError ncdfe) {
            Main.assertTrue(ncdfe.getCause() instanceof ClassNotFoundException);
            // good
        }

        try {
            new UnresClassSubclass();
            Main.assertTrue(false);
        } catch (NoClassDefFoundError ncdfe) {
            Main.assertTrue(ncdfe.getCause() instanceof ClassNotFoundException);
            // good
        }

        try {
            UnresClass[] uar = new UnresClass[3];
            Main.assertTrue(false);
        } catch (NoClassDefFoundError ncdfe) {
            Main.assertTrue(ncdfe.getCause() instanceof ClassNotFoundException);
            // good
        }

        checkCasts(stuff);
        System.out.println("UnresTest2 done");
    }
}
