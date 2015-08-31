public class Switch {
    /**
     * Test switch() blocks
     */
    private static void testSwitch() {
        System.out.println("Switch.testSwitch");

        int a = 1;

        switch (a) {
            case -1: Main.assertTrue(false); break;
            case 0: Main.assertTrue(false); break;
            case 1: /*correct*/ break;
            case 2: Main.assertTrue(false); break;
            case 3: Main.assertTrue(false); break;
            case 4: Main.assertTrue(false); break;
            default: Main.assertTrue(false); break;
        }
        switch (a) {
            case 3: Main.assertTrue(false); break;
            case 4: Main.assertTrue(false); break;
            default: /*correct*/ break;
        }

        a = 0x12345678;

        switch (a) {
            case 0x12345678: /*correct*/ break;
            case 0x12345679: Main.assertTrue(false); break;
            default: Main.assertTrue(false); break;
        }
        switch (a) {
            case 57: Main.assertTrue(false); break;
            case -6: Main.assertTrue(false); break;
            case 0x12345678: /*correct*/ break;
            case 22: Main.assertTrue(false); break;
            case 3: Main.assertTrue(false); break;
            default: Main.assertTrue(false); break;
        }
        switch (a) {
            case -6: Main.assertTrue(false); break;
            case 3: Main.assertTrue(false); break;
            default: /*correct*/ break;
        }

        a = -5;
        switch (a) {
            case 12: Main.assertTrue(false); break;
            case -5: /*correct*/ break;
            case 0: Main.assertTrue(false); break;
            default: Main.assertTrue(false); break;
        }

        switch (a) {
            default: /*correct*/ break;
        }
    }

    public static void run() {
        testSwitch();
    }
}
