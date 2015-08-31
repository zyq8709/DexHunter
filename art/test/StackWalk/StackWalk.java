public class StackWalk {
  public StackWalk() {
  }

  int f() {
    g(1);
    g(2);
    return 0;
  }

  void g(int num_calls) {
    if (num_calls == 1) {
      System.out.println("1st call");
    } else if (num_calls == 2) {
      System.out.println("2nd call");
    }
    System.out.println(shlemiel());
  }

  String shlemiel() {
    String s0 = new String("0");
    String s1 = new String("1");
    String s2 = new String("2");
    String s3 = new String("3");
    String s4 = new String("4");
    String s5 = new String("5");
    String s6 = new String("6");
    String s7 = new String("7");
    String s8 = new String("8");
    String s9 = new String("9");
    String s10 = new String("10");
    String s11 = new String("11");
    String s12 = new String("12");
    String s13 = new String("13");
    String s14 = new String("14");
    String s15 = new String("15");
    String s16 = new String("16");
    String s17 = new String("17");
    String s18 = new String("18");
    String s19 = new String("19");
    String s20 = new String("20");
    String s = new String();
    s += s0;
    s += s1;
    s += s2;
    s += s3;
    s += s4;
    s += s5;
    s += s6;
    s += s7;
    s += s8;
    s += s9;
    s += s10;
    s += s11;
    s += s12;
    s += s13;
    s += s14;
    s += s15;
    s += s16;
    s += s17;
    s += s18;
    s += s19;
    s += s20;

    s += s6;
    s += s5;
    s += s2;
    s += s3;

    s10 = s + s10;
    s10 += s20;

    s20 += s10;
    s = s17 + s20;

    s4 = s18 = s19;
    s += s4;
    s += s18;
    refmap(0);
    return s;
  }

  native int refmap(int x);

  static {
    System.loadLibrary("arttest");
  }

  public static void main(String[] args) {
    StackWalk st = new StackWalk();
    st.f();
  }
}
