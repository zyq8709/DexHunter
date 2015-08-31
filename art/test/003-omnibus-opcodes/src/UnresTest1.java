/*
 * Test failure to resolve class members.
 */
class UnresTest1 {
    public static void run() {
        System.out.println("UnresTest1...");

        UnresStuff stuff = new UnresStuff();
        try {
            int x = stuff.instField;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }
        try {       // hit the same one a second time
            int x = stuff.instField;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }
        try {
            stuff.instField = 5;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }

        try {
            double d = stuff.wideInstField;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }
        try {
            stuff.wideInstField = 0.0;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }

        try {
            int y = UnresStuff.staticField;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }
        try {
            UnresStuff.staticField = 17;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }

        try {
            double d = UnresStuff.wideStaticField;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }
        try {
            UnresStuff.wideStaticField = 1.0;
            Main.assertTrue(false);
        } catch (NoSuchFieldError nsfe) {
            // good
        }

        try {
            stuff.virtualMethod();
            Main.assertTrue(false);
        } catch (NoSuchMethodError nsfe) {
            // good
        }
        try {
            UnresStuff.staticMethod();
            Main.assertTrue(false);
        } catch (NoSuchMethodError nsfe) {
            // good
        }
    }
}
