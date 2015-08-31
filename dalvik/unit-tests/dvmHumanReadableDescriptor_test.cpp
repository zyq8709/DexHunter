#include <gtest/gtest.h>

#include "Dalvik.h"

TEST(dvmHumanReadableDescriptor, ArrayReferences) {
  ASSERT_EQ("java.lang.Class[]", dvmHumanReadableDescriptor("[Ljava/lang/Class;"));
  ASSERT_EQ("java.lang.Class[][]", dvmHumanReadableDescriptor("[[Ljava/lang/Class;"));
}

TEST(dvmHumanReadableDescriptor, ScalarReferences) {
  ASSERT_EQ("java.lang.String", dvmHumanReadableDescriptor("Ljava.lang.String;"));
  ASSERT_EQ("java.lang.String", dvmHumanReadableDescriptor("Ljava/lang/String;"));
}

TEST(dvmHumanReadableDescriptor, PrimitiveArrays) {
  ASSERT_EQ("boolean[]", dvmHumanReadableDescriptor("[Z"));
  ASSERT_EQ("boolean[][]", dvmHumanReadableDescriptor("[[Z"));
  ASSERT_EQ("byte[]", dvmHumanReadableDescriptor("[B"));
  ASSERT_EQ("byte[][]", dvmHumanReadableDescriptor("[[B"));
  ASSERT_EQ("char[]", dvmHumanReadableDescriptor("[C"));
  ASSERT_EQ("char[][]", dvmHumanReadableDescriptor("[[C"));
  ASSERT_EQ("double[]", dvmHumanReadableDescriptor("[D"));
  ASSERT_EQ("double[][]", dvmHumanReadableDescriptor("[[D"));
  ASSERT_EQ("float[]", dvmHumanReadableDescriptor("[F"));
  ASSERT_EQ("float[][]", dvmHumanReadableDescriptor("[[F"));
  ASSERT_EQ("int[]", dvmHumanReadableDescriptor("[I"));
  ASSERT_EQ("int[][]", dvmHumanReadableDescriptor("[[I"));
  ASSERT_EQ("long[]", dvmHumanReadableDescriptor("[J"));
  ASSERT_EQ("long[][]", dvmHumanReadableDescriptor("[[J"));
  ASSERT_EQ("short[]", dvmHumanReadableDescriptor("[S"));
  ASSERT_EQ("short[][]", dvmHumanReadableDescriptor("[[S"));
}

TEST(dvmHumanReadableDescriptor, PrimitiveScalars) {
  ASSERT_EQ("boolean", dvmHumanReadableDescriptor("Z"));
  ASSERT_EQ("byte", dvmHumanReadableDescriptor("B"));
  ASSERT_EQ("char", dvmHumanReadableDescriptor("C"));
  ASSERT_EQ("double", dvmHumanReadableDescriptor("D"));
  ASSERT_EQ("float", dvmHumanReadableDescriptor("F"));
  ASSERT_EQ("int", dvmHumanReadableDescriptor("I"));
  ASSERT_EQ("long", dvmHumanReadableDescriptor("J"));
  ASSERT_EQ("short", dvmHumanReadableDescriptor("S"));
}
